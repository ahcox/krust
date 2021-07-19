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

#include "application-graphics-swapchain.h"
#include "application.h"

namespace Krust {
namespace IO {

ApplicationGraphicsSwapchain::ApplicationGraphicsSwapchain(Application& app) : ApplicationComponent(app) {
}

bool ApplicationGraphicsSwapchain::Init(Application& app)
{
  // Build a depth buffer to be shared by all entries in the swapchain of color
  // images:
  if(!InitDepthBuffer(app, mDepthFormat))
  {
    return false;
  }

  return true;
}

bool ApplicationGraphicsSwapchain::InitDepthBuffer(Application& app, VkFormat depthFormat)
{

  // Create an image object for the depth buffer upfront so we can query the
  // amount of storage required for it from the Vulkan implementation:
  const unsigned width = app.mWindow->GetPlatformWindow().GetWidth();
  const unsigned height = app.mWindow->GetPlatformWindow().GetHeight();

  ImagePtr depthImage = Image::New(*app.mGpuInterface, CreateDepthImageInfo(app.mDefaultPresentQueueFamily, depthFormat, width, height));

  if(!depthImage.Get())
  {
    return false;
  }

  // Work out how much memory the depth image requires:
  VkMemoryRequirements depthMemoryRequirements;
  vkGetImageMemoryRequirements(*app.mGpuInterface, *depthImage, &depthMemoryRequirements);
  KRUST_LOG_INFO << "Depth buffer memory requirements: (Size = " << depthMemoryRequirements.size << ", Alignment = " << depthMemoryRequirements.alignment << ", Flags = " << depthMemoryRequirements.memoryTypeBits << ")." << endlog;

  // Work out which memory type we can use:
  ConditionalValue<uint32_t> memoryType = FindFirstMemoryTypeWithProperties(app.mGpuMemoryProperties, depthMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(!memoryType)
  {
    KRUST_LOG_ERROR << "No memory suitable for depth buffer." << endlog;
    return false;
  }

  // Allocate the memory to back the depth image:
  auto depthAllocationInfo = MemoryAllocateInfo(depthMemoryRequirements.size,
    memoryType.GetValue());
  DeviceMemoryPtr depthMemory{ DeviceMemory::New(*app.mGpuInterface, depthAllocationInfo) };

  // Tie the memory to the image:
  depthImage->BindMemory(*depthMemory, 0);

  // Create a view for the depth buffer image:
  VkImageView depthView = CreateDepthImageView(*app.mGpuInterface, *depthImage, depthFormat);
  if(!depthView)
  {
    return false;
  }

  mDepthBufferView = depthView;
  mDepthBufferImage = depthImage;
  mDepthBufferMemory = depthMemory;

  // Transition the depth buffer to the ideal layout:
  auto postPresentImageMemoryBarrier = ImageMemoryBarrier();
  postPresentImageMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
  postPresentImageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
  postPresentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  postPresentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  postPresentImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  postPresentImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  postPresentImageMemoryBarrier.image = *mDepthBufferImage,
  postPresentImageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};

  const auto depthLayoutResult = ApplyImageBarrierBlocking(*app.mGpuInterface, *mDepthBufferImage, app.mDefaultQueue, *app.mCommandPool, postPresentImageMemoryBarrier);
  if (VK_SUCCESS != depthLayoutResult)
  {
    KRUST_LOG_ERROR << "Failed to change depth image layout: " << ResultToString(depthLayoutResult) << Krust::endlog;
    return false;
  }
  return true;
}

bool ApplicationGraphicsSwapchain::DeInit(Application& app)
{
  if(mDepthBufferView)
  {
    vkDestroyImageView(*app.mGpuInterface, mDepthBufferView, Krust::GetAllocationCallbacks());
  }

  mDepthBufferImage.Reset(nullptr);  
  mDepthBufferMemory.Reset(nullptr);

  for(auto fb : mSwapChainFramebuffers)
  {
    vkDestroyFramebuffer(*app.mGpuInterface, fb, Krust::GetAllocationCallbacks());
  }

  for(unsigned i = 0; i < mRenderPasses.size(); ++i)
  {
    vkDestroyRenderPass(*app.mGpuInterface, mRenderPasses[i], Krust::GetAllocationCallbacks());
  }
  return true;
}
void ApplicationGraphicsSwapchain::OnResize(Application& app, unsigned width, unsigned height)
{

}

} /* namespace IO */
} /* namespace Krust */
