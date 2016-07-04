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
#include "krust/internal/krust-internal.h"

namespace Krust
{

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

Instance::~Instance()
{
  vkDestroyInstance(mInstance, Internal::sAllocator);
}

Device::Device(Instance & instance, VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo & createInfo) :
  mInstance(instance)
{
  KRUST_ASSERT1(&instance != 0 && instance != VK_NULL_HANDLE, "Invalid instance.");
  KRUST_ASSERT1(&physicalDevice != VK_NULL_HANDLE, "Invalid physical device.");
  KRUST_ASSERT1(&createInfo != 0, "Invalid device create info.");

  mPhysicalDevice = physicalDevice;
  auto & threadBase = ThreadBase::Get();
  const VkResult result = vkCreateDevice(physicalDevice, &createInfo, Internal::sAllocator, &mDevice);
  if (result != VK_SUCCESS)
  {
    mDevice = VK_NULL_HANDLE;
    threadBase.GetErrorPolicy().VulkanError("vkCreateDevice", result, nullptr, __FUNCTION__, __FILE__, __LINE__);
  }
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

CommandPool::~CommandPool()
{
  vkDestroyCommandPool(*mDevice, mCommandPool, Internal::sAllocator);
}

}
