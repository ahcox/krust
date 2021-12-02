// Copyright (c) 2021 Andrew Helge Cox
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

/**
 * @file Implementation of an Owner/Janitor for mapping host-visible GPU device
 * memory to be accessed directly from CPU code and then remembering to always
 * flush and unmap it.
 */

// Internal includes
#include "krust/public-api/device-memory-mapper.h"
#include "krust/public-api/vulkan-objects.h"
#include "krust/public-api/vulkan_struct_init.h"
#include "krust/public-api/thread-base.h"
//#include "krust/public-api/logging.h"
#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/krust-errors.h"
#include "krust/public-api/vulkan.h"

namespace Krust
{
DeviceMemoryMapper::DeviceMemoryMapper(DeviceMemory& deviceMemory, size_t offset, size_t size, uint32_t flags)
: mMemory(&deviceMemory), mOffset(offset), mSize(size)
{
    Device& device = deviceMemory.GetDevice();
    // Chapter 11: memory must not be currently host mapped
    const auto result = vkMapMemory(device, deviceMemory, offset, size, flags, &mHostAccess);
    const auto memRange = MappedMemoryRange(*mMemory, mOffset, mSize);
    const auto resultInval = vkInvalidateMappedMemoryRanges(device, 1, &memRange);
    if(resultInval != VK_SUCCESS){
        KRUST_LOG_ERROR << "Failed to invalidate host caches for mapped memory with result: " << resultInval << endlog;
    }
    // ppData is a pointer to a void * variable in which is returned a host-accessible pointer
    // to the beginning of the mapped range.
    // This pointer minus offset must be aligned to at least
    // VkPhysicalDeviceLimits::minMemoryMapAlignment.
    if(result != VK_SUCCESS){
      mHostAccess = nullptr;
      ThreadBase::Get().GetErrorPolicy().VulkanError("vkMapMemory", result, "Failed to map device memory on the host.", __FUNCTION__, __FILE__, __LINE__);
    }
}

DeviceMemoryMapper::~DeviceMemoryMapper()
{
    unMap();
}

void DeviceMemoryMapper::unMap()
{
    if(mHostAccess)
    {
        auto& device = mMemory->GetDevice();
        const auto memRange = MappedMemoryRange(*mMemory, mOffset, mSize);
        const auto result = vkFlushMappedMemoryRanges(device, 1, &memRange);
        if(result != VK_SUCCESS){
            KRUST_LOG_ERROR << "Failed to flush mapped memory with result: " << result << endlog;
        }
        vkUnmapMemory(device, *mMemory);
        mHostAccess = nullptr;
    }
}

}
