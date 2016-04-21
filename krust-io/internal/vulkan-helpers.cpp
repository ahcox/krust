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

#include "vulkan-helpers.h"

// Internal includes
#include "krust/public-api/compiler.h"
#include "krust/public-api/krust-assertions.h"
#include <krust/public-api/logging.h>
#include <krust/public-api/vulkan-logging.h>

// External includes:
#include <cstring>

namespace Krust
{
namespace IO {

bool FindExtension(const std::vector<VkExtensionProperties> &extensions, const char *const extension) {
  KRUST_ASSERT1(extension, "Name of extension to look up is missing.");
  for (auto potential : extensions) {
    if (strcmp(extension, potential.extensionName) == 0) {
      return true;
    }
  }
  KRUST_LOG_WARN << "Failed to find extension \"" << extension << "\"." << endlog;
  return false;
}

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance &instance, const char *const name) {
  KRUST_ASSERT1(name, "Must pass a name to look up.");
  const PFN_vkVoidFunction address = vkGetInstanceProcAddr(instance, name);
  if (!address) {
    KRUST_LOG_ERROR << "Failed to get the address of instance proc: " << name << endlog;
  }
  return address;
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice &device, const char *const name) {
  KRUST_ASSERT1(name, "Must pass a name to look up.");
  const PFN_vkVoidFunction address = vkGetDeviceProcAddr(device, name);
  if (!address) {
    KRUST_LOG_ERROR << "Failed to get the address of device proc: " << name << endlog;
  }
  return address;
}

std::vector<VkExtensionProperties> GetGlobalExtensionProperties(const char *const name) {
  VkResult result;
  uint32_t extensionCount = 0;
  std::vector<VkExtensionProperties> extensionProperties;

  // We make the call once to get the size of the array to return and then again to
  // actually populate an array:

  result = vkEnumerateInstanceExtensionProperties(name, &extensionCount, 0);
  if (result) {
    KRUST_LOG_ERROR << ": vkGetGlobalExtensionProperties returned " << ResultToString(result) << ".\n";
  }
  else {
    extensionProperties.resize(extensionCount);
    result = vkEnumerateInstanceExtensionProperties(name, &extensionCount, &extensionProperties[0]);
    KRUST_ASSERT1(extensionCount <= extensionProperties.size(), "Extension count changed.");
    if (result) {
      extensionProperties.clear();
      KRUST_LOG_ERROR << ": vkGetGlobalExtensionProperties returned " << ResultToString(result) << ".\n";
    }
  }
  return extensionProperties;
}

std::vector<VkLayerProperties> EnumerateInstanceLayerProperties() {
  VkResult result;
  uint32_t layerCount = 0;
  std::vector<VkLayerProperties> layerProperties;

  // We make the call once to get the size of the array to return and then again to
  // actually populate an array:

  result = vkEnumerateInstanceLayerProperties(&layerCount, 0);
  if (result) {
    KRUST_LOG_ERROR << ": vkGetGlobalLayerProperties returned " << ResultToString(result) << ".\n";
  }
  else {
    layerProperties.resize(layerCount);
    result = vkEnumerateInstanceLayerProperties(&layerCount, &layerProperties[0]);
    KRUST_ASSERT1(layerCount <= layerProperties.size(), "Layer count changed");
    if (result) {
      layerProperties.clear();
      KRUST_LOG_ERROR << ": vkGetGlobalLayerProperties returned " << ResultToString(result) << ".\n";
    }
  }
  return layerProperties;
}

std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(
  const VkInstance &instance) {
  uint32_t physicalDeviceCount;
  std::vector<VkPhysicalDevice> physicalDevices;

  // Query for the number of GPUs:
  VkResult gpuResult = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);

  if (gpuResult == VK_SUCCESS) {
    // Go ahead and get the list of GPUs:
    physicalDevices.resize(physicalDeviceCount);
    gpuResult = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);

    if (gpuResult != VK_SUCCESS) {
      physicalDevices.resize(0);
    }
  }

  if (gpuResult != VK_SUCCESS) {
    KRUST_LOG_ERROR << "vkEnumeratePhysicalDevices() returned " << ResultToString(gpuResult) << ".\n";
  }

  return physicalDevices;
}

const char *MessageFlagsToLevel(const VkFlags flags)
{
  const char* level = "UNKNOWN";
  switch(flags)
  {
    case VK_DEBUG_REPORT_INFORMATION_BIT_EXT: { level = "INFO"; break; }
    case VK_DEBUG_REPORT_WARNING_BIT_EXT: { level = "WARN"; break; }
    case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT: { level = "PERF_WARN"; break; }
    case VK_DEBUG_REPORT_ERROR_BIT_EXT: { level = "ERROR"; break; }
    case VK_DEBUG_REPORT_DEBUG_BIT_EXT: { level = "DEBUG"; break; }
  }
  return level;
}

VkBool32 DebugCallback(
  VkFlags flags,
  VkDebugReportObjectTypeEXT objectType,
  uint64_t object,
  size_t location,
  int32_t code,
  const char *layerPrefix,
  const char *message,
  void* userData
)
{
  suppress_unused(object, objectType, userData);
  static uint32_t error_count = 0;
  static uint32_t warning_count = 0;

  if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
  {
    ++error_count; //< Just a line to set a breakpoint on to watch errors.
  }
  if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
  {
    ++warning_count; //< Just a line to set a breakpoint on to watch warnings.
  }

  KRUST_DEBUG_LOG_WARN << "<< VK_" << MessageFlagsToLevel(flags) << " [" << layerPrefix << "] " << message << ". [code = " << code << ", location = " << location << "] >>" << endlog;
  // Return 0 to allow the system to keep going (may fail disasterously anyway):
  return 0;
}

std::vector<VkExtensionProperties>
EnumerateDeviceExtensionProperties(VkPhysicalDevice &gpu,
                                   const char *const name) {
  uint32_t count = 0;
  std::vector<VkExtensionProperties> properties;
  VkResult result = vkEnumerateDeviceExtensionProperties(gpu, name, &count, 0);
  if (result == VK_SUCCESS) {
    properties.resize(count);
    result = vkEnumerateDeviceExtensionProperties(gpu, name, &count, &properties[0]);
    if (result != VK_SUCCESS) {
      properties.clear();
      KRUST_LOG_ERROR << "Error getting physical device extensions: " << ResultToString(result) << endlog;
    }
  }
  return properties;
}

std::vector<VkLayerProperties> EnumerateDeviceLayerProperties(
  const VkPhysicalDevice &gpu) {
  uint32_t layerCount = 0;
  std::vector<VkLayerProperties> layerProperties;
  VkResult result = vkEnumerateDeviceLayerProperties(gpu, &layerCount, 0);
  if (result == VK_SUCCESS) {
    layerProperties.resize(layerCount);
    result = vkEnumerateDeviceLayerProperties(gpu, &layerCount, &layerProperties[0]);
    if (result != VK_SUCCESS) {
      layerProperties.resize(0);
    }
  }
  if (result != VK_SUCCESS) {
    KRUST_LOG_ERROR << "Failed to get GPU layer properties. Error: " << ResultToString(result) << endlog;
  }
  return layerProperties;
}

std::vector<VkQueueFamilyProperties> GetPhysicalDeviceQueueFamilyProperties(const VkPhysicalDevice gpu) {
  uint32_t count = 0;
  std::vector<VkQueueFamilyProperties> properties;

  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, 0);
  properties.resize(count);
  vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, &properties[0]);

  return properties;
}

std::vector<VkSurfaceFormatKHR> GetSurfaceFormatsKHR(PFN_vkGetPhysicalDeviceSurfaceFormatsKHR pGetSurfaceFormatsKHR,
                                                     VkPhysicalDevice gpuInterface,
                                                     VkSurfaceKHR surface)
{
  uint32_t numFormats = 0;
  std::vector<VkSurfaceFormatKHR> formats;
  VkResult result = pGetSurfaceFormatsKHR(gpuInterface, surface, &numFormats, nullptr);
  if(result == VK_SUCCESS)
  {
    formats.resize(numFormats);
    result = pGetSurfaceFormatsKHR(gpuInterface, surface, &numFormats, &formats[0]);
    if(result != VK_SUCCESS)
    {
      formats.clear();
    }
  }
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Unable to get surface formats. Error: " << ResultToString(result) << endlog;
  }
  return formats;
}

std::vector<VkImage> GetSwapChainImages(PFN_vkGetSwapchainImagesKHR getter,
                                        VkDevice gpuInterface,
                                        VkSwapchainKHR swapChain)
{
  KRUST_ASSERT1(getter, "Null pointer to function.");
  KRUST_ASSERT1(gpuInterface, "Invalid device.");
  KRUST_ASSERT1(swapChain, "Invalid swapchain.");

  auto images = std::vector<VkImage>();
  uint32_t numImages = 0;
  VkResult result = getter(gpuInterface, swapChain, &numImages, nullptr );
  if(result == VK_SUCCESS)
  {
    images.resize(numImages);
    result = getter(gpuInterface, swapChain, &numImages, &images[0]);
    if(result != VK_SUCCESS)
    {
      images.clear();
    }
  }
  if(result != VK_SUCCESS || images.size() < 1)
  {
    KRUST_LOG_ERROR << "Unable to get images for swapchain. Error: " << result << endlog;
  }
  return images;
}

std::vector<VkPresentModeKHR> GetSurfacePresentModesKHR(PFN_vkGetPhysicalDeviceSurfacePresentModesKHR pGetPhysicalDeviceSurfacePresentModesKHR,
                                                        const VkPhysicalDevice physicalDevice, const VkSurfaceKHR& surface)
{
  std::vector<VkPresentModeKHR> presentModes;
  uint32_t numModes = 0;
  VkResult result = pGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, 0);
  if(result == VK_SUCCESS)
  {
    presentModes.resize(numModes);
    result = pGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &numModes, &presentModes[0]);
    if(result != VK_SUCCESS)
    {
      presentModes.clear();
    }
  }
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to get surface present modes. Error: " << result << endlog;
  }

  return presentModes;
}

const char* FormatToString(const VkFormat format)
{
  const char *string = "<<Unknown Format>>";
  switch (format) {
    case VK_FORMAT_UNDEFINED: { string = "VK_FORMAT_UNDEFINED"; break; }
    case VK_FORMAT_R4G4_UNORM_PACK8: { string = "VK_FORMAT_R4G4_UNORM_PACK8"; break; }
    case VK_FORMAT_R4G4B4A4_UNORM_PACK16: { string = "VK_FORMAT_R4G4B4A4_UNORM_PACK16"; break; }
    case VK_FORMAT_B4G4R4A4_UNORM_PACK16: { string = "VK_FORMAT_B4G4R4A4_UNORM_PACK16"; break; }
    case VK_FORMAT_R5G6B5_UNORM_PACK16: { string = "VK_FORMAT_R5G6B5_UNORM_PACK16"; break; }
    case VK_FORMAT_B5G6R5_UNORM_PACK16: { string = "VK_FORMAT_B5G6R5_UNORM_PACK16"; break; }
    case VK_FORMAT_R5G5B5A1_UNORM_PACK16: { string = "VK_FORMAT_R5G5B5A1_UNORM_PACK16"; break; }
    case VK_FORMAT_B5G5R5A1_UNORM_PACK16: { string = "VK_FORMAT_B5G5R5A1_UNORM_PACK16"; break; }
    case VK_FORMAT_A1R5G5B5_UNORM_PACK16: { string = "VK_FORMAT_A1R5G5B5_UNORM_PACK16"; break; }
    case VK_FORMAT_R8_UNORM: { string = "VK_FORMAT_R8_UNORM"; break; }
    case VK_FORMAT_R8_SNORM: { string = "VK_FORMAT_R8_SNORM"; break; }
    case VK_FORMAT_R8_USCALED: { string = "VK_FORMAT_R8_USCALED"; break; }
    case VK_FORMAT_R8_SSCALED: { string = "VK_FORMAT_R8_SSCALED"; break; }
    case VK_FORMAT_R8_UINT: { string = "VK_FORMAT_R8_UINT"; break; }
    case VK_FORMAT_R8_SINT: { string = "VK_FORMAT_R8_SINT"; break; }
    case VK_FORMAT_R8_SRGB: { string = "VK_FORMAT_R8_SRGB"; break; }
    case VK_FORMAT_R8G8_UNORM: { string = "VK_FORMAT_R8G8_UNORM"; break; }
    case VK_FORMAT_R8G8_SNORM: { string = "VK_FORMAT_R8G8_SNORM"; break; }
    case VK_FORMAT_R8G8_USCALED: { string = "VK_FORMAT_R8G8_USCALED"; break; }
    case VK_FORMAT_R8G8_SSCALED: { string = "VK_FORMAT_R8G8_SSCALED"; break; }
    case VK_FORMAT_R8G8_UINT: { string = "VK_FORMAT_R8G8_UINT"; break; }
    case VK_FORMAT_R8G8_SINT: { string = "VK_FORMAT_R8G8_SINT"; break; }
    case VK_FORMAT_R8G8_SRGB: { string = "VK_FORMAT_R8G8_SRGB"; break; }
    case VK_FORMAT_R8G8B8_UNORM: { string = "VK_FORMAT_R8G8B8_UNORM"; break; }
    case VK_FORMAT_R8G8B8_SNORM: { string = "VK_FORMAT_R8G8B8_SNORM"; break; }
    case VK_FORMAT_R8G8B8_USCALED: { string = "VK_FORMAT_R8G8B8_USCALED"; break; }
    case VK_FORMAT_R8G8B8_SSCALED: { string = "VK_FORMAT_R8G8B8_SSCALED"; break; }
    case VK_FORMAT_R8G8B8_UINT: { string = "VK_FORMAT_R8G8B8_UINT"; break; }
    case VK_FORMAT_R8G8B8_SINT: { string = "VK_FORMAT_R8G8B8_SINT"; break; }
    case VK_FORMAT_R8G8B8_SRGB: { string = "VK_FORMAT_R8G8B8_SRGB"; break; }
    case VK_FORMAT_B8G8R8_UNORM: { string = "VK_FORMAT_B8G8R8_UNORM"; break; }
    case VK_FORMAT_B8G8R8_SNORM: { string = "VK_FORMAT_B8G8R8_SNORM"; break; }
    case VK_FORMAT_B8G8R8_USCALED: { string = "VK_FORMAT_B8G8R8_USCALED"; break; }
    case VK_FORMAT_B8G8R8_SSCALED: { string = "VK_FORMAT_B8G8R8_SSCALED"; break; }
    case VK_FORMAT_B8G8R8_UINT: { string = "VK_FORMAT_B8G8R8_UINT"; break; }
    case VK_FORMAT_B8G8R8_SINT: { string = "VK_FORMAT_B8G8R8_SINT"; break; }
    case VK_FORMAT_B8G8R8_SRGB: { string = "VK_FORMAT_B8G8R8_SRGB"; break; }
    case VK_FORMAT_R8G8B8A8_UNORM: { string = "VK_FORMAT_R8G8B8A8_UNORM"; break; }
    case VK_FORMAT_R8G8B8A8_SNORM: { string = "VK_FORMAT_R8G8B8A8_SNORM"; break; }
    case VK_FORMAT_R8G8B8A8_USCALED: { string = "VK_FORMAT_R8G8B8A8_USCALED"; break; }
    case VK_FORMAT_R8G8B8A8_SSCALED: { string = "VK_FORMAT_R8G8B8A8_SSCALED"; break; }
    case VK_FORMAT_R8G8B8A8_UINT: { string = "VK_FORMAT_R8G8B8A8_UINT"; break; }
    case VK_FORMAT_R8G8B8A8_SINT: { string = "VK_FORMAT_R8G8B8A8_SINT"; break; }
    case VK_FORMAT_R8G8B8A8_SRGB: { string = "VK_FORMAT_R8G8B8A8_SRGB"; break; }
    case VK_FORMAT_B8G8R8A8_UNORM: { string = "VK_FORMAT_B8G8R8A8_UNORM"; break; }
    case VK_FORMAT_B8G8R8A8_SNORM: { string = "VK_FORMAT_B8G8R8A8_SNORM"; break; }
    case VK_FORMAT_B8G8R8A8_USCALED: { string = "VK_FORMAT_B8G8R8A8_USCALED"; break; }
    case VK_FORMAT_B8G8R8A8_SSCALED: { string = "VK_FORMAT_B8G8R8A8_SSCALED"; break; }
    case VK_FORMAT_B8G8R8A8_UINT: { string = "VK_FORMAT_B8G8R8A8_UINT"; break; }
    case VK_FORMAT_B8G8R8A8_SINT: { string = "VK_FORMAT_B8G8R8A8_SINT"; break; }
    case VK_FORMAT_B8G8R8A8_SRGB: { string = "VK_FORMAT_B8G8R8A8_SRGB"; break; }
    case VK_FORMAT_A8B8G8R8_UNORM_PACK32: { string = "VK_FORMAT_A8B8G8R8_UNORM_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_SNORM_PACK32: { string = "VK_FORMAT_A8B8G8R8_SNORM_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_USCALED_PACK32: { string = "VK_FORMAT_A8B8G8R8_USCALED_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: { string = "VK_FORMAT_A8B8G8R8_SSCALED_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_UINT_PACK32: { string = "VK_FORMAT_A8B8G8R8_UINT_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_SINT_PACK32: { string = "VK_FORMAT_A8B8G8R8_SINT_PACK32"; break; }
    case VK_FORMAT_A8B8G8R8_SRGB_PACK32: { string = "VK_FORMAT_A8B8G8R8_SRGB_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32: { string = "VK_FORMAT_A2R10G10B10_UNORM_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_SNORM_PACK32: { string = "VK_FORMAT_A2R10G10B10_SNORM_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_USCALED_PACK32: { string = "VK_FORMAT_A2R10G10B10_USCALED_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: { string = "VK_FORMAT_A2R10G10B10_SSCALED_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_UINT_PACK32: { string = "VK_FORMAT_A2R10G10B10_UINT_PACK32"; break; }
    case VK_FORMAT_A2R10G10B10_SINT_PACK32: { string = "VK_FORMAT_A2R10G10B10_SINT_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: { string = "VK_FORMAT_A2B10G10R10_UNORM_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_SNORM_PACK32: { string = "VK_FORMAT_A2B10G10R10_SNORM_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_USCALED_PACK32: { string = "VK_FORMAT_A2B10G10R10_USCALED_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: { string = "VK_FORMAT_A2B10G10R10_SSCALED_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_UINT_PACK32: { string = "VK_FORMAT_A2B10G10R10_UINT_PACK32"; break; }
    case VK_FORMAT_A2B10G10R10_SINT_PACK32: { string = "VK_FORMAT_A2B10G10R10_SINT_PACK32"; break; }
    case VK_FORMAT_R16_UNORM: { string = "VK_FORMAT_R16_UNORM"; break; }
    case VK_FORMAT_R16_SNORM: { string = "VK_FORMAT_R16_SNORM"; break; }
    case VK_FORMAT_R16_USCALED: { string = "VK_FORMAT_R16_USCALED"; break; }
    case VK_FORMAT_R16_SSCALED: { string = "VK_FORMAT_R16_SSCALED"; break; }
    case VK_FORMAT_R16_UINT: { string = "VK_FORMAT_R16_UINT"; break; }
    case VK_FORMAT_R16_SINT: { string = "VK_FORMAT_R16_SINT"; break; }
    case VK_FORMAT_R16_SFLOAT: { string = "VK_FORMAT_R16_SFLOAT"; break; }
    case VK_FORMAT_R16G16_UNORM: { string = "VK_FORMAT_R16G16_UNORM"; break; }
    case VK_FORMAT_R16G16_SNORM: { string = "VK_FORMAT_R16G16_SNORM"; break; }
    case VK_FORMAT_R16G16_USCALED: { string = "VK_FORMAT_R16G16_USCALED"; break; }
    case VK_FORMAT_R16G16_SSCALED: { string = "VK_FORMAT_R16G16_SSCALED"; break; }
    case VK_FORMAT_R16G16_UINT: { string = "VK_FORMAT_R16G16_UINT"; break; }
    case VK_FORMAT_R16G16_SINT: { string = "VK_FORMAT_R16G16_SINT"; break; }
    case VK_FORMAT_R16G16_SFLOAT: { string = "VK_FORMAT_R16G16_SFLOAT"; break; }
    case VK_FORMAT_R16G16B16_UNORM: { string = "VK_FORMAT_R16G16B16_UNORM"; break; }
    case VK_FORMAT_R16G16B16_SNORM: { string = "VK_FORMAT_R16G16B16_SNORM"; break; }
    case VK_FORMAT_R16G16B16_USCALED: { string = "VK_FORMAT_R16G16B16_USCALED"; break; }
    case VK_FORMAT_R16G16B16_SSCALED: { string = "VK_FORMAT_R16G16B16_SSCALED"; break; }
    case VK_FORMAT_R16G16B16_UINT: { string = "VK_FORMAT_R16G16B16_UINT"; break; }
    case VK_FORMAT_R16G16B16_SINT: { string = "VK_FORMAT_R16G16B16_SINT"; break; }
    case VK_FORMAT_R16G16B16_SFLOAT: { string = "VK_FORMAT_R16G16B16_SFLOAT"; break; }
    case VK_FORMAT_R16G16B16A16_UNORM: { string = "VK_FORMAT_R16G16B16A16_UNORM"; break; }
    case VK_FORMAT_R16G16B16A16_SNORM: { string = "VK_FORMAT_R16G16B16A16_SNORM"; break; }
    case VK_FORMAT_R16G16B16A16_USCALED: { string = "VK_FORMAT_R16G16B16A16_USCALED"; break; }
    case VK_FORMAT_R16G16B16A16_SSCALED: { string = "VK_FORMAT_R16G16B16A16_SSCALED"; break; }
    case VK_FORMAT_R16G16B16A16_UINT: { string = "VK_FORMAT_R16G16B16A16_UINT"; break; }
    case VK_FORMAT_R16G16B16A16_SINT: { string = "VK_FORMAT_R16G16B16A16_SINT"; break; }
    case VK_FORMAT_R16G16B16A16_SFLOAT: { string = "VK_FORMAT_R16G16B16A16_SFLOAT"; break; }
    case VK_FORMAT_R32_UINT: { string = "VK_FORMAT_R32_UINT"; break; }
    case VK_FORMAT_R32_SINT: { string = "VK_FORMAT_R32_SINT"; break; }
    case VK_FORMAT_R32_SFLOAT: { string = "VK_FORMAT_R32_SFLOAT"; break; }
    case VK_FORMAT_R32G32_UINT: { string = "VK_FORMAT_R32G32_UINT"; break; }
    case VK_FORMAT_R32G32_SINT: { string = "VK_FORMAT_R32G32_SINT"; break; }
    case VK_FORMAT_R32G32_SFLOAT: { string = "VK_FORMAT_R32G32_SFLOAT"; break; }
    case VK_FORMAT_R32G32B32_UINT: { string = "VK_FORMAT_R32G32B32_UINT"; break; }
    case VK_FORMAT_R32G32B32_SINT: { string = "VK_FORMAT_R32G32B32_SINT"; break; }
    case VK_FORMAT_R32G32B32_SFLOAT: { string = "VK_FORMAT_R32G32B32_SFLOAT"; break; }
    case VK_FORMAT_R32G32B32A32_UINT: { string = "VK_FORMAT_R32G32B32A32_UINT"; break; }
    case VK_FORMAT_R32G32B32A32_SINT: { string = "VK_FORMAT_R32G32B32A32_SINT"; break; }
    case VK_FORMAT_R32G32B32A32_SFLOAT: { string = "VK_FORMAT_R32G32B32A32_SFLOAT"; break; }
    case VK_FORMAT_R64_UINT: { string = "VK_FORMAT_R64_UINT"; break; }
    case VK_FORMAT_R64_SINT: { string = "VK_FORMAT_R64_SINT"; break; }
    case VK_FORMAT_R64_SFLOAT: { string = "VK_FORMAT_R64_SFLOAT"; break; }
    case VK_FORMAT_R64G64_UINT: { string = "VK_FORMAT_R64G64_UINT"; break; }
    case VK_FORMAT_R64G64_SINT: { string = "VK_FORMAT_R64G64_SINT"; break; }
    case VK_FORMAT_R64G64_SFLOAT: { string = "VK_FORMAT_R64G64_SFLOAT"; break; }
    case VK_FORMAT_R64G64B64_UINT: { string = "VK_FORMAT_R64G64B64_UINT"; break; }
    case VK_FORMAT_R64G64B64_SINT: { string = "VK_FORMAT_R64G64B64_SINT"; break; }
    case VK_FORMAT_R64G64B64_SFLOAT: { string = "VK_FORMAT_R64G64B64_SFLOAT"; break; }
    case VK_FORMAT_R64G64B64A64_UINT: { string = "VK_FORMAT_R64G64B64A64_UINT"; break; }
    case VK_FORMAT_R64G64B64A64_SINT: { string = "VK_FORMAT_R64G64B64A64_SINT"; break; }
    case VK_FORMAT_R64G64B64A64_SFLOAT: { string = "VK_FORMAT_R64G64B64A64_SFLOAT"; break; }
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32: { string = "VK_FORMAT_B10G11R11_UFLOAT_PACK32"; break; }
    case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: { string = "VK_FORMAT_E5B9G9R9_UFLOAT_PACK32"; break; }
    case VK_FORMAT_D16_UNORM: { string = "VK_FORMAT_D16_UNORM"; break; }
    case VK_FORMAT_X8_D24_UNORM_PACK32: { string = "VK_FORMAT_X8_D24_UNORM_PACK32"; break; }
    case VK_FORMAT_D32_SFLOAT: { string = "VK_FORMAT_D32_SFLOAT"; break; }
    case VK_FORMAT_S8_UINT: { string = "VK_FORMAT_S8_UINT"; break; }
    case VK_FORMAT_D16_UNORM_S8_UINT: { string = "VK_FORMAT_D16_UNORM_S8_UINT"; break; }
    case VK_FORMAT_D24_UNORM_S8_UINT: { string = "VK_FORMAT_D24_UNORM_S8_UINT"; break; }
    case VK_FORMAT_D32_SFLOAT_S8_UINT: { string = "VK_FORMAT_D32_SFLOAT_S8_UINT"; break; }
    case VK_FORMAT_BC1_RGB_UNORM_BLOCK: { string = "VK_FORMAT_BC1_RGB_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC1_RGB_SRGB_BLOCK: { string = "VK_FORMAT_BC1_RGB_SRGB_BLOCK"; break; }
    case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: { string = "VK_FORMAT_BC1_RGBA_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: { string = "VK_FORMAT_BC1_RGBA_SRGB_BLOCK"; break; }
    case VK_FORMAT_BC2_UNORM_BLOCK: { string = "VK_FORMAT_BC2_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC2_SRGB_BLOCK: { string = "VK_FORMAT_BC2_SRGB_BLOCK"; break; }
    case VK_FORMAT_BC3_UNORM_BLOCK: { string = "VK_FORMAT_BC3_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC3_SRGB_BLOCK: { string = "VK_FORMAT_BC3_SRGB_BLOCK"; break; }
    case VK_FORMAT_BC4_UNORM_BLOCK: { string = "VK_FORMAT_BC4_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC4_SNORM_BLOCK: { string = "VK_FORMAT_BC4_SNORM_BLOCK"; break; }
    case VK_FORMAT_BC5_UNORM_BLOCK: { string = "VK_FORMAT_BC5_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC5_SNORM_BLOCK: { string = "VK_FORMAT_BC5_SNORM_BLOCK"; break; }
    case VK_FORMAT_BC6H_UFLOAT_BLOCK: { string = "VK_FORMAT_BC6H_UFLOAT_BLOCK"; break; }
    case VK_FORMAT_BC6H_SFLOAT_BLOCK: { string = "VK_FORMAT_BC6H_SFLOAT_BLOCK"; break; }
    case VK_FORMAT_BC7_UNORM_BLOCK: { string = "VK_FORMAT_BC7_UNORM_BLOCK"; break; }
    case VK_FORMAT_BC7_SRGB_BLOCK: { string = "VK_FORMAT_BC7_SRGB_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK"; break; }
    case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: { string = "VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK"; break; }
    case VK_FORMAT_EAC_R11_UNORM_BLOCK: { string = "VK_FORMAT_EAC_R11_UNORM_BLOCK"; break; }
    case VK_FORMAT_EAC_R11_SNORM_BLOCK: { string = "VK_FORMAT_EAC_R11_SNORM_BLOCK"; break; }
    case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: { string = "VK_FORMAT_EAC_R11G11_UNORM_BLOCK"; break; }
    case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: { string = "VK_FORMAT_EAC_R11G11_SNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_4x4_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_4x4_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_5x4_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_5x4_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_5x5_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_5x5_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_6x5_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_6x5_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_6x6_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_6x6_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_8x5_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_8x5_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_8x6_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_8x6_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_8x8_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_8x8_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_10x5_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_10x5_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_10x6_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_10x6_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_10x8_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_10x8_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_10x10_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_10x10_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_12x10_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_12x10_SRGB_BLOCK"; break; }
    case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: { string = "VK_FORMAT_ASTC_12x12_UNORM_BLOCK"; break; }
    case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: { string = "VK_FORMAT_ASTC_12x12_SRGB_BLOCK"; break; }
    // Specials (not actually formats):
    case VK_FORMAT_RANGE_SIZE: { string = "VK_FORMAT_RANGE_SIZE"; break; }
    case VK_FORMAT_MAX_ENUM: {string = "VK_FORMAT_MAX_ENUM"; break;};
  };
  return string;
}

const char* KHRColorspaceToString(const VkColorSpaceKHR space)
{
  KRUST_ASSERT2(space <= VK_COLORSPACE_END_RANGE, "Out of range color space.");
  const char* string = "<<unkown colorspace>>";
  switch(space){
    case VK_COLORSPACE_SRGB_NONLINEAR_KHR: {string = "VK_COLORSPACE_SRGB_NONLINEAR_WS"; break; };
    case VK_COLORSPACE_RANGE_SIZE: {string = "<<invalid: VK_COLORSPACE_RANGE_SIZE>>"; break; };
    case VK_COLORSPACE_MAX_ENUM: {string = "<<invalid: VK_COLORSPACE_MAX_ENUM>>"; break; };
  }
  return string;
}

void LogVkPhysicalDeviceFeatures(const VkPhysicalDeviceFeatures &features)
{
  // Regenerate this block from enum using regex in vim:
  // %s/^[ \t]*VkBool32[ \t]*\([^;]*\).*$/    << "\\n\\t\1 = " << (0 != features.\1)/gc
  KRUST_LOG_INFO << "VkPhysicalDeviceFeatures(" << &features << "){"
    << "\n\trobustBufferAccess = " << (0 != features.robustBufferAccess)
    << "\n\tfullDrawIndexUint32 = " << (0 != features.fullDrawIndexUint32)
    << "\n\timageCubeArray = " << (0 != features.imageCubeArray)
    << "\n\tindependentBlend = " << (0 != features.independentBlend)
    << "\n\tgeometryShader = " << (0 != features.geometryShader)
    << "\n\ttessellationShader = " << (0 != features.tessellationShader)
    << "\n\tsampleRateShading = " << (0 != features.sampleRateShading)
    << "\n\tdualSrcBlend = " << (0 != features.dualSrcBlend)
    << "\n\tlogicOp = " << (0 != features.logicOp)
    << "\n\tmultiDrawIndirect = " << (0 != features.multiDrawIndirect)
    << "\n\tdrawIndirectFirstInstance = " << (0 != features.drawIndirectFirstInstance)
    << "\n\tdepthClamp = " << (0 != features.depthClamp)
    << "\n\tdepthBiasClamp = " << (0 != features.depthBiasClamp)
    << "\n\tfillModeNonSolid = " << (0 != features.fillModeNonSolid)
    << "\n\tdepthBounds = " << (0 != features.depthBounds)
    << "\n\twideLines = " << (0 != features.wideLines)
    << "\n\tlargePoints = " << (0 != features.largePoints)
    << "\n\talphaToOne = " << (0 != features.alphaToOne)
    << "\n\tmultiViewport = " << (0 != features.multiViewport)
    << "\n\tsamplerAnisotropy = " << (0 != features.samplerAnisotropy)
    << "\n\ttextureCompressionETC2 = " << (0 != features.textureCompressionETC2)
    << "\n\ttextureCompressionASTC_LDR = " << (0 != features.textureCompressionASTC_LDR)
    << "\n\ttextureCompressionBC = " << (0 != features.textureCompressionBC)
    << "\n\tocclusionQueryPrecise = " << (0 != features.occlusionQueryPrecise)
    << "\n\tpipelineStatisticsQuery = " << (0 != features.pipelineStatisticsQuery)
    << "\n\tvertexPipelineStoresAndAtomics = " << (0 != features.vertexPipelineStoresAndAtomics)
    << "\n\tfragmentStoresAndAtomics = " << (0 != features.fragmentStoresAndAtomics)
    << "\n\tshaderTessellationAndGeometryPointSize = " << (0 != features.shaderTessellationAndGeometryPointSize)
    << "\n\tshaderImageGatherExtended = " << (0 != features.shaderImageGatherExtended)
    << "\n\tshaderStorageImageExtendedFormats = " << (0 != features.shaderStorageImageExtendedFormats)
    << "\n\tshaderStorageImageMultisample = " << (0 != features.shaderStorageImageMultisample)
    << "\n\tshaderStorageImageReadWithoutFormat = " << (0 != features.shaderStorageImageReadWithoutFormat)
    << "\n\tshaderStorageImageWriteWithoutFormat = " << (0 != features.shaderStorageImageWriteWithoutFormat)
    << "\n\tshaderUniformBufferArrayDynamicIndexing = " << (0 != features.shaderUniformBufferArrayDynamicIndexing)
    << "\n\tshaderSampledImageArrayDynamicIndexing = " << (0 != features.shaderSampledImageArrayDynamicIndexing)
    << "\n\tshaderStorageBufferArrayDynamicIndexing = " << (0 != features.shaderStorageBufferArrayDynamicIndexing)
    << "\n\tshaderStorageImageArrayDynamicIndexing = " << (0 != features.shaderStorageImageArrayDynamicIndexing)
    << "\n\tshaderClipDistance = " << (0 != features.shaderClipDistance)
    << "\n\tshaderCullDistance = " << (0 != features.shaderCullDistance)
    << "\n\tshaderFloat64 = " << (0 != features.shaderFloat64)
    << "\n\tshaderInt64 = " << (0 != features.shaderInt64)
    << "\n\tshaderInt16 = " << (0 != features.shaderInt16)
    << "\n\tshaderResourceResidency = " << (0 != features.shaderResourceResidency)
    << "\n\tshaderResourceMinLod = " << (0 != features.shaderResourceMinLod)
    << "\n\tsparseBinding = " << (0 != features.sparseBinding)
    << "\n\tsparseResidencyBuffer = " << (0 != features.sparseResidencyBuffer)
    << "\n\tsparseResidencyImage2D = " << (0 != features.sparseResidencyImage2D)
    << "\n\tsparseResidencyImage3D = " << (0 != features.sparseResidencyImage3D)
    << "\n\tsparseResidency2Samples = " << (0 != features.sparseResidency2Samples)
    << "\n\tsparseResidency4Samples = " << (0 != features.sparseResidency4Samples)
    << "\n\tsparseResidency8Samples = " << (0 != features.sparseResidency8Samples)
    << "\n\tsparseResidency16Samples = " << (0 != features.sparseResidency16Samples)
    << "\n\tsparseResidencyAliased = " << (0 != features.sparseResidencyAliased)
    << "\n\tvariableMultisampleRate = " << (0 != features.variableMultisampleRate)
    << "\n\tinheritedQueries = " << (0 != features.inheritedQueries)
  << "\n};" << endlog;
}

void LogVkPhysicalDeviceLimits(const VkPhysicalDeviceLimits &limits)
{
  KRUST_DEBUG_LOG_INFO << "sizeof(VkPhysicalDeviceLimits) = " << sizeof(VkPhysicalDeviceLimits) << endlog;
  KRUST_LOG_INFO << "VkPhysicalDeviceLimits(" << &limits << "){"
    << "\n\tmaxImageDimension1D = " << limits.maxImageDimension1D
    << "\n\tmaxImageDimension2D = " << limits.maxImageDimension2D
    << "\n\tmaxImageDimension3D = " << limits.maxImageDimension3D
    << "\n\tmaxImageDimensionCube = " << limits.maxImageDimensionCube
    << "\n\tmaxImageArrayLayers = " << limits.maxImageArrayLayers
    << "\n\tmaxTexelBufferElements = " << limits.maxTexelBufferElements
    << "\n\tmaxUniformBufferRange = " << limits.maxUniformBufferRange
    << "\n\tmaxStorageBufferRange = " << limits.maxStorageBufferRange
    << "\n\tmaxPushConstantsSize = " << limits.maxPushConstantsSize
    << "\n\tmaxMemoryAllocationCount = " << limits.maxMemoryAllocationCount
    << "\n\tmaxSamplerAllocationCount = " << limits.maxSamplerAllocationCount
    << "\n\tbufferImageGranularity = " << limits.bufferImageGranularity
    << "\n\tsparseAddressSpaceSize = " << limits.sparseAddressSpaceSize
    << "\n\tmaxBoundDescriptorSets = " << limits.maxBoundDescriptorSets
    << "\n\tmaxPerStageDescriptorSamplers = " << limits.maxPerStageDescriptorSamplers
    << "\n\tmaxPerStageDescriptorUniformBuffers = " << limits.maxPerStageDescriptorUniformBuffers
    << "\n\tmaxPerStageDescriptorStorageBuffers = " << limits.maxPerStageDescriptorStorageBuffers
    << "\n\tmaxPerStageDescriptorSampledImages = " << limits.maxPerStageDescriptorSampledImages
    << "\n\tmaxPerStageDescriptorStorageImages = " << limits.maxPerStageDescriptorStorageImages
    << "\n\tmaxPerStageDescriptorInputAttachments = " << limits.maxPerStageDescriptorInputAttachments
    << "\n\tmaxPerStageResources = " << limits.maxPerStageResources
    << "\n\tmaxDescriptorSetSamplers = " << limits.maxDescriptorSetSamplers
    << "\n\tmaxDescriptorSetUniformBuffers = " << limits.maxDescriptorSetUniformBuffers
    << "\n\tmaxDescriptorSetUniformBuffersDynamic = " << limits.maxDescriptorSetUniformBuffersDynamic
    << "\n\tmaxDescriptorSetStorageBuffers = " << limits.maxDescriptorSetStorageBuffers
    << "\n\tmaxDescriptorSetStorageBuffersDynamic = " << limits.maxDescriptorSetStorageBuffersDynamic
    << "\n\tmaxDescriptorSetSampledImages = " << limits.maxDescriptorSetSampledImages
    << "\n\tmaxDescriptorSetStorageImages = " << limits.maxDescriptorSetStorageImages
    << "\n\tmaxDescriptorSetInputAttachments = " << limits.maxDescriptorSetInputAttachments
    << "\n\tmaxVertexInputAttributes = " << limits.maxVertexInputAttributes
    << "\n\tmaxVertexInputBindings = " << limits.maxVertexInputBindings
    << "\n\tmaxVertexInputAttributeOffset = " << limits.maxVertexInputAttributeOffset
    << "\n\tmaxVertexInputBindingStride = " << limits.maxVertexInputBindingStride
    << "\n\tmaxVertexOutputComponents = " << limits.maxVertexOutputComponents
    << "\n\tmaxTessellationGenerationLevel = " << limits.maxTessellationGenerationLevel
    << "\n\tmaxTessellationPatchSize = " << limits.maxTessellationPatchSize
    << "\n\tmaxTessellationControlPerVertexInputComponents = " << limits.maxTessellationControlPerVertexInputComponents
    << "\n\tmaxTessellationControlPerVertexOutputComponents = " << limits.maxTessellationControlPerVertexOutputComponents
    << "\n\tmaxTessellationControlPerPatchOutputComponents = " << limits.maxTessellationControlPerPatchOutputComponents
    << "\n\tmaxTessellationControlTotalOutputComponents = " << limits.maxTessellationControlTotalOutputComponents
    << "\n\tmaxTessellationEvaluationInputComponents = " << limits.maxTessellationEvaluationInputComponents
    << "\n\tmaxTessellationEvaluationOutputComponents = " << limits.maxTessellationEvaluationOutputComponents
    << "\n\tmaxGeometryShaderInvocations = " << limits.maxGeometryShaderInvocations
    << "\n\tmaxGeometryInputComponents = " << limits.maxGeometryInputComponents
    << "\n\tmaxGeometryOutputComponents = " << limits.maxGeometryOutputComponents
    << "\n\tmaxGeometryOutputVertices = " << limits.maxGeometryOutputVertices
    << "\n\tmaxGeometryTotalOutputComponents = " << limits.maxGeometryTotalOutputComponents
    << "\n\tmaxFragmentInputComponents = " << limits.maxFragmentInputComponents
    << "\n\tmaxFragmentOutputAttachments = " << limits.maxFragmentOutputAttachments
    << "\n\tmaxFragmentDualSrcAttachments = " << limits.maxFragmentDualSrcAttachments
    << "\n\tmaxFragmentCombinedOutputResources = " << limits.maxFragmentCombinedOutputResources
    << "\n\tmaxComputeSharedMemorySize = " << limits.maxComputeSharedMemorySize

    << "\n\tmaxComputeWorkGroupCount[0] = " << limits.maxComputeWorkGroupCount[0]
    << "\n\tmaxComputeWorkGroupCount[1] = " << limits.maxComputeWorkGroupCount[1]
    << "\n\tmaxComputeWorkGroupCount[2] = " << limits.maxComputeWorkGroupCount[2]
    << "\n\tmaxComputeWorkGroupInvocations = " << limits.maxComputeWorkGroupInvocations
    << "\n\tmaxComputeWorkGroupSize[0] = " << limits.maxComputeWorkGroupSize[0]
    << "\n\tmaxComputeWorkGroupSize[1] = " << limits.maxComputeWorkGroupSize[1]
    << "\n\tmaxComputeWorkGroupSize[2] = " << limits.maxComputeWorkGroupSize[2]

    << "\n\tsubPixelPrecisionBits = " << limits.subPixelPrecisionBits
    << "\n\tsubTexelPrecisionBits = " << limits.subTexelPrecisionBits
    << "\n\tmipmapPrecisionBits = " << limits.mipmapPrecisionBits
    << "\n\tmaxDrawIndexedIndexValue = " << limits.maxDrawIndexedIndexValue
    << "\n\tmaxDrawIndirectCount = " << limits.maxDrawIndirectCount
    << "\n\tmaxSamplerLodBias = " << limits.maxSamplerLodBias
    << "\n\tmaxSamplerAnisotropy = " << limits.maxSamplerAnisotropy
    << "\n\tmaxViewports = " << limits.maxViewports

    << "\n\tmaxViewportDimensions[0] = " << limits.maxViewportDimensions[0]
    << "\n\tmaxViewportDimensions[1] = " << limits.maxViewportDimensions[1]
    << "\n\tviewportBoundsRange[0] = " << limits.viewportBoundsRange[0]
    << "\n\tviewportBoundsRange[1] = " << limits.viewportBoundsRange[1]

    << "\n\tviewportSubPixelBits = " << limits.viewportSubPixelBits
    << "\n\tminMemoryMapAlignment = " << limits.minMemoryMapAlignment
    << "\n\tminTexelBufferOffsetAlignment = " << limits.minTexelBufferOffsetAlignment
    << "\n\tminUniformBufferOffsetAlignment = " << limits.minUniformBufferOffsetAlignment
    << "\n\tminStorageBufferOffsetAlignment = " << limits.minStorageBufferOffsetAlignment
    << "\n\tminTexelOffset = " << limits.minTexelOffset
    << "\n\tmaxTexelOffset = " << limits.maxTexelOffset
    << "\n\tminTexelGatherOffset = " << limits.minTexelGatherOffset
    << "\n\tmaxTexelGatherOffset = " << limits.maxTexelGatherOffset
    << "\n\tminInterpolationOffset = " << limits.minInterpolationOffset
    << "\n\tmaxInterpolationOffset = " << limits.maxInterpolationOffset
    << "\n\tsubPixelInterpolationOffsetBits = " << limits.subPixelInterpolationOffsetBits
    << "\n\tmaxFramebufferWidth = " << limits.maxFramebufferWidth
    << "\n\tmaxFramebufferHeight = " << limits.maxFramebufferHeight
    << "\n\tmaxFramebufferLayers = " << limits.maxFramebufferLayers
    << "\n\tframebufferColorSampleCounts = " << limits.framebufferColorSampleCounts
    << "\n\tframebufferDepthSampleCounts = " << limits.framebufferDepthSampleCounts
    << "\n\tframebufferStencilSampleCounts = " << limits.framebufferStencilSampleCounts
    << "\n\tframebufferNoAttachmentsSampleCounts = " << limits.framebufferNoAttachmentsSampleCounts
    << "\n\tmaxColorAttachments = " << limits.maxColorAttachments
    << "\n\tsampledImageColorSampleCounts = " << limits.sampledImageColorSampleCounts
    << "\n\tsampledImageIntegerSampleCounts = " << limits.sampledImageIntegerSampleCounts
    << "\n\tsampledImageDepthSampleCounts = " << limits.sampledImageDepthSampleCounts
    << "\n\tsampledImageStencilSampleCounts = " << limits.sampledImageStencilSampleCounts
    << "\n\tstorageImageSampleCounts = " << limits.storageImageSampleCounts
    << "\n\tmaxSampleMaskWords = " << limits.maxSampleMaskWords
    << "\n\ttimestampComputeAndGraphics = " << limits.timestampComputeAndGraphics
    << "\n\ttimestampPeriod = " << limits.timestampPeriod
    << "\n\tmaxClipDistances = " << limits.maxClipDistances
    << "\n\tmaxCullDistances = " << limits.maxCullDistances
    << "\n\tmaxCombinedClipAndCullDistances = " << limits.maxCombinedClipAndCullDistances
    << "\n\tdiscreteQueuePriorities = " << limits.discreteQueuePriorities

    << "\n\tpointSizeRange[0] = " << limits.pointSizeRange[0]
    << "\n\tpointSizeRange[1] = " << limits.pointSizeRange[1]
    << "\n\tlineWidthRange[0] = " << limits.lineWidthRange[0]
    << "\n\tlineWidthRange[21 = " << limits.lineWidthRange[1]

    << "\n\tpointSizeGranularity = " << limits.pointSizeGranularity
    << "\n\tlineWidthGranularity = " << limits.lineWidthGranularity
    << "\n\tstrictLines = " << limits.strictLines
    << "\n\tstandardSampleLocations = " << limits.standardSampleLocations
    << "\n\toptimalBufferCopyOffsetAlignment = " << limits.optimalBufferCopyOffsetAlignment
    << "\n\toptimalBufferCopyRowPitchAlignment = " << limits.optimalBufferCopyRowPitchAlignment
    << "\n\tnonCoherentAtomSize = " << limits.nonCoherentAtomSize
  << "\n}" << endlog;
}

void LogVkSurfaceCapabilitiesKHR(const VkSurfaceCapabilitiesKHR &capabilities,
                                 LogChannel &logChannel, LogLevel level)
{
#if !defined(KRUST_BUILD_CONFIG_DISABLE_LOGGING)
  LogBuilder message(logChannel, level);
  message << "VkSurfaceCapabilitiesKHR(" << &capabilities << "){"
    << "\n\tminImageCount = " << capabilities.minImageCount
    << "\n\tmaxImageCount = " << capabilities.maxImageCount << (capabilities.maxImageCount == 0 ? " [0 => UNLIMITED]" : "")
    << "\n\tcurrentExtent = " << capabilities.currentExtent
    << "\n\tminImageExtent = " << capabilities.minImageExtent
    << "\n\tmaxImageExtent = " << capabilities.maxImageExtent
    << "\n\tmaxImageArrayLayers = " << capabilities.maxImageArrayLayers
    << "\n\tsupportedTransforms = " << capabilities.supportedTransforms
    << "\n\tcurrentTransform = " << SurfaceTransformToString(capabilities.currentTransform)
    << "\n\tsupportedCompositeAlpha = " << capabilities.supportedCompositeAlpha
    << "\n\tsupportedUsageFlags = " << capabilities.supportedUsageFlags
  << "\n}" << endlog;
#endif // !defined(KRUST_BUILD_CONFIG_DISABLE_LOGGING)
}

} /* namespace IO */
} /* namespace Krust */
