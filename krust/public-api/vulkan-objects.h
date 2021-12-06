#ifndef KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF
#define KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF

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
#include "krust/public-api/vulkan-objects-fwd.h"
#include "krust/public-api/ref-object.h"
#include <krust/public-api/vulkan_types_and_macros.h>

// External includes:
#include <vector>

/**
* @file
*
* Krust implementation of RAII for Vulkan API objects.
*/

namespace Krust
{

/**
 * @defgroup VulkanObjectOwners Vulkan Object Owners
 *
 * Reference counted wrappers for Vulkan objects and smart pointers to them that
 * keep them alive as long as necessary and clean them up when they are no
 * longer in use.
 * A reference to an Image, for example, will keep it's associated DeviceMemory,
 * Device and Instance alive too.
 *
 * These attempt to solve the single problem of managing the lifetime of Vulkan
 * API objects in an RAII manner and thus do not wrap every Vulkan API call
 * associated with a given object in an attempt to provide an OO veneer over the
 * API. Instead, a conversion operator is provided for each which yields the
 * underlying Vulkan object handle. This lets them be passed to Vulkan API
 * functions and random helper code the user may have assembled from various
 * sources.
 *
 * Submitting command buffers to the GPU keeps all associated API objects and
 * resources alive until the GPU is finished using them, as long as the Queue
 * wrapper is used for submission.
 *
 * @note While the implementation of Queue must call `vkQueueWaitIdle` and
 * Device must call `vkDeviceWaitIdle` for their associated GPU to idle before
 * their destruction can complete, the majority of Vulkan object owners do not
 * need to block on GPU idle prior to freeing their Vulkan handles.
 */
///@{



/* ----------------------------------------------------------------------- *//**
 * Base class for all ownership wrappers for Vulkan API objects.
 **/
class VulkanObject : public RefObject
{
public:
  VulkanObject() {}
private:
  // Ban copying objects:
  VulkanObject(const VulkanObject&) = delete;
  VulkanObject& operator=(const VulkanObject&) = delete;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for a VkInstance Vulkan API object.
 */
class Instance : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  Instance(const VkInstanceCreateInfo& createInfo);
public:
  /** Creation function to return new Instances via smart pointers. */
  static InstancePtr New(const VkInstanceCreateInfo& createInfo);
  ~Instance();
  /**
   * Operator to allow the object to be used in raw Vulkan API calls and avoid
   * having to wrap those too.
   */
  operator VkInstance() const { return mInstance; }
private:
  VkInstance mInstance = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for vkDevice instances.
 */
class Device : public VulkanObject
{
  Device(Instance& instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo& createInfo);
public:
  static DevicePtr New(Instance& instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo& createInfo);
  ~Device();
  VkPhysicalDevice GetPhysicalDevice() const { return mPhysicalDevice; }
  operator VkDevice() const { return mDevice; }
private:
  /// The instance this Device was created from. We keep it alive as long as this
  /// device is by holding an owner pointer to it.
  InstancePtr mInstance;
  /// The physical device this logical one corresponds to.
  VkPhysicalDevice mPhysicalDevice;
  VkDevice mDevice = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to an instance of Vulkan's VkAccelerationStructure API
   object.
 */
class AccelerationStructure : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  /// @todo Should the buffer be owned???????????????????????????????????????????????????????????????????????/ @fixme ?
  AccelerationStructure(Device& device, const VkAccelerationStructureCreateInfoKHR& info);

public:
  /**
   * @brief Creator for new AccelerationStructure objects.
   * @return Smart pointer wrapper to keep the AccelerationStructure alive.
   */
  static AccelerationStructurePtr New(Device& device, VkAccelerationStructureCreateFlagsKHR createFlags, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size,  VkAccelerationStructureTypeKHR type, VkDeviceAddress deviceAddress);
  ~AccelerationStructure();
  operator VkAccelerationStructureKHR() const { return mAccelerationStructure; }

private:
  /// The GPU device this AccelerationStructure is tied to. Keep it alive as long as this AccelerationStructure is.
  DevicePtr mDevice;
  /// The raw Vulkan AccelerationStructure handle.
  VkAccelerationStructureKHR mAccelerationStructure = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for VkCommandPool API objects.
 */
class CommandPool : public VulkanObject
{
  CommandPool(Device& device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex);
public:
  static CommandPoolPtr New(Device& device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex);
  ~CommandPool();
  Device& GetDevice() const { return *mDevice; }
  operator VkCommandPool() const { return mCommandPool; }
private:
  /// The GPU device this CommandPool is tied to. Keep it alive as long as this CommandPool is.
  DevicePtr mDevice = nullptr;
  /// The raw Vulkan CommandPool handle.
  VkCommandPool mCommandPool = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for VkCommandBuffer API objects.
 */
class CommandBuffer : public VulkanObject
{
  CommandBuffer(CommandPool& pool, VkCommandBufferLevel level);
  /// Used in bulk allocation by Allocate.
  explicit CommandBuffer(CommandPool& pool, VkCommandBuffer vkHandle);
public:
  static CommandBufferPtr New(CommandPool& pool, VkCommandBufferLevel level);
  static void Allocate(CommandPool& pool, VkCommandBufferLevel level, unsigned number, std::vector<CommandBufferPtr>& outCommandBuffers);
  ~CommandBuffer();
  operator VkCommandBuffer() const { return mCommandBuffer;  }
  VkCommandBuffer GetVkCommandBuffer() const { return mCommandBuffer; }
  const VkCommandBuffer* GetVkCommandBufferAddress() const { return &mCommandBuffer; }
  Device& GetDevice() const { return mPool->GetDevice(); }
  /**
   * Keep the parameter alive as long as this command buffer is.
   * Use for resources required for the execution of these commands.
   */
  void KeepAlive(VulkanObject& needed);

private:
  /// The command buffer pool this command buffers was allocated out of.
  CommandPoolPtr mPool;
  /// A container of all Vulkan objects used by the command buffer that keeps
  /// them alive as long as this is.
  void* mKeepAlives = nullptr;
  /// The raw Vulkan CommandBuffer handle.
  VkCommandBuffer mCommandBuffer = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to a Compute Pipeline, wrapped to allow RAII management of its
 * lifetime.
 *
 * @todo Add a factory function which works with pipeline caches.
 * @todo Consider a single Pipeline object to map directly to VkPipeline.
 */
class ComputePipeline : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  ComputePipeline(Device& device, const VkComputePipelineCreateInfo& createInfo);
public:
  /** Creation function to return new ComputePipelines via smart pointers. */
  static ComputePipelinePtr New(Device& device, const VkComputePipelineCreateInfo& createInfo);
  ~ComputePipeline();
 /**
   * Operator to allow the object to be used in raw Vulkan API calls and avoid
   * having to wrap those too.
   */
  operator VkPipeline() const { return mPipeline; }
private:
  /// The GPU device this pipeline is tied to. Keep it alive as long as this image is.
  DevicePtr mDevice;
  /// The raw vulkan handle.
  VkPipeline mPipeline = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for VkDescriptorPool API objects.
 * Only handles pools of descriptors which can be individually
 * freed.
 */
class DescriptorPool : public VulkanObject
{
  DescriptorPool(Device& device, const VkDescriptorPoolCreateInfo& createInfo);
public:
  /// @todo templated std::array<VkDescriptorPoolSize, N> that wraps a call to this and boils away in compilation.
  static DescriptorPoolPtr New(
    Device& device,
    VkDescriptorPoolCreateFlags flags,
    uint32_t maxSets,
    /// Number of different types of descriptor in the pool.
    uint32_t poolSizeCount,
    /// For each type of descriptor in the pool, the number of that type.
    const VkDescriptorPoolSize* pPoolSizes);
  /// For creating a pool of a single type of descriptor.
  static DescriptorPoolPtr New(
    Device& device,
    VkDescriptorPoolCreateFlags flags,
    uint32_t maxSets,
    const VkDescriptorPoolSize& poolSize);
  ~DescriptorPool();
  Device& GetDevice() const { return *mDevice; }
  operator VkDescriptorPool() const { return mDescriptorPool; }
private:
  /// The GPU device this DescriptorPool is tied to. Keep it alive as long as this DescriptorPool is.
  DevicePtr mDevice = nullptr;
  /// The raw Vulkan DescriptorPool handle.
  VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to a descriptor set layout object.
 */
class DescriptorSetLayout : public VulkanObject
{
  DescriptorSetLayout(Device& device, const VkDescriptorSetLayoutCreateInfo& createInfo);
public:
  static DescriptorSetLayoutPtr New(Device& device, VkDescriptorSetLayoutCreateFlags flags,  uint32_t bindingCount,  const VkDescriptorSetLayoutBinding* pBindings);
  ~DescriptorSetLayout();
  operator VkDescriptorSetLayout() const { return mDescriptorSetLayout; }
  const VkDescriptorSetLayout* GetDescriptorSetLayoutAddress() const { return &mDescriptorSetLayout; }
  VkDescriptorSetLayout*       GetDescriptorSetLayoutAddress()       { return &mDescriptorSetLayout; }
private:
  DevicePtr mDevice;
  VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for VkDescriptorSet API objects.
 */
class DescriptorSet : public VulkanObject
{
  //DescriptorSet(Device& device, const VkDescriptorSetAllocateInfo& allocateInfo);
  DescriptorSet(DescriptorPool& pool, const VkDescriptorSetAllocateInfo& allocateInfo);
public:
  /// Allocate a single descriptor set.
  static DescriptorSetPtr Allocate(
    DescriptorPool& pool,
    DescriptorSetLayout& setLayout
  );
  ~DescriptorSet();
  DescriptorPool& GetPool() const { return *mPool; }
  Device& GetDevice() const { return mPool->GetDevice(); }
  operator VkDescriptorSet() const { return mDescriptorSet; }
  const VkDescriptorSet* GetHandleAddress() const { return &mDescriptorSet; }
private:
  /// The pool this DescriptorSet is allocated from. Keep it alive as long as this DescriptorSet is.
  DescriptorPoolPtr mPool = nullptr;
  /// The raw Vulkan DescriptorSet handle.
  VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to a block of memory on a device.
 *
 * Memory is allocated on construction and freed on destruction.
 * Owns a VkDeviceMemory object and allows its lifetime to be managed in RAII
 * fashion with shared pointers.
 */
class DeviceMemory : public VulkanObject
{
  DeviceMemory(Device& device, const VkMemoryAllocateInfo& info);
public:
  static DeviceMemoryPtr New(Device& device, const VkMemoryAllocateInfo& info);
  ~DeviceMemory();
  operator VkDeviceMemory() const { return mDeviceMemory; }
  Device& GetDevice() const { return *mDevice; }
private:
  DevicePtr mDevice;
  VkDeviceMemory mDeviceMemory = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * An owner for VkBuffer API objects.
 * It keeps its own handle alive and the device it came from.
 */
class Buffer : public VulkanObject
{
  Buffer(Device& device, const VkBufferCreateInfo& info);
public:
  /// @todo A factory function which allows more than one queue family.
  static BufferPtr New(Device& device, VkBufferCreateFlags flags, VkDeviceSize size, VkBufferUsageFlags usage, VkSharingMode sharingMode, uint32_t queueFamilyIndex);
  ~Buffer();
  /// Back this buffer with a region of memory as vkBindBufferMemory() would, but also establish that this object keepsthe memory alive.
  VkResult BindMemory(DeviceMemory& memory, VkDeviceSize memoryOffset);
  Device& GetDevice() const { return *mDevice; }
  DeviceMemory& GetMemory() const { return *mMemory; }
  operator VkBuffer() const { return mBuffer; }
private:
  /// The GPU device this Buffer is tied to. Keep it alive as long as this Buffer is.
  DevicePtr mDevice = nullptr;
  /// The memory this buffer is associated with. At creation time this is null.
  DeviceMemoryPtr mMemory = nullptr;
  /// The raw Vulkan Buffer handle.
  VkBuffer mBuffer = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to a fence.
 *
 * Owns a VkFence object and allows its lifetime to be managed in RAII
 * fashion with shared pointers.
 */
class Fence : public VulkanObject
{
  Fence(Device& device, const VkFenceCreateInfo& info);
public:
  static FencePtr New(Device& device, const VkFenceCreateFlags flags);
  ~Fence();
  operator VkFence() const { return mFence; }
  const VkFence* GetVkFenceAddress() const { return &mFence; }
  VkFence* GetVkFenceAddress() { return &mFence; }
private:
  DevicePtr mDevice;
  VkFence mFence = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to an instance of Vulkan's abstract image API object.
 *
 * @todo: Don't need a direct device pointer (ask memory for it when needed)
 */
class Image : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  Image(Device& device, const VkImageCreateInfo& createInfo);
public:
  /**
   * @brief Creator for new Image objects.
   * @return Smart pointer wrapper to keep the Image alive.
   */
  static ImagePtr New(Device& device, const VkImageCreateInfo& createInfo);
  ~Image();
  operator VkImage() const { return mImage; }
  Device& device() { return *mDevice; }
  DeviceMemory& memory() { return *mMemory; }
  /**
   * @brief Provide a location in a block of device memory to hold the image's
   * pixels.
   *
   * Only call this once since, as the spec tells us, "Once bound, the memory
   * binding is immutable for the lifetime of the resource."
   */
  void BindMemory(DeviceMemory& memory, VkDeviceSize offset);
private:
  /// The GPU device this image is tied to. Keep it alive as long as this image is.
  DevicePtr mDevice;
  /// The GPU memory block the image pixels exist in (if any):
  DeviceMemoryPtr mMemory;
  /// The raw Vulkan image handle.
  VkImage mImage = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to an instance of Vulkan's opaque image handle API object.
 */
class ImageView : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  ImageView(Image& image, const VkImageViewCreateInfo& createInfo);
public:
  /**
   * @brief Creator for new Image objects.
   *
   * The image in the createInfo will be ignored and replaced with the image
   * passed-in.
   * @return Smart pointer wrapper to keep the Image alive.
   */
  static ImageViewPtr New(Image& image, const VkImageViewCreateInfo& createInfo);
  ~ImageView();
  operator VkImageView() const { return mImageView; }

  // Getters:
  Image&        image()  { return *mImage; }
  DeviceMemory& memory() { return  mImage->memory(); }
  Device&       device() { return  mImage->device(); }

private:
  /// The image this is a view onto.
  ImagePtr mImage;
  /// The raw Vulkan image view handle.
  VkImageView mImageView = VK_NULL_HANDLE;
};



/* ----------------------------------------------------------------------- *//**
 * @brief A handle to a pipeline layout, wrapped to allow RAII management of its
 * lifetime.
 *
 * Shared pointer to automatically manage the lifetime of a PipelineLayout object and
 * thus the underlying Vulkan API object.
 */
class PipelineLayout : public VulkanObject
{
  PipelineLayout(Device& device, const VkPipelineLayoutCreateInfo& createInfo);
public:
  /// @todo cleaner versions of this without separate count and pointer.
  static PipelineLayoutPtr New(
    Device&                         device,
    VkPipelineLayoutCreateFlags     flags,
    uint32_t                        setLayoutCount = 0,
    const VkDescriptorSetLayout*    pSetLayouts = nullptr,
    uint32_t                        pushConstantRangeCount = 0,
    const VkPushConstantRange*      pPushConstantRanges = nullptr);

  /// A version for when there is a single layout and a single push constant range.
  static PipelineLayoutPtr New(
    Device&                         device,
    VkPipelineLayoutCreateFlags     flags,
    const VkDescriptorSetLayout&    setLayout,
    const VkPushConstantRange&      pushConstantRange);

  /// A version for when there is a single layout and no push constants.
  static PipelineLayoutPtr New(
    Device&                         device,
    VkPipelineLayoutCreateFlags     flags,
    const VkDescriptorSetLayout&    setLayout);

  ~PipelineLayout();
  operator VkPipelineLayout() const { return mPipelineLayout; }
  //const VkPipelineLayout* GetPipelineLayoutAddress() const { return &mPipelineLayout; }
  //VkPipelineLayout*       GetPipelineLayoutAddress()       { return &mPipelineLayout; }
private:
  DevicePtr mDevice;
  VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
};



/// A bundle of SPIR-V static code.
/// @todo Make this a lightweight concrete class, just a size, a pmr allocator
/// pointer, and a pointer to the data.
using ShaderBuffer = std::vector<uint32_t>;
inline ShaderBuffer::size_type byte_size(const ShaderBuffer& sb) { return sb.size() * sizeof(ShaderBuffer::value_type); }

/* ----------------------------------------------------------------------- *//**
 * @brief A handle to an instance of Vulkan's VkShaderModule API object.
 */
class ShaderModule : public VulkanObject
{
  /** Hidden constructor to prevent users doing naked `new`s.*/
  ShaderModule(Device& device, const VkShaderModuleCreateInfo& info);

public:
  /**
   * @brief Creator for new ShaderModule objects.
   * @return Smart pointer wrapper to keep the ShaderModule alive.
   */
  static ShaderModulePtr New(Device& device, VkShaderModuleCreateFlags flags, const ShaderBuffer& src);
  ~ShaderModule();
  operator VkShaderModule() const { return mShaderModule; }

private:
  /// The GPU device this ShaderModule is tied to. Keep it alive as long as this ShaderModule is.
  DevicePtr mDevice;
  /// The raw Vulkan ShaderModule handle.
  VkShaderModule mShaderModule = VK_NULL_HANDLE;
};


///@}

}

#endif // #ifndef KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF
