// Copyright (c) 2016 Andrew Helge Cox
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
{
  return new CommandPool { device, flags, queueFamilyIndex };
}

CommandPool::~CommandPool()
{
  vkDestroyCommandPool(*mDevice, mCommandPool, Internal::sAllocator);
}

CommandBuffer::CommandBuffer(CommandPool& pool, const VkCommandBufferLevel level)
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

void Image::BindMemory(DeviceMemory& memory, VkDeviceSize offset)
{
  const VkResult result = vkBindImageMemory(*mDevice, mImage, memory, offset);
  if (result != VK_SUCCESS)
  {
    ThreadBase::Get().GetErrorPolicy().VulkanError("vkBindImageMemory", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
  mMemory = DeviceMemoryPtr(&memory);
}

}
