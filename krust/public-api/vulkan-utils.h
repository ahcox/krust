#ifndef KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H
#define KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H

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
#include "conditional-value.h"
#include <vulkan/vulkan.h>
#include <stdint.h>
#include <vector>

/**
 * @brief A little wrapper to make sure errors from Vulkan calls are logged.
 *
 * Pass the function identifier followed by the parameters to the function.
 * @param[in] FUNC A function or function pointer which returns a VkResult.
 */
#define VK_CALL(FUNC, ...) \
{ \
const VkResult FUNC##result = FUNC( __VA_ARGS__ ); \
if(FUNC##result != VK_SUCCESS) { \
  KRUST_LOG_ERROR << "Call to " #FUNC " failed with error: " << FUNC##result << Krust::endlog; \
} \
}

/**
 * @copydoc VK_CALL
 *
 * Returns from embedding function with bool: false on error.
 */
#define VK_CALL_RET(FUNC, ...) \
{ \
const VkResult FUNC##result = FUNC( __VA_ARGS__ ); \
if(FUNC##result != VK_SUCCESS) { \
  KRUST_LOG_ERROR << "Call to " #FUNC " failed with error: " << FUNC##result << Krust::endlog; \
  return false; \
} \
}

/**
 * @brief CPU memory allocator to be used by Vulkan implementations
 * if no better one is passed to Krust::InitKrust().
 * @todo Move KRUST_DEFAULT_ALLOCATION_CALLBACKS to non-public.
 */
#define KRUST_DEFAULT_ALLOCATION_CALLBACKS nullptr

namespace Krust
{

/**
 * @brief Fill out a creation struct suitable for creating an image object
 * suitable to be used as a depth buffer.
 * @return The completed struct.
 */
VkImageCreateInfo CreateDepthImageInfo(uint32_t presentQueueFamily, VkFormat depthFormat, uint32_t width, uint32_t height);

/**
 * @returns An image view for the depth buffer image passed in if the creation
 * succeeds, or else a zero handle if the creation fails.
 * The caller must destroy the view eventually.
 */
VkImageView CreateDepthImageView(VkDevice device, VkImage image, const VkFormat format);

/**
 * @brief Examines the memory types offered by the device, looking for one which
 * is both one of the input candidate types and has the desired property
 * associated with it.
 * @param[in] memoryProperties The device memory properties that should be
 *            examined.
 * @param[in] candidateTypeBitset A set of bits: if bit x of this is set then
 *            type x in the device's list of memory types can be considered.
 * @param[in] properties A set of VkMemoryPropertyFlagBits memory properties to
 *            be searched for. E.g. `VK_MEMORY_PROPERTY_DEVICE_ONLY`.
 * @returns A pair of the index of a compatible memory type and true if one
 *          could be found, or a pair of an undefined value and false otherwise.
 *
 */
ConditionalValue<uint32_t>
  FindMemoryTypeWithProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t candidateTypeBitset, VkMemoryPropertyFlags properties);

/**
 * @brief Determines whether format argument is usable for a depth buffer.
 */
bool IsDepthFormat(const VkFormat format);

/**
 * @brief Outputs a message string and textual representation of a result code
 * on the error log.
 * @deprecated Wrapping here loses location, file, line number, etc.
 */
void LogResultError(const char* message, VkResult result);

/**
 * @brief Converts a Vulkan result code to a string representation of it.
 */
const char* ResultToString(const VkResult result);

/**
 * @return An integer representing how desirable a present mode is. Lower is
 * better.
 * @param{in] mode The mode to be ranked.
 * @param[in] tearingAllowed Whether modes which cause tearing should be
 * penalised.
 */
int SortMetric(VkPresentModeKHR mode, bool tearingAllowed = false);

/**
 * @brief Convert a surface transform enumerant value into a string.
 *
 * For example: VK_SURFACE_TRANSFORM_NONE_KHR -> "VK_SURFACE_TRANSFORM_NONE_KHR"
 */
const char* SurfaceTransformToString(const VkSurfaceTransformFlagsKHR transform);

/**
 * @brief Build a framebuffer and renderpass per swapchain image with a simple
 * configuration using a single shared depth buffer.
 */
bool BuildFramebuffersForSwapChain(
  const VkDevice device,
  const std::vector<VkImageView>& swapChainImageViews,
  const VkImageView depthBufferView,
  const uint32_t surfaceWidth, const uint32_t surfaceHeight,
  const VkFormat colorFormat,
  const VkFormat depthFormat,
  const VkSampleCountFlagBits colorSamples,
  std::vector<VkRenderPass>& outRenderPasses,
  std::vector<VkFramebuffer>& outSwapChainFramebuffers);

/**
* @brief Find the named extension in the list of extension property structs
* passed in.
*/
bool FindExtension(const std::vector<VkExtensionProperties>& extensions, const char* const extension);

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
 * @name ScopedDestruction Destroy Vulkan objects as they go out of scope.
 * Part of the general strategy of weak exception safety.
 */
///@{

/**
 * @brief Frees device memory when it goes out of scope unless it is released
 * earlier.*/
struct ScopedDeviceMemoryOwner
{
  ScopedDeviceMemoryOwner(VkDevice dev, VkDeviceMemory mem);
  ~ScopedDeviceMemoryOwner();
  VkDeviceMemory Release() { VkDeviceMemory tmp = memory; memory = 0; return tmp; }
  VkDevice device;
  VkDeviceMemory memory;
};

/**
 * @brief Destroys an image when it goes out of scope unless the image is
 * released first.*/
struct ScopedImageOwner
{
  ScopedImageOwner(VkDevice dev, VkImage img);
  ~ScopedImageOwner();
  VkImage Release() { VkImage tmp = image; image = 0; return tmp; }
  VkDevice device;
  VkImage image;
};

///@}

} /* namespace Krust */

#endif // KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H
