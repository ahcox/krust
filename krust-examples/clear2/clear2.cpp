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
#include "krust-io/public-api/krust-io.h"
#include "krust-io/public-api/application-graphics-swapchain.h"
#include "krust/public-api/krust.h"

namespace kr = Krust;

namespace
{
/** Number of samples per framebuffer pixel. */
constexpr VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;
constexpr VkAllocationCallbacks* ALLOCATION_CALLBACKS = nullptr;
}
/**
 * Draws an empty frame, clearing the framebuffer and changing the
 * clear color each frame.
 */
class Clear2Application : public Krust::IO::Application
{
  // Optional components to extend the swapchain for access by graphics pipelines.
  Krust::IO::ApplicationGraphicsSwapchain mGraphicsSwapchain;

public:
  Clear2Application() : mGraphicsSwapchain(*this) {}

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
      *mGpuInterface,
      mSwapChainImageViews,
      mGraphicsSwapchain.mDepthBufferView,
      mWindow->GetPlatformWindow().GetWidth(),
      mWindow->GetPlatformWindow().GetHeight(),
      mFormat,
      mGraphicsSwapchain.mDepthFormat,
      NUM_SAMPLES,
      mGraphicsSwapchain.mRenderPasses,
      mGraphicsSwapchain.mSwapChainFramebuffers,
      mSwapChainFences))
    {
      return false;
    }

    // Allocate a command buffer per swapchain entry:
    KRUST_ASSERT1(mCommandBuffers.size() == 0, "Double init of command buffers.");
    kr::CommandBuffer::Allocate(*mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, unsigned(mSwapChainImageViews.size()), mCommandBuffers);

    // We will actually build the command buffers at draw time as we want to vary
    // the clear color.

    return true;
  }

  bool DoPreDeInit()
  {
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
    static unsigned frameNumber = 0;
    KRUST_LOG_INFO << "   ------------ Clear Example 2 draw frame! frame: " << frameNumber++ << ". currImage: " << mCurrentTargetImage << ". handle: " << mSwapChainImages[mCurrentTargetImage] << "  ------------\n";

    auto submitFence = mSwapChainFences[mCurrentTargetImage];
    const VkResult fenceWaitResult = vkWaitForFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress(), true, 1000000000); // Wait one second.
    if(VK_SUCCESS != fenceWaitResult)
    {
      KRUST_LOG_ERROR << "Wait for queue submit of main commandbuffer did not succeed: " << fenceWaitResult << Krust::endlog;
    }
    vkResetFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress());

    // Build a command buffer for the current swapchain entry:

    kr::CommandBufferPtr commandBuffer = mCommandBuffers[mCurrentTargetImage];
    VkImage framebufferImage = mSwapChainImages[mCurrentTargetImage];

    // Empty the command buffer and begin it again from scratch:

    const VkResult resetBufferResult = vkResetCommandBuffer(*commandBuffer, 0);
    if(VK_SUCCESS != resetBufferResult)
    {
      KRUST_LOG_ERROR << "Failed to reset command buffer. Error: " << resetBufferResult << Krust::endlog;
      return;
    }

    auto commandBufferInheritanceInfo = kr::CommandBufferInheritanceInfo(nullptr, 0,
      nullptr, VK_FALSE, 0, 0);
    auto bufferBeginInfo = kr::CommandBufferBeginInfo(0, &commandBufferInheritanceInfo);

    const VkResult beginBufferResult = vkBeginCommandBuffer(*commandBuffer,&bufferBeginInfo);
    if(VK_SUCCESS != beginBufferResult)
    {
      KRUST_LOG_ERROR << "Failed to begin command buffer. Error: " << beginBufferResult << Krust::endlog;
      return;
    }

    // Assume the image is returned from being presented and fix it up using
    // an image memory barrier:
    auto postPresentImageMemoryBarrier = kr::ImageMemoryBarrier( 0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      framebufferImage,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    vkCmdPipelineBarrier(
      *commandBuffer,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
       0, 0, nullptr, 0, nullptr,
      1, &postPresentImageMemoryBarrier
    );

    VkClearValue clearValues[2];
    clearValues[0].color.float32[0] = mClearColor[0];
    clearValues[0].color.float32[1] = mClearColor[1];
    // Make it visually obvious that there are multiple framebuffers:
    clearValues[0].color.float32[2] = mClearColor[2];
    clearValues[0].color.float32[3] = 0.2f;
    clearValues[1].depthStencil.depth   = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    auto renderArea = kr::Rect2D(
      kr::Offset2D(0, 0),
      kr::Extent2D(
        mWindow->GetPlatformWindow().GetWidth(),
        mWindow->GetPlatformWindow().GetHeight()));

    auto beginRenderPass = kr::RenderPassBeginInfo(
      mGraphicsSwapchain.mRenderPasses[mCurrentTargetImage],
      mGraphicsSwapchain.mSwapChainFramebuffers[mCurrentTargetImage],
      renderArea,
      2U,
      clearValues);

    vkCmdBeginRenderPass(*commandBuffer, &beginRenderPass, VK_SUBPASS_CONTENTS_INLINE);

    // We don't need to execute any explicit command to clear the screen in here.
    // The VK_ATTACHMENT_LOAD_OP_CLEAR of our subpass attachments clears implicitly.

    vkCmdEndRenderPass(*commandBuffer);

    // Assume the framebuffer will be presented so insert an image memory
    // barrier here first:
    auto presentBarrier = kr::ImageMemoryBarrier(
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      framebufferImage,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    vkCmdPipelineBarrier(*commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &presentBarrier);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(*commandBuffer);
    if(endCommandBufferResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to end command buffer with result: " << endCommandBufferResult << Krust::endlog;
      return;
    }

    // Execute command buffer on main queue:
    KRUST_LOG_DEBUG << "Submitting command buffer " << mCurrentTargetImage << "(" << *(mCommandBuffers[mCurrentTargetImage]) << ")." << Krust::endlog;
    constexpr VkPipelineStageFlags pipelineFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    auto submitInfo = kr::SubmitInfo(
      1, mSwapChainSemaphore->GetVkSemaphoreAddress(),
      &pipelineFlags,
      // We have one command buffer per presentable image, so submit the right one:
      1, mCommandBuffers[mCurrentTargetImage]->GetVkCommandBufferAddress(),
      0, nullptr);
    const VkResult submitResult = vkQueueSubmit(*mDefaultGraphicsQueue, 1, &submitInfo, *submitFence);
    if(submitResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to submit command buffer. Result: " << submitResult << Krust::endlog;
      return;
    }

    // Animate the clear color with wrap around:
    mClearColor[0] -= 0.007f;
    mClearColor[1] += 0.003f;
    mClearColor[2] += 0.009f;
    mClearColor[0] = mClearColor[0] < 0.0f ? mClearColor[0] + 1.0f : mClearColor[0];
    mClearColor[1] = mClearColor[1] > 1.0f ? mClearColor[1] - 1.0f : mClearColor[1];
    mClearColor[2] = mClearColor[2] > 1.0f ? mClearColor[2] - 1.0f : mClearColor[2];
  }

  ~Clear2Application()
  {
    KRUST_LOG_DEBUG << "Clear2Application::~Clear2Application()" << Krust::endlog;
  }

private:
  // Data:
  float mClearColor[3] = {0.9f, 0.7f, 0.2f};
};

int main()
{
  std::cout << "Krust Clear example 2, version 0.9.0\n";

  Clear2Application application;
  application.SetName("Clear 2");
  application.SetVersion(1);

  // Request a busy loop which constantly repaints to show the clear
  // animation:
  const int status = application.Run(Krust::IO::MainLoopType::Busy);

  KRUST_LOG_INFO << "Exiting cleanly with code " << status << ".\n";
  return status;
}
