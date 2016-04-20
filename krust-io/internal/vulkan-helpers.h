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

// External includes:
#include <vulkan/vulkan.h>
#include <krust/public-api/krust-assertions.h>
#include <krust/public-api/vulkan-utils.h>
#include <krust/public-api/logging.h>
// System external includes:
#include <vector>

/** Save a little typing when getting a pointer to an extension. */
#define KRUST_GET_INSTANCE_EXTENSION(instance, extensionSuffix) \
    (PFN_vk##extensionSuffix) Krust::IO::GetInstanceProcAddr(instance, "vk"#extensionSuffix)

/** Save a little typing when getting a pointer to an extension. */
#define KRUST_GET_DEVICE_EXTENSION(device, extensionSuffix) \
    (PFN_vk##extensionSuffix) Krust::IO::GetDeviceProcAddr(device, "vk"#extensionSuffix)

namespace Krust
{

// Logging forward declarations:
class LogChannel;
enum class LogLevel : uint8_t;

namespace IO
{

/** Get a function pointer for a device extension and log the error if it fails. */
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance& instance, const char * const name);

/** Get a function pointer for a device extension and log the error if it fails. */
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice& device, const char * const name);

/**
 * @brief Find the named extension in the list of extension property structs
 * passed in.
 */
bool FindExtension(const std::vector<VkExtensionProperties>& extensions, const char* const extension);

/**
 * @brief Can be registered with Vk to output errors.
 */
VkBool32 DebugCallback(VkFlags flags, VkDebugReportObjectTypeEXT objectType,
  uint64_t object, size_t location, int32_t code, const char *layerPrefix,
  const char *message, void* userData);

/**
 * @brief Converts VkDebugReportFlagsEXT with single bit set to all-caps string
 * for human-readable logging.
 */
const char * MessageFlagsToLevel(const VkFlags flags);

/**
 * @return a vector of properties for each global layer available.
 */
std::vector<VkLayerProperties> EnumerateInstanceLayerProperties();

/**
 * @brief Allows the application to query properties of all extensions.
 * @param[in] layerName The name of the layer to query extions properties from.
 *             If this is null, the extension properties are queried globally.
 * @return a vector of properties for each extension available.
 */
std::vector<VkExtensionProperties> GetGlobalExtensionProperties(const char * const layerName = 0);

/**
 * @brief Return a list of handles to physical GPUs known to the loader.
 * @return A vector of GPU handles if successful, or an empty list if there was an error.
 */
std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(
  const VkInstance &instance);

/**
 *
 */
std::vector<VkExtensionProperties>
    EnumerateDeviceExtensionProperties(VkPhysicalDevice &gpu,
                                       const char *const name);

/**
 * @brief Gets a list of layers for the physical device (GPU) passed in.
 * @return A vector of layer property structs if successful, or an empty vector if there was an error.
 */
std::vector<VkLayerProperties>
  EnumerateDeviceLayerProperties(const VkPhysicalDevice &gpu);

/**
 * @brief Gets the properties of all GPU queue families.
 *
 * The properties are a set of flags and a count of the number of queues
 * belonging to the family. The flags are Graphics, Compute, sparse memory,
 * DMA, etc.
 */
std::vector<VkQueueFamilyProperties>
    GetPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice gpu);

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
 * @brief Converts a Vulkan format to its textual representation.
 *
 * For example: <code>VK_FORMAT_R8_SRGB -> "VK_FORMAT_R8_SRGB"</code>.
 */
const char* FormatToString(const VkFormat format);

/**
 * @brief Converts a KHR colorspace enum into a string representation of it.
 */
const char* KHRColorspaceToString(const VkColorSpaceKHR space);

/**
 * @brief Output device features to info log.
 */
void LogVkPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures& features);

/**
 * @brief Output device limits to info log.
 */
void LogVkPhysicalDeviceLimits(const VkPhysicalDeviceLimits &limits);

/**
 * @brief Output surface capabilities to log.
 */
void LogVkSurfaceCapabilitiesKHR(const VkSurfaceCapabilitiesKHR &capabilities,
                                 LogChannel &log = Krust::log,
                                 LogLevel level = LogLevel::Info);



} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_INTERNAL_VULKAN_HELPERS_H_ */
