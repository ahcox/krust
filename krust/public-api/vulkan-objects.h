#ifndef KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF
#define KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF

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
#include "krust/public-api/intrusive-pointer.h"
#include "krust/public-api/ref-object.h"

// External includes:
#include <vulkan/vulkan.h>

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

/**
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

class Instance;
/**
* Shared pointer to automatically manage the lifetime of an Instance object and
* thus the underlying Vulkan API object.
*/
using InstancePtr = IntrusivePointer<Instance>;

/**
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

class Device;
using DevicePtr = IntrusivePointer<Device>;

/**
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

class CommandPool;
using CommandPoolPtr = IntrusivePointer<CommandPool>;

/**
 * An owner for VkCommandPool API objects.
 */
class CommandPool : public VulkanObject
{
  CommandPool(Device& device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex);
public:
  static CommandPoolPtr New(Device& device, VkCommandPoolCreateFlags flags, uint32_t queueFamilyIndex);
  ~CommandPool();
  operator VkCommandPool() const { return mCommandPool; }
private:
  /// The GPU device this CommandPool is tied to. Keep it alive as long as this CommandPool is.
  DevicePtr mDevice = nullptr;
  /// The raw Vulkan CommandPool handle.
  VkCommandPool mCommandPool = VK_NULL_HANDLE;
};

class DeviceMemory;
using DeviceMemoryPtr = IntrusivePointer<DeviceMemory>;

/**
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
private:
  DevicePtr mDevice;
  VkDeviceMemory mDeviceMemory = VK_NULL_HANDLE;
};

class Image;
using ImagePtr = IntrusivePointer<Image>;

/**
 * @brief A handle to an instance of Vulkan's abstract image API object.  
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
  /**
   * @brief Provide a location in a block of device memory to hold the image's
   * pixels.
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

///@}

}

#endif // #ifndef KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF
