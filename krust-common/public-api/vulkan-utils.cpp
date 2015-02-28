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

// Compilation unit header:
#include "vulkan-utils.h"

// Internal includes:
#include "krust-common/public-api/logging.h"
#include "krust-common/public-api/krust-assertions.h"
#include "krust-common.h" /// For KRUST_SHOW_UINT_AS_WARNING

// External includes:
#include <algorithm>

namespace Krust
{

VkImage CreateDepthImage(VkDevice gpuInterface, const uint32_t presentQueueFamily, const VkFormat depthFormat, uint32_t width, uint32_t height)
{
  KRUST_ASSERT2(gpuInterface, "Invalid device.");
  KRUST_ASSERT2(IsDepthFormat(depthFormat), "Format not useable for a depth buffer.");
  VkImageCreateInfo info;
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
	  info.pNext = nullptr,
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
	  info.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 
  VkImage depthImage = 0;
  const VkResult result = vkCreateImage(gpuInterface, &info, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &depthImage);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create depth image. Error: " << result << endlog;
    depthImage = 0;
  }

  return depthImage;
}

VkImageView CreateDepthImageView(VkDevice device, VkImage image, const VkFormat format)
{
	VkImageViewCreateInfo viewInfo;
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    viewInfo.pNext = nullptr,
    viewInfo.flags = 0, //< No flags needed [Could set VK_IMAGE_VIEW_CREATE_READ_ONLY_DEPTH_BIT or VK_IMAGE_VIEW_CREATE_READ_ONLY_STENCIL_BIT for read-only depth buffer.]
    viewInfo.image = image,
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D,
    viewInfo.format = format,
      viewInfo.components.r = VK_COMPONENT_SWIZZLE_ZERO,
      viewInfo.components.g = VK_COMPONENT_SWIZZLE_ZERO,
      viewInfo.components.b = VK_COMPONENT_SWIZZLE_ZERO,
      viewInfo.components.a = VK_COMPONENT_SWIZZLE_ZERO,
      viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
      viewInfo.subresourceRange.baseMipLevel = 0,
      viewInfo.subresourceRange.levelCount = 1,
      viewInfo.subresourceRange.baseArrayLayer = 0,
      viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;
  const VkResult result = vkCreateImageView(device, &viewInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &imageView);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create image view for depth buffer. Error: " << result << endlog;
    imageView = 0;
  }
  return imageView;
}

ConditionalValue<uint32_t>
FindMemoryTypeWithProperties(const VkPhysicalDeviceMemoryProperties& memoryProperties, uint32_t candidateTypeBitset, VkMemoryPropertyFlags properties)
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

bool IsDepthFormat(const VkFormat format)
{
  // Use this to find out the new number if the assert below fires:
  //KRUST_SHOW_UINT_AS_WARNING(VK_FORMAT_RANGE_SIZE);
  KRUST_COMPILE_ASSERT(VK_FORMAT_RANGE_SIZE == 185, "Number of formats changed. Check if any are depth ones.");

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
    case VK_ERROR_SURFACE_LOST_KHR: { string = "VK_ERROR_SURFACE_LOST_KHR"; break; }
    case VK_SUBOPTIMAL_KHR: { string = "VK_SUBOPTIMAL_KHR"; break; }
    case VK_ERROR_OUT_OF_DATE_KHR: { string = "VK_ERROR_OUT_OF_DATE_KHR"; break; }
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: { string = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break; }
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: { string = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break; }
    case VK_ERROR_VALIDATION_FAILED_EXT: { string = "VK_ERROR_VALIDATION_FAILED_EXT"; break; }
    case VK_RESULT_RANGE_SIZE: { string = "VK_RESULT_RANGE_SIZE"; break; }
    case VK_RESULT_MAX_ENUM: { string = "VK_RESULT_MAX_ENUM"; break; }
  };

  return string;
}

int SortMetric(const VkPresentModeKHR mode, const bool tearingAllowed)
{
  KRUST_COMPILE_ASSERT(VK_PRESENT_MODE_END_RANGE == 3,
    "Need to examine VkPresentModeKHR as it has changed since this function was "
    "written. Check if the natural ordering is still of decreasing goodness and if"
    "a new mode was added.");
  // The modes are enumerated from best to worst, with the caveat that the first
  // will tear. Therefore we use their values as the metric but penalise the tearing
  // mode if tearing is not allowed.
  int sortKey = mode;
  if(!tearingAllowed && (mode == VK_PRESENT_MODE_IMMEDIATE_KHR))
  {
    sortKey += int(VK_PRESENT_MODE_END_RANGE) + 1;
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

  VkRenderPassCreateInfo renderPassInfo;
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    renderPassInfo.pNext = nullptr,
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
    VK_CALL_RET(vkCreateRenderPass, device, &renderPassInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &renderPass);
    outRenderPasses.push_back(renderPass);
  }

  // Create a FrameBuffer object for each image in the swapchain that we are
  // going to be presenting to the screen:

  // Populate the second attachment as our depth buffer:
  VkImageView colorDepthViews[2];
  colorDepthViews[1] = depthBufferView;

  outSwapChainFramebuffers.resize(swapChainImageViews.size());
  VkFramebufferCreateInfo framebufferInfo;
     framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
    framebufferInfo.pNext = nullptr,
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
    VK_CALL_RET(vkCreateFramebuffer, device, &framebufferInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &outSwapChainFramebuffers[i]);
  }

  return true;
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
    vkDestroyImage(device, image, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
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
    vkFreeMemory(device, memory, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }
}

}
