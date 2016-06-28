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
 * A reference to an Image, for example, will keep it's associated Device and
 * Instance alive too.
 * 
 * These attempt to solve the single problem of managing the lifetime of Vulkan
 * API objects in an RAII manner and thus do not wrap every Vulkan API call
 * associated with a given object in an attempt to provide an OO veneer over the
 * API. Instead, a conversion operator is provided for each that yields the
 * underlying Vulkan object handle. This lets them be passed to Vulkan API
 * functions and random helper code the user may have assembled from various
 * sources.
 *
 * Submitting command buffers to the GPU keeps all associated API objects and
 * resources alive until the GPU is using them, as long as the Queue wrapper is
 * used for submission.
 * Queue must call `vkQueueWaitIdle` and Device must call `vkDeviceWaitIdle` for
 * their associated GPU to idle before their destruction can complete but the
 * majority of Vulkan object owners do not need to block on GPU idle.
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

/**
 * An owner for a VkInstance Vulkan API object. 
 */
class Instance : public VulkanObject
{
public:
  Instance(const VkInstanceCreateInfo& createInfo);
  ~Instance();
  /**
   * Operator to allow the object to be used in raw Vulkan API calls and avoid
   * having to wrap those too.
   */
  operator VkInstance() const { return mInstance; }
private:
  VkInstance mInstance = VK_NULL_HANDLE;
};

/**
 * Shared pointer to automatically manage the lifetime of an Instance object and
 * thus the underlying Vulkan API object.
 */
typedef IntrusivePointer<Instance> InstancePtr;

/**
 * An owner for vkDevice instances.
 */
class Device : public VulkanObject
{
public:
  Device(Instance& instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo& createInfo);
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

typedef IntrusivePointer<Device> DevicePtr;

///@}

}

#endif // #ifndef KRUST_VULKAN_OBJECTS_H_INCLUDED_E26EF
