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
#include "krust-io.h"
#include "krust-common/public-api/krust-common.h"
#include "krust-common/public-api/logging.h"
#include "krust-common/public-api/vulkan-utils.h"

namespace
{
/** Number of samples per framebuffer pixel. */
const VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;
}
/**
 * Draws an empty frame, clearing the framebuffer.
 */
class ClearApplication : public Krust::IO::Application
{
public:
  /**
   * Called by the default initialization once Krust is initialised and a window
   * has been created. Now is the time to do any additional setup.
   *
   * We will need a framebuffer, a render pass, and a command buffer, all tied
   * together.
   */
  virtual bool DoPostInit()
  {
    KRUST_LOG_DEBUG << "DoPostInit() entered." << Krust::endlog;

    // To begin recording a command buffer, we need to pass in a VkRenderPass, and
    // a VkFrameBuffer which are compatible with each other, so lets make one of each
    // of those now:

    if(!Krust::BuildFramebuffersForSwapChain(
      mGpuInterface,
      mSwapChainImageViews,
      mDepthBufferView,
      mDefaultWindow->GetPlatformWindow().GetWidth(),
      mDefaultWindow->GetPlatformWindow().GetHeight(),
      mFormat,
      mDepthFormat,
      NUM_SAMPLES,
      mRenderPasses,
      mSwapChainFramebuffers))
    {
      return false;
    }

    VkCommandBufferAllocateInfo bufferInfo;
      bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      bufferInfo.pNext = nullptr,
      bufferInfo.commandPool = mCommandPool,
      bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      bufferInfo.commandBufferCount = uint32_t(mSwapChainImageViews.size());

    KRUST_ASSERT1(mCommandBuffers.size() == 0, "Double init of command buffers.");
    mCommandBuffers.resize(mSwapChainImageViews.size());
    const VkResult bufferResult = vkAllocateCommandBuffers(mGpuInterface, &bufferInfo, &mCommandBuffers[0]);
    if(bufferResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to allocate command buffers. Error: " << bufferResult << Krust::endlog;
      return false;
    }

    // Build a command buffer per swapchain entry:
    VkCommandBufferInheritanceInfo commandBufferInheritanceInfo;
      commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
      commandBufferInheritanceInfo.pNext = nullptr,
      commandBufferInheritanceInfo.renderPass = nullptr,
      commandBufferInheritanceInfo.subpass = 0,
      commandBufferInheritanceInfo.framebuffer = nullptr,
      commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE,
      commandBufferInheritanceInfo.queryFlags = 0,
      commandBufferInheritanceInfo.pipelineStatistics = 0;

    VkCommandBufferBeginInfo bufferBeginInfo;
      bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      bufferBeginInfo.pNext = nullptr,
      bufferBeginInfo.flags = 0,
      bufferBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

    for(unsigned i = 0; i < mCommandBuffers.size(); ++i)
    {
      VkCommandBuffer commandBuffer = mCommandBuffers[i];
      VkImage framebufferImage = mSwapChainImages[i];

      const VkResult beginBufferResult = vkBeginCommandBuffer(commandBuffer,
                                                              &bufferBeginInfo);

      if(VK_SUCCESS != beginBufferResult)
      {
        KRUST_LOG_ERROR << "Failed to begin command buffer. Error: " << beginBufferResult << Krust::endlog;
        return false;
      }

      // Assume the image is returned from being presented and fix it up using
      // an image memory barrier:
      VkImageMemoryBarrier postPresentImageMemoryBarrier;
      postPresentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        postPresentImageMemoryBarrier.pNext = NULL,
        postPresentImageMemoryBarrier.srcAccessMask = 0,
        postPresentImageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        postPresentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        postPresentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        postPresentImageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        postPresentImageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        postPresentImageMemoryBarrier.image = framebufferImage,
        postPresentImageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &postPresentImageMemoryBarrier);

      VkClearValue clearValues[2];
      clearValues[0].color.float32[0] = 0.9f;
      clearValues[0].color.float32[1] = 0.7f;
      // Make it visually obvious that there are multiple framebuffers:
      clearValues[0].color.float32[2] = 0.2f + 0.2f * i;
      clearValues[0].color.float32[3] = 0.2f;
      clearValues[1].depthStencil.depth   = 1.0f;
      clearValues[1].depthStencil.stencil = 0;

      VkRect2D renderArea;
      renderArea.offset = { 0, 0 },
      renderArea.extent = {
        uint32_t(mDefaultWindow->GetPlatformWindow().GetWidth()), 
        uint32_t(mDefaultWindow->GetPlatformWindow().GetHeight())
      };

      VkRenderPassBeginInfo beginRenderPass;
      beginRenderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        beginRenderPass.pNext = nullptr,
        beginRenderPass.renderPass = mRenderPasses[i],
        beginRenderPass.framebuffer = mSwapChainFramebuffers[i],
        beginRenderPass.renderArea = renderArea,
        beginRenderPass.clearValueCount = 2U,
        beginRenderPass.pClearValues = clearValues;
      vkCmdBeginRenderPass(commandBuffer, &beginRenderPass, VK_SUBPASS_CONTENTS_INLINE);

      // We don't need to execute any explicit command to clear the screen in here.
      // The VK_ATTACHMENT_LOAD_OP_CLEAR of our subpass attachments clears implicitly.

      vkCmdEndRenderPass(commandBuffer);

      // Assume the framebuffer will be presented so insert an image memory
      // barrier here first:
      VkImageMemoryBarrier presentBarrier;
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        presentBarrier.pNext = NULL,
        presentBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        presentBarrier.image = framebufferImage,
        presentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

      vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                           nullptr, 0, nullptr, 1, &presentBarrier);

      VK_CALL_RET(vkEndCommandBuffer, commandBuffer);
    }

    return true;
  }

  bool DoPreDeInit()
  {
    // Destroy VK objects:
    for(unsigned i = 0; i < mRenderPasses.size(); ++i)
    {
      vkDestroyRenderPass(mGpuInterface, mRenderPasses[i], KRUST_DEFAULT_ALLOCATION_CALLBACKS);
    }

    return true;
  }

  /**
   * Called by the default run loop to repaint the screen.
   *
   * Debugging tips:
   * 1. Try adding VK_CALL(vkQueueWaitIdle, *mDefaultPresentQueue); at start and
   *    end of this function.
   */
  virtual void DoDrawFrame()
  {
    KRUST_LOG_INFO << "   -------------------------- Clear Example draw frame! currImage: " << mCurrentTargetImage << " (handle: " << mSwapChainImages[mCurrentTargetImage] << ")  --------------------------\n";

    const VkPipelineStageFlags pipelineFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkSubmitInfo submitInfo;
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      submitInfo.pNext = nullptr,
      submitInfo.waitSemaphoreCount = 1,
      submitInfo.pWaitSemaphores = &mSwapChainSemaphore,
      submitInfo.pWaitDstStageMask = &pipelineFlags,
      submitInfo.commandBufferCount = 1,
      // We have one command buffer per presentable image, so submit the right one:
      submitInfo.pCommandBuffers = &mCommandBuffers[mCurrentTargetImage],
      submitInfo.signalSemaphoreCount = 0,
      submitInfo.pSignalSemaphores = nullptr;

    KRUST_LOG_DEBUG << "Submitting command buffer " << mCurrentTargetImage << "(" << mCommandBuffers[mCurrentTargetImage] << ")." << Krust::endlog;
    // Execute command buffer on main queue:
    VK_CALL(vkQueueSubmit, *mDefaultGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  }

  ~ClearApplication()
  {
    KRUST_LOG_DEBUG << "ClearApplication::~ClearApplication()" << Krust::endlog;
  }
};

int main()
{
  std::cout << "Krust Clear Example 0.9.0\n";

  ClearApplication application;
  application.SetName("Clear");
  application.SetVersion(1);

  const int status =  application.Run();

  KRUST_LOG_INFO << "Exiting cleanly with code " << status << ".\n";
  return status;
}
