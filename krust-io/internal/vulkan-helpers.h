#ifndef KRUST_IO_INTERNAL_VULKAN_HELPERS_H_
#define KRUST_IO_INTERNAL_VULKAN_HELPERS_H_

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

/**
* @file Helpers and utilities for the Vulkan API.
*/

// External includes:
#include <krust/public-api/krust-assertions.h>
#include <krust/public-api/vulkan-utils.h>
#include <krust/public-api/logging.h>
#include <krust/public-api/vulkan_types_and_macros.h>

// System external includes:
#include <vector>

namespace Krust
{

// Logging forward declarations:
class LogChannel;
enum class LogLevel : uint8_t;

namespace IO
{

/**
 * @brief Can be registered with Vk to output errors.
 */
VkBool32 DebugCallback(VkFlags flags, VkDebugReportObjectTypeEXT objectType,
  uint64_t object, size_t location, int32_t code, const char *layerPrefix,
  const char *message, void* userData);

/**
 * @brief Gets the image formats compatible with the surface.
 */
std::vector<VkSurfaceFormatKHR>
    GetSurfaceFormatsKHR(PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pGetSurfaceFormatsKHR,
                         VkPhysicalDevice gpu,
                         VkSurfaceKHR surface);
/**
 * @brief Gets the KHR Present modes compatible with the surface.
 */
std::vector<VkPresentModeKHR>
  GetSurfacePresentModesKHR(PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pGetPhysicalDeviceSurfacePresentModesKHR,
                            const VkPhysicalDevice physicalDevice, const VkSurfaceKHR& surface);

/**
 * @brief Gets the images created for a swapchain by a previous call to
 * vkCreateSwapchainKHR.
 * @return Vector of images to be used in the swap chain if the function
 * succeeds or an empty vector if it fails.
 */
std::vector<VkImage> GetSwapChainImages(PFN_vkGetSwapchainImagesKHR getter, VkDevice gpuInterface, VkSwapchainKHR swapChain);

/**
 * @brief Output device features to info log.
 */
void LogVkPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& features);

/**
 * @brief Output device limits to info log.
 */
void LogVkPhysicalDeviceLimits(const VkPhysicalDeviceLimits &limits);

/**
 * @brief Output device memory types and heaps to info log.
 */
void LogMemoryTypes(const VkPhysicalDeviceMemoryProperties& memory_properties);

/**
 * @brief Output surface capabilities to log.
 */
void LogVkSurfaceCapabilitiesKHR(const VkSurfaceCapabilitiesKHR &capabilities,
                                 LogChannel &log = Krust::log,
                                 LogLevel level = LogLevel::Info);

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_INTERNAL_VULKAN_HELPERS_H_ */
