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

/// @file Utility code for Vulkan.

#include "vulkan-helpers.h"

// Internal includes
#include "krust/public-api/compiler.h"
#include "krust/public-api/krust-assertions.h"
#include <krust/public-api/logging.h>
#include <krust/public-api/vulkan-logging.h>

// External includes:
#include <cstring>
/// @todo Move and improve this test.
#if !defined(NDEBUG) && !defined(_WIN64) && !defined(_WIN32)
#define KRUST_DEBUGGING_ON_LINUX
#include <csignal>
#endif

namespace Krust
{
namespace IO {

PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char *const name) {
  KRUST_ASSERT1(name, "Must pass a name to look up.");
  const PFN_vkVoidFunction address = vkGetInstanceProcAddr(instance, name);
  if (!address) {
    KRUST_LOG_ERROR << "Failed to get the address of instance proc: " << name << endlog;
  }
  return address;
}

PFN_vkVoidFunction GetDeviceProcAddr(VkDevice device, const char *const name) {
  KRUST_ASSERT1(name, "Must pass a name to look up.");
  const PFN_vkVoidFunction address = vkGetDeviceProcAddr(device, name);
  if (!address) {
    KRUST_LOG_ERROR << "Failed to get the address of device proc: " << name << endlog;
  }
  return address;
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
    ++error_count; //< Just a line to set a breakpoint on to watch errors. When tracking down the call-site for validation error logs, break here.
#if defined(KRUST_DEBUGGING_ON_LINUX)
    raise(SIGTRAP);
#endif
  }
  if(flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
  {
    ++warning_count; //< Just a line to set a breakpoint on to watch warnings.
  }

  // Log messages printed here will start with VK_ERROR, VK_DEBUG, etc.
  KRUST_DEBUG_LOG_WARN << "<< VK_" << MessageFlagsToLevel(flags) << " [" << layerPrefix << "] " << message << ". [code = " << code << ", location = " << location << "] >>" << endlog;
  // Return 0 to allow the system to keep going (may fail disasterously anyway):
  return 0;
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
