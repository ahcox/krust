#ifndef KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H
#define KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H

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

/**
 * @file Helpers and utilities for the Vulkan API.
 *
 * @todo Separate out helpers that use vulkan-objects.h ownership wrappers into their own file.
 */

// Internal includes:
#include "krust/public-api/vulkan-objects.h"
#include <krust/public-api/vulkan_types_and_macros.h>

// External includes:
#include "conditional-value.h"
#include <stdint.h>
#include <vector>

/** Save a little typing when getting a pointer to an extension. */
#define KRUST_GET_INSTANCE_EXTENSION(instance, extensionSuffix) \
    (PFN_vk##extensionSuffix) Krust::GetInstanceProcAddr(instance, "vk"#extensionSuffix)

/** Save a little typing when getting a pointer to an extension. */
#define KRUST_GET_DEVICE_EXTENSION(device, extensionSuffix) \
    (PFN_vk##extensionSuffix) Krust::GetDeviceProcAddr(device, "vk"#extensionSuffix)


/**
 * @brief A little wrapper to make sure errors from Vulkan calls are logged.
 *
 * Pass the function identifier followed by the parameters to the function.
 * @param[in] FUNC A function or function pointer which returns a VkResult.
 * @todo Call standard error handling mechanism and take a dependency on threadbase in this file? Krust::ThreadBase::Get().GetErrorPolicy().VulkanError(""#FUNC, FUNC##result, nullptr, __FUNCTION__, __FILE__, __LINE__);
 */
#define VK_CALL(FUNC, ...) \
{ \
const VkResult FUNC##result = FUNC( __VA_ARGS__ ); \
if(FUNC##result != VK_SUCCESS) { \
  KRUST_LOG_ERROR << "Call to " #FUNC " failed with error: " << Krust::ResultToString(FUNC##result) << Krust::endlog; \
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
  KRUST_LOG_ERROR << "Call to " #FUNC " failed with error: " << Krust::ResultToString(FUNC##result) << Krust::endlog; \
  return false; \
} \
}

/**
 * @copydoc VK_CALL
 *
 * Returns from embedding function with VkResult from called API function.
 */
#define VK_CALL_RET_RES(FUNC, ...) \
{ \
const VkResult FUNC##result = FUNC( __VA_ARGS__ ); \
if(FUNC##result != VK_SUCCESS) { \
  KRUST_LOG_ERROR << "Call to " #FUNC " failed with error: " << Krust::ResultToString(FUNC##result) << Krust::endlog; \
  return FUNC##result; \
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

/** @brief Runs a memory barrier on the image passed in and waits for it to
 *  complete.
 * 
 * This is a slow function for use a few times during startup, not something to
 * call inside a render loop per-frame.
 */
VkResult ApplyImageBarrierBlocking(Device& device, VkImage image, VkQueue queue, CommandPool& pool, const VkImageMemoryBarrier& barrier);

/**
 * @brief Examines the memory types offered by the device, looking for one which
 * is both one of the input candidate types and has the desired properties
 * associated with it.
 * The type may also have other properties.
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
FindFirstMemoryTypeWithProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t candidateTypeBitset, VkMemoryPropertyFlags properties);

/**
 * @brief Examines the memory types offered by the device, looking for one which
 * is both one of the input candidate types and has exactly the desired properties
 * associated with it.
 * The type will not have any other properties.
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
FindMemoryTypeMatchingProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t candidateTypeBitset, VkMemoryPropertyFlags properties);

/**
 * @brief Examines the memory types offered by the device, looking for one which
 * is both one of the input candidate types and both has the desired properties
 * and doesn't have the banned properties associated with it.
 * The type may have any other properties.
 * @param[in] memoryProperties The device memory properties that should be
 *            examined.
 * @param[in] candidateTypeBitset A set of bits: if bit x of this is set then
 *            type x in the device's list of memory types can be considered.
 * @param[in] properties A set of VkMemoryPropertyFlagBits memory properties to
 *            be searched for. E.g. `VK_MEMORY_PROPERTY_DEVICE_ONLY`.
 * @param[in] avoided_properties A set of VkMemoryPropertyFlagBits memory properties to
 *            be avoide. E.g. `VK_MEMORY_PROPERTY_HOST_VISIBLE`.
 * @returns A pair of the index of a compatible memory type and true if one
 *          could be found, or a pair of an undefined value and false otherwise.
 *
 */
ConditionalValue<uint32_t>
FindMemoryTypeWithAndWithout(
  const VkPhysicalDeviceMemoryProperties& memoryProperties,
  uint32_t candidateTypeBitset,
  VkMemoryPropertyFlags properties,
  VkMemoryPropertyFlags avoided_properties);

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
 * @brief Create a given number of fences.
 */
void BuildFences(
  Krust::Device& device,
  const VkFenceCreateFlags flags,
  const size_t numSwapChainImageViews,
  std::vector<FencePtr>& outSwapChainFences);

/**
 * @brief Build a framebuffer and renderpass per swapchain image with a simple
 * configuration using a single shared depth buffer.
 */
bool BuildFramebuffersForSwapChain(
  Krust::Device& device,
  const std::vector<VkImageView>& swapChainImageViews,
  const VkImageView depthBufferView,
  const uint32_t surfaceWidth, const uint32_t surfaceHeight,
  const VkFormat colorFormat,
  const VkFormat depthFormat,
  const VkSampleCountFlagBits colorSamples,
  std::vector<VkRenderPass>& outRenderPasses,
  std::vector<VkFramebuffer>& outSwapChainFramebuffers,
  std::vector<FencePtr>& outSwapChainFences);

/** Get a function pointer for a device extension and log the error if it fails. */
PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char * const name);

/** Get a function pointer for a device extension and log the error if it fails. */
PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char * const name);

/**
* @brief Find the named extension in the list of extension property structs
* passed in.
*/
bool FindExtension(const std::vector<VkExtensionProperties>& extensions, const char* const extension);

/**
 * @brief Search for a layer name among the layer property structs passed in.
 */
bool FindLayer(const std::vector<VkLayerProperties> &layers, const char *const layer);

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

/**
 * Simple synchronous load of a SPIR-V shader from a file.
 */
ShaderBuffer loadSpirV(const char* const filename);

///@}

} /* namespace Krust */

#endif // KRUST_COMMON_PUBLIC_API_VULKAN_UTILS_H
