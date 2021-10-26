// Copyright (c) 2016-2021 Andrew Helge Cox
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Internal includes
#include "krust/public-api/vulkan-objects.h"
#include "krust/public-api/vulkan_struct_init.h"
#include "krust/public-api/thread-base.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/krust-errors.h"
#include "krust/internal/keep-alive-set.h"
#include "krust/internal/krust-internal.h"
#include "krust/internal/scoped-temp-array.h"


namespace Krust
{

/// Max size for a temporary buffer on the stack (over this and we do a temp heap alloc).
constexpr unsigned MAX_STACK_BUFFER_BYTES = 4096U;


// -----------------------------------------------------------------------------
CommandBuffer::CommandBuffer(CommandPool& pool, const VkCommandBufferLevel level) :
  mPool(&pool)
{
  auto info = CommandBufferAllocateInfo(pool, level, 1u);
  const VkResult result = vkAllocateCommandBuffers(pool.GetDevice(), &info, &mCommandBuffer);
  if (result != VK_SUCCESS)
  {
    mCommandBuffer = VK_NULL_HANDLE;
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkAllocateCommandBuffers", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

CommandBuffer::CommandBuffer(CommandPool& pool, VkCommandBuffer vkBuf) : mPool(&pool), mCommandBuffer(vkBuf) {}

CommandBufferPtr CommandBuffer::New(CommandPool & pool, VkCommandBufferLevel level)
{
  return CommandBufferPtr{ new CommandBuffer{pool, level} };
}

void CommandBuffer::Allocate(CommandPool & pool, VkCommandBufferLevel level, unsigned number, std::vector<CommandBufferPtr>& outCommandBuffers)
{
  ScopedTempArray<VkCommandBuffer, MAX_STACK_BUFFER_BYTES>  buffers(number);
  outCommandBuffers.clear();
  outCommandBuffers.reserve(number); // Do early to throw if going to throw.
  auto info = CommandBufferAllocateInfo(pool, level, number);
  const VkResult result = vkAllocateCommandBuffers(pool.GetDevice(), &info, buffers.Get());
  if (result != VK_SUCCESS)
  {
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkAllocateCommandBuffers", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
  try
  {
    std::for_each(buffers.Get(), buffers.Get() + number, [&pool, &outCommandBuffers](auto rawCommandBuffer) {
      outCommandBuffers.push_back(CommandBufferPtr(new CommandBuffer(pool, rawCommandBuffer)));
    });
  }
  catch (...)
  {
    vkFreeCommandBuffers(pool.GetDevice(), pool, number, buffers.Get());
    throw;
  }


}

CommandBuffer::~CommandBuffer()
{
  vkFreeCommandBuffers(mPool->GetDevice(), *mPool, 1u, &mCommandBuffer);
  delete reinterpret_cast<KeepAliveSet*>(mKeepAlives);
}

void CommandBuffer::KeepAlive(VulkanObject & needed)
{
  if (!mKeepAlives)
  {
    mKeepAlives = new KeepAliveSet{};
  }
  reinterpret_cast<KeepAliveSet*>(mKeepAlives)->Add(needed);
}



// -----------------------------------------------------------------------------
CommandPool::CommandPool(Device & device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex) :
  mDevice(device)
{
  auto poolInfo = CommandPoolCreateInfo(flags, queueFamilyIndex);
  const VkResult result = vkCreateCommandPool(device, &poolInfo, Internal::sAllocator, &mCommandPool);
  if (result != VK_SUCCESS)
  {
    mCommandPool = VK_NULL_HANDLE;
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkCreateCommandPool", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }

}

CommandPoolPtr CommandPool::New(Device & device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex)
{//DescriptorSet::DescriptorSet(Device& device, const VkDescriptorSetAllocateInfo& allocateInfo)
  return new CommandPool { device, flags, queueFamilyIndex };
}

CommandPool::~CommandPool()
{
  vkDestroyCommandPool(*mDevice, mCommandPool, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
ComputePipeline::ComputePipeline(Device& device, const VkComputePipelineCreateInfo& createInfo) :
  mDevice(device)
{
  const VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, Internal::sAllocator, &mPipeline);
  if (result != VK_SUCCESS)
  {
    mPipeline = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreateComputePipelines", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

ComputePipelinePtr ComputePipeline::New(Device& device, const VkComputePipelineCreateInfo& createInfo)
{
  return new ComputePipeline { device, createInfo };
}

ComputePipeline::~ComputePipeline()
{
  vkDestroyPipeline(*mDevice, mPipeline, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
DescriptorPool::DescriptorPool(Device& device, const VkDescriptorPoolCreateInfo& createInfo) :
  mDevice(device)
{
  VkDescriptorPoolCreateInfo ci = createInfo;
  ci.flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  const VkResult result = vkCreateDescriptorPool(device, &ci, Internal::sAllocator, &mDescriptorPool);
  if (result != VK_SUCCESS)
  {
    mDescriptorPool = VK_NULL_HANDLE;
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkCreateDescriptorPool", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

DescriptorPoolPtr DescriptorPool::New(Device& device, VkDescriptorPoolCreateFlags flags, uint32_t maxSets, uint32_t poolSizeCount, const VkDescriptorPoolSize* pPoolSizes)
{
  return new DescriptorPool { device, DescriptorPoolCreateInfo(flags, maxSets, poolSizeCount, pPoolSizes)};
}

DescriptorPool::~DescriptorPool()
{
  vkDestroyDescriptorPool(*mDevice, mDescriptorPool, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
DescriptorSet::DescriptorSet(DescriptorPool& pool, const VkDescriptorSetAllocateInfo& allocateInfo) :
  mPool(pool)
{
  Device& device = pool.GetDevice();

  const VkResult result = vkAllocateDescriptorSets(device, &allocateInfo, &mDescriptorSet);
  if (result != VK_SUCCESS)
  {
    mDescriptorSet = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkAllocateDescriptorSets", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

DescriptorSetPtr DescriptorSet::Allocate(
  DescriptorPool& pool,
  DescriptorSetLayout& setLayout)
{
  return new DescriptorSet {pool, DescriptorSetAllocateInfo(pool, 1, setLayout.GetDescriptorSetLayoutAddress())};

}

DescriptorSet::~DescriptorSet()
{
  /// @todo Delay freeing until all command buffers referencing the set are free
  /// and all work on GPUs using them is complete.
  /// 1. Command buffers must hold a reference to them.
  /// 2. Queues keep them alive transitively by keeping command buffers alive.
  vkFreeDescriptorSets(mPool->GetDevice(), *mPool, 1, &mDescriptorSet);
}



// -----------------------------------------------------------------------------
DescriptorSetLayout::DescriptorSetLayout(Device& device, const VkDescriptorSetLayoutCreateInfo& createInfo) :
  mDevice(&device)
{
  const VkResult result = vkCreateDescriptorSetLayout(device, &createInfo, Internal::sAllocator, &mDescriptorSetLayout);
  if (result != VK_SUCCESS)
  {
    mDescriptorSetLayout = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreateDescriptorSetLayout", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

DescriptorSetLayoutPtr DescriptorSetLayout::New(
  Device& device,
  VkDescriptorSetLayoutCreateFlags flags,
  uint32_t bindingCount,
  const VkDescriptorSetLayoutBinding* pBindings)
{
    return new DescriptorSetLayout {device, DescriptorSetLayoutCreateInfo(flags, bindingCount, pBindings)};
}

DescriptorSetLayout::~DescriptorSetLayout()
{
  vkDestroyDescriptorSetLayout(*mDevice, mDescriptorSetLayout, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
Device::Device(Instance & instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo & createInfo) :
  mInstance(instance)
{
  KRUST_ASSERT1(instance != VK_NULL_HANDLE, "Invalid instance.");
  KRUST_ASSERT1(physicalDevice != VK_NULL_HANDLE, "Invalid physical device.");

  mPhysicalDevice = physicalDevice;
  auto & threadBase = ThreadBase::Get();
  const VkResult result = vkCreateDevice(physicalDevice, &createInfo, Internal::sAllocator, &mDevice);
  if (result != VK_SUCCESS)
  {
    mDevice = VK_NULL_HANDLE;
    threadBase.GetErrorPolicy().VulkanError("vkCreateDevice", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

DevicePtr Device::New(Instance & instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo & createInfo)
{
  return new Device{ instance, physicalDevice, createInfo };
}

Device::~Device()
{
  vkDestroyDevice(mDevice, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
DeviceMemory::DeviceMemory(Device & device, const VkMemoryAllocateInfo & info) :
  mDevice(&device)
{
  const VkResult result = vkAllocateMemory(device, &info, Internal::sAllocator, &mDeviceMemory);
  if (result != VK_SUCCESS)
  {
    mDeviceMemory = VK_NULL_HANDLE;
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkAllocateMemory", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

DeviceMemoryPtr DeviceMemory::New(Device & device, const VkMemoryAllocateInfo & info)
{
  return new DeviceMemory { device, info };
}

DeviceMemory::~DeviceMemory()
{
  vkFreeMemory(*mDevice, mDeviceMemory, Internal::sAllocator);
}

/// @todo move to top of file <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< BOOKMARK
/// @todo Use this macro for all create functions in constructors.
#define KRUST_CALL_CREATOR(NAME) \
const VkResult result = vkCreate##NAME(device, &info, Internal::sAllocator, &m##NAME);\
  if (result != VK_SUCCESS)\
  {\
    m##NAME = VK_NULL_HANDLE;\
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreate##NAME", result, nullptr, __FUNCTION__, __FILE__, __LINE__);\
  }

#define KRUST_VKOBJ_CONSTRUCTOR(NAME) \
NAME::NAME(Device& device, const Vk##NAME##CreateInfo& info) : \
  mDevice(&device)\
{\
    KRUST_CALL_CREATOR(NAME);\
}

#define KRUST_VKOBJ_DESTRUCTOR(NAME) \
NAME::~NAME() \
{\
    vkDestroy##NAME(*mDevice, m##NAME, Internal::sAllocator);\
}

#define KRUST_VKOBJ_LIFETIME(NAME) \
KRUST_VKOBJ_CONSTRUCTOR(NAME)\
KRUST_VKOBJ_DESTRUCTOR(NAME)

// -----------------------------------------------------------------------------
// Buffer
KRUST_VKOBJ_LIFETIME(Buffer)

BufferPtr Buffer::New(Device& device, VkBufferCreateFlags flags, VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, uint32_t queueFamilyIndex)
{
  return new Buffer(device, BufferCreateInfo(flags, size, usage, sharingMode, 1, &queueFamilyIndex));
}

VkResult Buffer::BindMemory(DeviceMemory& memory, VkDeviceSize memoryOffset)
{
  const VkResult result = vkBindBufferMemory(*mDevice, mBuffer, memory, memoryOffset);
  if(result == VK_SUCCESS){
    mMemory = memory;
  }
  return result;
}



// -----------------------------------------------------------------------------
Fence::Fence(Device& device, const VkFenceCreateFlags flags) :
  mDevice(&device)
{
  const auto createInfo = FenceCreateInfo(flags);
  const VkResult result = vkCreateFence(device, &createInfo, Internal::sAllocator, &mFence);
  if (result != VK_SUCCESS)
  {
    mFence = VK_NULL_HANDLE;
    auto & threadBase = ThreadBase::Get();
    threadBase.GetErrorPolicy().VulkanError("vkCreateFence", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

FencePtr Fence::New(Device& device, const VkFenceCreateFlags flags)
{
  return new Fence {device, flags};
}

Fence::~Fence()
{
  vkDestroyFence(*mDevice, mFence, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
Image::Image(Device & device, const VkImageCreateInfo & createInfo) :
  mDevice(device)
{
  const VkResult result = vkCreateImage(device, &createInfo, Internal::sAllocator, &mImage);
  if (result != VK_SUCCESS)
  {
    mImage = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreateImage", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

ImagePtr Image::New(Device & device, const VkImageCreateInfo & createInfo)
{
  return new Image{ device, createInfo };
}

Image::~Image()
{
  vkDestroyImage(*mDevice, mImage, Internal::sAllocator);
}

void Image::BindMemory(DeviceMemory& memory, const VkDeviceSize offset)
{
  const VkResult result = vkBindImageMemory(*mDevice, mImage, memory, offset);
  if (result != VK_SUCCESS)
  {
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkBindImageMemory", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
  mMemory = DeviceMemoryPtr(&memory);
}



// -----------------------------------------------------------------------------
ImageView::ImageView(Image& image, const VkImageViewCreateInfo& createInfo) :
  mImage(&image)
{
  VkImageViewCreateInfo info { createInfo };
  info.image = image;
  mImage = image;
  const VkResult result = vkCreateImageView(image.device(), &info, Internal::sAllocator, &mImageView);
  if (result != VK_SUCCESS)
  {
    mImage.Reset(0);
    mImageView = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreateImage", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

ImageViewPtr ImageView::New(Image& image, const VkImageViewCreateInfo& createInfo)
{
  return new ImageView { image, createInfo };
}

ImageView::~ImageView()
{
  vkDestroyImageView(device(), mImageView, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
Instance::Instance(const VkInstanceCreateInfo & createInfo)
{
  auto & threadBase = ThreadBase::Get();
  const VkResult result = vkCreateInstance(&createInfo, Internal::sAllocator, &mInstance);
  if (result != VK_SUCCESS)
  {
    mInstance = VK_NULL_HANDLE;
    threadBase.GetErrorPolicy().VulkanError("vkCreateInstance", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

InstancePtr Instance::New(const VkInstanceCreateInfo & createInfo)
{
  return new Instance { createInfo };
}

Instance::~Instance()
{
  vkDestroyInstance(mInstance, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
PipelineLayout::PipelineLayout(Device& device, const VkPipelineLayoutCreateInfo& createInfo) :
  mDevice(&device)
{
  const VkResult result = vkCreatePipelineLayout(device, &createInfo, Internal::sAllocator, &mPipelineLayout);
  if (result != VK_SUCCESS)
  {
    mPipelineLayout = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreatePipelineLayout", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

PipelineLayoutPtr PipelineLayout::New(
  Device&                         device,
  VkPipelineLayoutCreateFlags     flags,
  uint32_t                        setLayoutCount,
  const VkDescriptorSetLayout*    pSetLayouts,
  uint32_t                        pushConstantRangeCount,
  const VkPushConstantRange*      pPushConstantRanges)
{
  return new PipelineLayout(device, PipelineLayoutCreateInfo(flags, setLayoutCount, pSetLayouts, pushConstantRangeCount, pPushConstantRanges));
}

PipelineLayoutPtr PipelineLayout::New(
    Device&                         device,
    VkPipelineLayoutCreateFlags     flags,
    const VkDescriptorSetLayout&    setLayout,
    const VkPushConstantRange&      pushConstantRange)
{
  return new PipelineLayout(device, PipelineLayoutCreateInfo(flags, 1, &setLayout, 1, &pushConstantRange));
}

PipelineLayout::~PipelineLayout()
{
  vkDestroyPipelineLayout(*mDevice, mPipelineLayout, Internal::sAllocator);
}



// -----------------------------------------------------------------------------
ShaderModule::ShaderModule(Device& device, const VkShaderModuleCreateFlags flags, const ShaderBuffer& src)
: mDevice(device)
{
  const auto createInfo = ShaderModuleCreateInfo(flags, byte_size(src), &src[0]);
  const VkResult result = vkCreateShaderModule(device, &createInfo, Internal::sAllocator, &mShaderModule);
  if (result != VK_SUCCESS)
  {
    mShaderModule = VK_NULL_HANDLE;
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkCreateShaderModule", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
}

ShaderModulePtr ShaderModule::New(Device& device, const VkShaderModuleCreateFlags flags, const ShaderBuffer& src)
{
  return new ShaderModule { device, flags, src };
}

ShaderModule::~ShaderModule(){
  vkDestroyShaderModule(*mDevice, mShaderModule, Internal::sAllocator);
}

}
