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
/// @todo Commit functions with dedicated error handling to use the Krust deferred error handling mechanism or move them to their own file of weakly-coupled helpers (these came from krust-io and were written when there was an idea that someone might want to use krust-io without krust core. E.g. EnumeratePhysicalDevices().  

// Compilation unit header:
#include "vulkan-utils.h"

// Internal includes:
#include "krust/public-api/logging.h"
#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/vulkan_struct_init.h"
#include "krust/public-api/vulkan-objects.h"
#include "krust/internal/krust-internal.h"

// External includes:
#include <algorithm>
#include <cstring>

namespace Krust
{

VkImageCreateInfo CreateDepthImageInfo(const uint32_t presentQueueFamily, const VkFormat depthFormat, const uint32_t width, const uint32_t height)
{
  KRUST_ASSERT2(IsDepthFormat(depthFormat), "Format not usable for a depth buffer.");
  auto info = ImageCreateInfo();
  info.flags = 0,
    info.imageType = VK_IMAGE_TYPE_2D,
    info.format = depthFormat,
    info.extent.width = width,
    info.extent.height = height,
    info.extent.depth = 1,
    info.mipLevels = 1,
    info.arrayLayers = 1,
    info.samples = VK_SAMPLE_COUNT_1_BIT,
    info.tiling = VK_IMAGE_TILING_OPTIMAL,
    info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    info.queueFamilyIndexCount = 1,
    info.pQueueFamilyIndices = &presentQueueFamily,
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // But I want to get into: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  return info;
}

VkImageView CreateDepthImageView(VkDevice device, VkImage image, const VkFormat format)
{
  auto viewInfo = ImageViewCreateInfo();
    viewInfo.flags = 0, //< No flags needed [Could set VK_IMAGE_VIEW_CREATE_READ_ONLY_DEPTH_BIT or VK_IMAGE_VIEW_CREATE_READ_ONLY_STENCIL_BIT for read-only depth buffer.]
    viewInfo.image = image,
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D,
    viewInfo.format = format,
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT | VK_IMAGE_ASPECT_DEPTH_BIT,
    viewInfo.subresourceRange.baseMipLevel = 0,
    viewInfo.subresourceRange.levelCount = 1,
    viewInfo.subresourceRange.baseArrayLayer = 0,
    viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  const VkResult result = vkCreateImageView(device, &viewInfo, Krust::Internal::sAllocator, &imageView);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create image view for depth buffer. Error: " << result << endlog;
    imageView = 0;
  }
  return imageView;
}


VkResult ApplyImageBarrierBlocking(
  Krust::Device& device, VkImage image, VkQueue queue, Krust::CommandPool& pool,
  const VkImageMemoryBarrier& barrier)
{
  auto commandBuffer = CommandBuffer::New(pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  auto commandBufferInheritanceInfo = CommandBufferInheritanceInfo();
    commandBufferInheritanceInfo.renderPass = nullptr,
    commandBufferInheritanceInfo.subpass = 0,
    commandBufferInheritanceInfo.framebuffer = nullptr,
    commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE,
    commandBufferInheritanceInfo.queryFlags = 0,
    commandBufferInheritanceInfo.pipelineStatistics = 0;

  auto bufferBeginInfo = CommandBufferBeginInfo(
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    &commandBufferInheritanceInfo);

    const VkResult beginBufferResult = vkBeginCommandBuffer(
      *commandBuffer,
      &bufferBeginInfo);

    if(VK_SUCCESS != beginBufferResult)
    {
      KRUST_LOG_ERROR << "Failed to begin command buffer. Error: " << beginBufferResult << Krust::endlog;
      return beginBufferResult;
    }

    vkCmdPipelineBarrier(
      *commandBuffer,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      0, // VkDependencyFlags
      0, nullptr,
      0, nullptr,
      1, &barrier);

    VK_CALL_RET_RES(vkEndCommandBuffer, *commandBuffer);

    // Submit the buffer and then wait for it to complete:

    constexpr VkPipelineStageFlags pipelineFlags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    auto submitInfo = SubmitInfo(
      0, nullptr,
      &pipelineFlags,
      1, commandBuffer->GetVkCommandBufferAddress(),
      0, nullptr);

    // Execute command buffer on main queue:
    FencePtr fence {Fence::New(device, 0)};
    VK_CALL(vkQueueSubmit, queue, 1, &submitInfo, *fence);
    const VkResult fenceWaitResult = vkWaitForFences(device, 1, fence->GetVkFenceAddress(), true, 1000000000); // Wait one second.
    if(VK_SUCCESS != fenceWaitResult)
    {
      KRUST_LOG_ERROR << "Wait for queue submit of image memory barrier did not succeed: " << fenceWaitResult << Krust::endlog;
      return fenceWaitResult;
    }

  return VK_SUCCESS;
}



ConditionalValue<uint32_t>
FindFirstMemoryTypeWithProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, const uint32_t candidateTypeBitset, const VkMemoryPropertyFlags properties)
{
  const uint32_t count = std::min(uint32_t(VK_MAX_MEMORY_TYPES), memoryProperties.memoryTypeCount);
  for(uint32_t memoryType = 0; memoryType < count; ++memoryType)
  {
    // Check whether the memory type is in the bitset of allowable options:
    if(candidateTypeBitset & (1u << memoryType))
    {
      // Check whether all the requested properties are set for the memory type:
      if((memoryProperties.memoryTypes[memoryType].propertyFlags & properties) == properties)
      {
        return ConditionalValue<uint32_t>(memoryType, true);
      }
    }
  }
  KRUST_LOG_WARN << "No suitable memory type found with the requested properties (" << properties << ") among the allowed types in the flag set (" << candidateTypeBitset << ")." << endlog;
  return ConditionalValue<uint32_t>(0, false);
}

ConditionalValue<uint32_t>
FindMemoryTypeMatchingProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, const uint32_t candidateTypeBitset, const VkMemoryPropertyFlags properties)
{
  const uint32_t count = std::min(uint32_t(VK_MAX_MEMORY_TYPES), memoryProperties.memoryTypeCount);
  for(uint32_t memoryType = 0; memoryType < count; ++memoryType)
  {
    // Check whether the memory type is in the bitset of allowable options:
    if(candidateTypeBitset & (1u << memoryType))
    {
      // Check whether all the requested properties are set for the memory type:
      if(memoryProperties.memoryTypes[memoryType].propertyFlags  == properties)
      {
        return ConditionalValue<uint32_t>(memoryType, true);
      }
    }
  }
  KRUST_LOG_WARN << "No suitable memory type found with the requested properties (" << properties << ") among the allowed types in the flag set (" << candidateTypeBitset << ")." << endlog;
  return ConditionalValue<uint32_t>(0, false);
}

bool IsDepthFormat(const VkFormat format)
{
  // Use this to find out the new number if the assert below fires:
  // KRUST_SHOW_UINT_AS_WARNING(VK_FORMAT_RANGE_SIZE);
  // Vulkan headers have started shipping withotu these useful extra enumernats:
  // KRUST_COMPILE_ASSERT(VK_FORMAT_RANGE_SIZE == 185, "Number of formats changed. Check if any are depth ones.");

  // Note, a depth format could be added while a non-depth one was removed and not
  // trigger the assert above.
  bool isDepth = false;
  if(format == VK_FORMAT_D16_UNORM ||
     format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
     format == VK_FORMAT_D32_SFLOAT ||
     format == VK_FORMAT_D16_UNORM_S8_UINT ||
     format == VK_FORMAT_D24_UNORM_S8_UINT ||
     format == VK_FORMAT_D32_SFLOAT_S8_UINT)
  {
    isDepth = true;
  }

  return isDepth;
}

void LogResultError(const char* message, VkResult result)
{
  KRUST_LOG_ERROR << message << " Result: " << ResultToString(result) << "." << endlog;
}

const char* ResultToString(const VkResult result)
{
  const char *string = "<<Unknown Result Code>>";
  // Recreate with %s/\(VK_.*\) = .*,$/case \1: { string = "\1"; break; }/
  switch (result) {
    case VK_SUCCESS: { string = "VK_SUCCESS"; break; }
    case VK_NOT_READY: { string = "VK_NOT_READY"; break; }
    case VK_TIMEOUT: { string = "VK_TIMEOUT"; break; }
    case VK_EVENT_SET: { string = "VK_EVENT_SET"; break; }
    case VK_EVENT_RESET: { string = "VK_EVENT_RESET"; break; }
    case VK_INCOMPLETE: { string = "VK_INCOMPLETE"; break; }
    case VK_ERROR_OUT_OF_HOST_MEMORY: { string = "VK_ERROR_OUT_OF_HOST_MEMORY"; break; }
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: { string = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break; }
    case VK_ERROR_INITIALIZATION_FAILED: { string = "VK_ERROR_INITIALIZATION_FAILED"; break; }
    case VK_ERROR_DEVICE_LOST: { string = "VK_ERROR_DEVICE_LOST"; break; }
    case VK_ERROR_MEMORY_MAP_FAILED: { string = "VK_ERROR_MEMORY_MAP_FAILED"; break; }
    case VK_ERROR_LAYER_NOT_PRESENT: { string = "VK_ERROR_LAYER_NOT_PRESENT"; break; }
    case VK_ERROR_EXTENSION_NOT_PRESENT: { string = "VK_ERROR_EXTENSION_NOT_PRESENT"; break; }
    case VK_ERROR_FEATURE_NOT_PRESENT: { string = "VK_ERROR_FEATURE_NOT_PRESENT"; break; }
    case VK_ERROR_INCOMPATIBLE_DRIVER: { string = "VK_ERROR_INCOMPATIBLE_DRIVER"; break; }
    case VK_ERROR_TOO_MANY_OBJECTS: { string = "VK_ERROR_TOO_MANY_OBJECTS"; break; }
    case VK_ERROR_FORMAT_NOT_SUPPORTED: { string = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break; }
    case VK_ERROR_FRAGMENTED_POOL: { string = "VK_ERROR_FRAGMENTED_POOL"; break; }
    case VK_ERROR_UNKNOWN: { string = "VK_ERROR_UNKNOWN"; break; }
    case VK_ERROR_OUT_OF_POOL_MEMORY: { string = "VK_ERROR_OUT_OF_POOL_MEMORY"; break; }
    case VK_ERROR_INVALID_EXTERNAL_HANDLE: { string = "VK_ERROR_INVALID_EXTERNAL_HANDLE"; break; }
    case VK_ERROR_FRAGMENTATION: { string = "VK_ERROR_FRAGMENTATION"; break; }
    case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: { string = "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"; break; }
    case VK_ERROR_SURFACE_LOST_KHR: { string = "VK_ERROR_SURFACE_LOST_KHR"; break; }
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: { string = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break; }
    case VK_SUBOPTIMAL_KHR: { string = "VK_SUBOPTIMAL_KHR"; break; }
    case VK_ERROR_OUT_OF_DATE_KHR: { string = "VK_ERROR_OUT_OF_DATE_KHR"; break; }
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: { string = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break; }
    case VK_ERROR_VALIDATION_FAILED_EXT: { string = "VK_ERROR_VALIDATION_FAILED_EXT"; break; }
    case VK_ERROR_INVALID_SHADER_NV: { string = "VK_ERROR_INVALID_SHADER_NV"; break; }
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: { string = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break; }
    case VK_ERROR_NOT_PERMITTED_EXT: { string = "VK_ERROR_NOT_PERMITTED_EXT"; break; }
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: { string = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break; }
    case VK_THREAD_IDLE_KHR: { string = "VK_THREAD_IDLE_KHR"; break; }
    case VK_THREAD_DONE_KHR: { string = "VK_THREAD_DONE_KHR"; break; }
    case VK_OPERATION_DEFERRED_KHR: { string = "VK_OPERATION_DEFERRED_KHR"; break; }
    case VK_OPERATION_NOT_DEFERRED_KHR: { string = "VK_OPERATION_NOT_DEFERRED_KHR"; break; }
    case VK_PIPELINE_COMPILE_REQUIRED_EXT: { string = "VK_PIPELINE_COMPILE_REQUIRED_EXT"; break; }
    
    // case VK_RESULT_RANGE_SIZE: { string = "VK_RESULT_RANGE_SIZE"; break; }
    case VK_RESULT_MAX_ENUM: { string = "VK_RESULT_MAX_ENUM"; break; }
  };

  return string;
}

int SortMetric(const VkPresentModeKHR mode, const bool tearingAllowed)
{
  /// @todo Revise code that chooses a present mode. Maybe force app to have a list from best to worst it knows about.
  // The modes are enumerated from best to worst, with the caveat that the first
  // will tear. Therefore we use their values as the metric but penalize the tearing
  // mode if tearing is not allowed.
  int sortKey = mode;
  if(!tearingAllowed && (mode == VK_PRESENT_MODE_IMMEDIATE_KHR))
  {
    sortKey += 128;
  }
  return sortKey;
}

const char* SurfaceTransformToString(const VkSurfaceTransformFlagsKHR transform)
{
  const char* text = "<<<UNKNOWN VK_SURFACE_TRANSFORM>>>";
  switch(transform)
  {
    case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR"; break; }
    case VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR: { text = "VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR"; break; }
  };
  return text;
}

bool BuildFramebuffersForSwapChain(
  const VkDevice device,
  const std::vector<VkImageView>& swapChainImageViews,
  const VkImageView depthBufferView,
  const uint32_t surfaceWidth, const uint32_t surfaceHeight,
  const VkFormat colorFormat,
  const VkFormat depthFormat,
  const VkSampleCountFlagBits colorSamples,
  std::vector<VkRenderPass>& outRenderPasses,
  std::vector<VkFramebuffer>& outSwapChainFramebuffers)
{
  // Create RenderPass per swap chain image:
  VkAttachmentDescription attachments[2];
  attachments[0].flags = 0,
	  attachments[0].format = colorFormat,
	  attachments[0].samples = colorSamples,
	  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
	  attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	  attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	  attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	  attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	  attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    
	attachments[1].flags = 0,
		attachments[1].format = depthFormat,
		attachments[1].samples = colorSamples,
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference subpassColorAttachment;
    subpassColorAttachment.attachment = 0,
    subpassColorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthStencilAttachment;
    depthStencilAttachment.attachment = 1,
    depthStencilAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass;
  subpass.flags = 0,
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    // Which of the two attachments of the RenderPass will be read at the start of
    // the Subpass. Since we do a clear at the start, we read zero of them.
    subpass.inputAttachmentCount = 0,
    subpass.pInputAttachments = nullptr,
    subpass.colorAttachmentCount = 1,
    subpass.pColorAttachments = &subpassColorAttachment,
    // Attachments to resolve multisample color into be we are not doing AA:
    subpass.pResolveAttachments = nullptr,
    subpass.pDepthStencilAttachment = &depthStencilAttachment,
    // Non-modified attachments which must be preserved:
    subpass.preserveAttachmentCount = 0,
    subpass.pPreserveAttachments = nullptr;

  auto renderPassInfo = RenderPassCreateInfo();
    renderPassInfo.flags = 0,
    renderPassInfo.attachmentCount = 2, // Depth and color.
    renderPassInfo.pAttachments = attachments,
    renderPassInfo.subpassCount = 1,
    renderPassInfo.pSubpasses = &subpass,
    // List of subpass pairs where the execution of flagged stages of the second
    // must wait for flagged stages of the first to complete,
    // but there is only one subpass so that is irrelevant:
    renderPassInfo.dependencyCount = 0,
    renderPassInfo.pDependencies = nullptr;

  KRUST_ASSERT1(outRenderPasses.size() == 0, "Double init of primary view renderpasses.");
  for(unsigned i = 0; i < swapChainImageViews.size(); ++i)
  {
    VkRenderPass renderPass;
    VK_CALL_RET(vkCreateRenderPass, device, &renderPassInfo, Krust::Internal::sAllocator, &renderPass);
    outRenderPasses.push_back(renderPass);
  }

  // Create a FrameBuffer object for each image in the swapchain that we are
  // going to be presenting to the screen:

  // Populate the second attachment as our depth buffer:
  VkImageView colorDepthViews[2];
  colorDepthViews[1] = depthBufferView;

  outSwapChainFramebuffers.resize(swapChainImageViews.size());
  auto framebufferInfo = FramebufferCreateInfo();
    framebufferInfo.flags = 0,
    framebufferInfo.renderPass = nullptr, // Init this inside loop below.
    framebufferInfo.attachmentCount = 2,
    framebufferInfo.pAttachments = colorDepthViews,
    framebufferInfo.width = surfaceWidth,
    framebufferInfo.height = surfaceHeight,
    framebufferInfo.layers = 1;

  for(unsigned i = 0; i < swapChainImageViews.size(); ++i)
  {
    framebufferInfo.renderPass = outRenderPasses[i];
    colorDepthViews[0] = swapChainImageViews[i]; // Reset color buffer, but share depth.
    VK_CALL_RET(vkCreateFramebuffer, device, &framebufferInfo, Krust::Internal::sAllocator, &outSwapChainFramebuffers[i]);
  }

  return true;
}

const char* FormatToString(const VkFormat format)
{
  const char *string = "<<Unknown Format>>";
  // Recreate contents with: %s/\(VK_FORMAT.*\) = .*,$/case \1: { string = "\1"; break; }/
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
    case VK_FORMAT_G8B8G8R8_422_UNORM: { string = "VK_FORMAT_G8B8G8R8_422_UNORM"; break; }
    case VK_FORMAT_B8G8R8G8_422_UNORM: { string = "VK_FORMAT_B8G8R8G8_422_UNORM"; break; }
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM: { string = "VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM"; break; }
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM: { string = "VK_FORMAT_G8_B8R8_2PLANE_420_UNORM"; break; }
    case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM: { string = "VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM"; break; }
    case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM: { string = "VK_FORMAT_G8_B8R8_2PLANE_422_UNORM"; break; }
    case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM: { string = "VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM"; break; }
    case VK_FORMAT_R10X6_UNORM_PACK16: { string = "VK_FORMAT_R10X6_UNORM_PACK16"; break; }
    case VK_FORMAT_R10X6G10X6_UNORM_2PACK16: { string = "VK_FORMAT_R10X6G10X6_UNORM_2PACK16"; break; }
    case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16: { string = "VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16"; break; }
    case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16: { string = "VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16"; break; }
    case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16: { string = "VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16"; break; }
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16: { string = "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16"; break; }
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16: { string = "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16"; break; }
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16: { string = "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16"; break; }
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16: { string = "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16"; break; }
    case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16: { string = "VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16"; break; }
    case VK_FORMAT_R12X4_UNORM_PACK16: { string = "VK_FORMAT_R12X4_UNORM_PACK16"; break; }
    case VK_FORMAT_R12X4G12X4_UNORM_2PACK16: { string = "VK_FORMAT_R12X4G12X4_UNORM_2PACK16"; break; }
    case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16: { string = "VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16"; break; }
    case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16: { string = "VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16"; break; }
    case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16: { string = "VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16"; break; }
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16: { string = "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16"; break; }
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16: { string = "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16"; break; }
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16: { string = "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16"; break; }
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16: { string = "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16"; break; }
    case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16: { string = "VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16"; break; }
    case VK_FORMAT_G16B16G16R16_422_UNORM: { string = "VK_FORMAT_G16B16G16R16_422_UNORM"; break; }
    case VK_FORMAT_B16G16R16G16_422_UNORM: { string = "VK_FORMAT_B16G16R16G16_422_UNORM"; break; }
    case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM: { string = "VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM"; break; }
    case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM: { string = "VK_FORMAT_G16_B16R16_2PLANE_420_UNORM"; break; }
    case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM: { string = "VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM"; break; }
    case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM: { string = "VK_FORMAT_G16_B16R16_2PLANE_422_UNORM"; break; }
    case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM: { string = "VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM"; break; }
    case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: { string = "VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: { string = "VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG: { string = "VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG: { string = "VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: { string = "VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: { string = "VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG: { string = "VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG"; break; }
    case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: { string = "VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG"; break; }
    case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT: { string = "VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT"; break; }
    case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT: { string = "VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT"; break; }
    case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT: { string = "VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT"; break; }
    case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT: { string = "VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT"; break; }
    case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT: { string = "VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT"; break; }
    case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT: { string = "VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT"; break; }
    case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT: { string = "VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT"; break; }

    // Specials (not actually formats):
    //case VK_FORMAT_RANGE_SIZE: { string = "VK_FORMAT_RANGE_SIZE"; break; }
    case VK_FORMAT_MAX_ENUM: {string = "VK_FORMAT_MAX_ENUM"; break; };
  };
  return string;
}

const char* KHRColorspaceToString(const VkColorSpaceKHR space)
{
  // KRUST_ASSERT2(space <= VK_COLOR_SPACE_END_RANGE_KHR, "Out of range color space.");
  const char* string = "<<unkown colorspace>>";
  // recreate cases below with: %s/\(VK_.*\) = .*,$/case \1: { string = "\1"; break; }/
  switch (space) {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR: { string = "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR"; break; }
    case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: { string = "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT"; break; }
    case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: { string = "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT"; break; }
    case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT: { string = "VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT"; break; }
    case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT: { string = "VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT"; break; }
    case VK_COLOR_SPACE_BT709_LINEAR_EXT: { string = "VK_COLOR_SPACE_BT709_LINEAR_EXT"; break; }
    case VK_COLOR_SPACE_BT709_NONLINEAR_EXT: { string = "VK_COLOR_SPACE_BT709_NONLINEAR_EXT"; break; }
    case VK_COLOR_SPACE_BT2020_LINEAR_EXT: { string = "VK_COLOR_SPACE_BT2020_LINEAR_EXT"; break; }
    case VK_COLOR_SPACE_HDR10_ST2084_EXT: { string = "VK_COLOR_SPACE_HDR10_ST2084_EXT"; break; }
    case VK_COLOR_SPACE_DOLBYVISION_EXT: { string = "VK_COLOR_SPACE_DOLBYVISION_EXT"; break; }
    case VK_COLOR_SPACE_HDR10_HLG_EXT: { string = "VK_COLOR_SPACE_HDR10_HLG_EXT"; break; }
    case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT: { string = "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT"; break; }
    case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT: { string = "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT"; break; }
    case VK_COLOR_SPACE_PASS_THROUGH_EXT: { string = "VK_COLOR_SPACE_PASS_THROUGH_EXT"; break; }
    case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT: { string = "VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT"; break; }
    case VK_COLOR_SPACE_DISPLAY_NATIVE_AMD: { string = "VK_COLOR_SPACE_DISPLAY_NATIVE_AMD"; break; }

    // case VK_COLOR_SPACE_RANGE_SIZE_KHR: {string = "<<invalid: VK_COLORSPACE_RANGE_SIZE>>"; break; };
    case VK_COLOR_SPACE_MAX_ENUM_KHR: {string = "<<invalid: VK_COLORSPACE_MAX_ENUM>>"; break; };
  }
  return string;
}

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

bool FindLayer(const std::vector<VkLayerProperties> &layers, const char *const layer) {
  KRUST_ASSERT1(layer, "Name of extension to look up is missing.");
  for (auto potential : layers) {
    if (strcmp(layer, potential.layerName) == 0) {
      return true;
    }
  }
  KRUST_LOG_WARN << "Failed to find layer \"" << layer << "\"." << endlog;
  return false;
}

const char *MessageFlagsToLevel(const VkFlags flags)
{
  const char* level = "UNKNOWN";
  switch (flags)
  {
  case VK_DEBUG_REPORT_INFORMATION_BIT_EXT: { level = "INFO"; break; }
  case VK_DEBUG_REPORT_WARNING_BIT_EXT: { level = "WARN"; break; }
  case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT: { level = "PERF_WARN"; break; }
  case VK_DEBUG_REPORT_ERROR_BIT_EXT: { level = "ERROR"; break; }
  case VK_DEBUG_REPORT_DEBUG_BIT_EXT: { level = "DEBUG"; break; }
  }
  return level;
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

ScopedImageOwner::ScopedImageOwner(VkDevice dev, VkImage img) : device(dev), image(img)
{
  KRUST_ASSERT2(dev || !img, "Need a device so the destroy will work later.");
}

ScopedImageOwner::~ScopedImageOwner()
{
  if(image)
  {
    KRUST_ASSERT2(device, "No device so can't destroy.");
    vkDestroyImage(device, image, Krust::Internal::sAllocator);
  }
}

ScopedDeviceMemoryOwner::ScopedDeviceMemoryOwner(VkDevice dev,
                                                 VkDeviceMemory mem) :
  device(dev), memory(mem)
{
  KRUST_ASSERT2(dev || !mem, "Need a device so the free will work later.");
}

ScopedDeviceMemoryOwner::~ScopedDeviceMemoryOwner()
{
  if(memory)
  {
    KRUST_ASSERT2(device, "No device so can't free.");
    vkFreeMemory(device, memory, Krust::Internal::sAllocator);
  }
}

}
