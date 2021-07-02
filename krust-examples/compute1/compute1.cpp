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

// External includes:
#include "krust-io/public-api/krust-io.h"
#include "krust/public-api/krust.h"
#include "krust/public-api/conditional-value.h"
#include <fstream>

namespace kr = Krust;

namespace
{
/** Number of samples per framebuffer pixel. */
constexpr VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;
constexpr VkAllocationCallbacks* ALLOCATION_CALLBACKS = nullptr;
constexpr unsigned WORKGROUP_X = 8u;
constexpr unsigned WORKGROUP_Y = 8u;

kr::ShaderBuffer loadSpirV(const char* const filename)
{
  kr::ShaderBuffer spirv;
  if(std::ifstream is{filename, std::ios::binary | std::ios::ate}) {
    auto size = is.tellg();
    spirv.resize(size / sizeof(kr::ShaderBuffer::value_type));
    is.seekg(0);
    is.read((char*)&spirv[0], size);
  } else {
    KRUST_LOG_ERROR << "Failed to open shader file \"" << filename << "\"." << Krust::endlog;
  }
  return spirv;
}
}

/**
 * Fills its window with a pattern generated in a compute shader each frame.
 */
class Compute1Application : public Krust::IO::Application
{
public:
  /**
   * Called by the default initialization once Krust is initialised and a window
   * has been created. Now is the time to do any additional setup.
   */
  virtual bool DoPostInit()
  {
    KRUST_LOG_DEBUG << "DoPostInit() entered." << Krust::endlog;

    const unsigned win_width  { mDefaultWindow->GetPlatformWindow().GetWidth()};
    const unsigned win_height { mDefaultWindow->GetPlatformWindow().GetHeight()};

    // Setup for buffered rendering with a graphics pipeline even though that
    // does more than we need to write to the swapchain directly in the compute
    // stage:
    if(!Krust::BuildFramebuffersForSwapChain(
      *mGpuInterface,
      mSwapChainImageViews,
      mDepthBufferView,
      win_width,
      win_height,
      mFormat,
      mDepthFormat,
      NUM_SAMPLES,
      mRenderPasses,
      mSwapChainFramebuffers,
      mSwapChainFences))
    {
      return false;
    }

    // Allocate a command buffer per swapchain entry:
    KRUST_ASSERT1(mCommandBuffers.size() == 0, "Double init of command buffers.");
    kr::CommandBuffer::Allocate(*mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, unsigned(mSwapChainImageViews.size()), mCommandBuffers);

    // Build all resources required to run the compute shader:

    // Load the spir-v shader code into a module:
    kr::ShaderBuffer spirv { loadSpirV("compute1_cmake.comp.spv") };
    if(spirv.empty()){
      return false;
    }
    auto shaderModule = kr::ShaderModule::New(*mGpuInterface, 0, spirv);

    const auto ssci = kr::PipelineShaderStageCreateInfo(
      0,
      VK_SHADER_STAGE_COMPUTE_BIT,
      *shaderModule,
      "main",
      nullptr // VkSpecializationInfo
    );

    // Define the descriptor and pipeline layouts:

    const auto fbBinding = kr::DescriptorSetLayoutBinding(
      0, // Binding to the first location
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      1,
      VK_SHADER_STAGE_COMPUTE_BIT,
      nullptr // No immutable samplers.
    );

    auto descriptorSetLayout = kr::DescriptorSetLayout::New(*mGpuInterface, 0, 1, &fbBinding);
    mPipelineLayout = kr::PipelineLayout::New(*mGpuInterface, 0, 1, descriptorSetLayout->GetDescriptorSetLayoutAddress());

    // Construct our compute pipeline:
    mComputePipeline = kr::ComputePipeline::New(*mGpuInterface, kr::ComputePipelineCreateInfo(
      0,    // no flags
      ssci,
      *mPipelineLayout,
      VK_NULL_HANDLE, // no base pipeline
      -1 // No index of a base pipeline.
    ));

    // Create a descriptor pool and allocate a matching descriptorSet for each image:
    VkDescriptorPoolSize poolSizes[1] = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, uint32_t(mSwapChainImages.size())}};
    mDescriptorPool = kr::DescriptorPool::New(*mGpuInterface, 0, uint32_t(mSwapChainImages.size()), uint32_t(1), poolSizes);
    for(unsigned i = 0; i < mSwapChainImages.size(); ++i)
    {
      auto set = kr::DescriptorSet::Allocate(*mDescriptorPool, *descriptorSetLayout);
      mDescriptorSets.push_back(set);
      auto imageInfo = kr::DescriptorImageInfo(VK_NULL_HANDLE, mSwapChainImageViews[i], VK_IMAGE_LAYOUT_GENERAL);
      auto write = kr::WriteDescriptorSet(*set, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo, nullptr, nullptr);
      vkUpdateDescriptorSets(*mGpuInterface, 1, &write, 0, nullptr);
    }

    return true;
  }

  bool DoPreDeInit()
  {
    // Destroy VK objects:
    for(unsigned i = 0; i < mRenderPasses.size(); ++i)
    {
      vkDestroyRenderPass(*mGpuInterface, mRenderPasses[i], ALLOCATION_CALLBACKS);
    }

    mComputePipeline.Reset();
    mPipelineLayout.Reset();;
    mDescriptorSets.clear();
    mDescriptorPool.Reset();

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
    KRUST_LOG_INFO << "   ------------ Compute Example 1: draw frame! frame: " << frameNumber++ << ". currImage: " << mCurrentTargetImage << ". handle: " << mSwapChainImages[mCurrentTargetImage] << "  ------------\n";

    auto submitFence = mSwapChainFences[mCurrentTargetImage];
    const VkResult fenceWaitResult = vkWaitForFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress(), true, 1000000000); // Wait one second.
    if(VK_SUCCESS != fenceWaitResult)
    {
      KRUST_LOG_ERROR << "Wait for queue submit of main commandbuffer did not succeed: " << fenceWaitResult << Krust::endlog;
    }
    vkResetFences(*mGpuInterface, 1, submitFence->GetVkFenceAddress());

    constexpr VkPipelineStageFlags pipelineFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    auto submitInfo = kr::SubmitInfo(
      1, &mSwapChainSemaphore,
      &pipelineFlags,
      // We have one command buffer per presentable image, so submit the right one:
      1, mCommandBuffers[mCurrentTargetImage]->GetVkCommandBufferAddress(),
      0, nullptr);

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
      VK_ACCESS_SHADER_READ_BIT |
      VK_ACCESS_SHADER_WRITE_BIT |
      VK_ACCESS_MEMORY_READ_BIT |
      VK_ACCESS_MEMORY_WRITE_BIT,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_IMAGE_LAYOUT_GENERAL,
      VK_QUEUE_FAMILY_IGNORED,
      VK_QUEUE_FAMILY_IGNORED,
      framebufferImage,
      { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    vkCmdPipelineBarrier(
        *commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, // VkDependencyFlags
        0, nullptr,
        0, nullptr,
        1, &postPresentImageMemoryBarrier);

    // Bind the Descriptor set to the current command buffer:
    vkCmdBindDescriptorSets(
      *commandBuffer,
      VK_PIPELINE_BIND_POINT_COMPUTE,
      *mPipelineLayout,
      0,
      1,
      mDescriptorSets[mCurrentTargetImage]->GetHandleAddress(),
      0, nullptr // No dynamic offsets.
      );

    // Here is where we kick off our compute shader.

    vkCmdBindPipeline(*commandBuffer,VK_PIPELINE_BIND_POINT_COMPUTE, *mComputePipeline);
    const unsigned win_width  { mDefaultWindow->GetPlatformWindow().GetWidth()};
    const unsigned win_height { mDefaultWindow->GetPlatformWindow().GetHeight()};
    vkCmdDispatch(*commandBuffer,
      win_width / WORKGROUP_X + (win_width % WORKGROUP_X ? 1 : 0 ),
      win_height / WORKGROUP_Y + (win_width % WORKGROUP_Y ? 1 : 0 ),
      1);

    // Assume the framebuffer will be presented so insert an image memory
    // barrier here first:
    auto presentBarrier = kr::ImageMemoryBarrier(
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_MEMORY_READ_BIT,
      VK_IMAGE_LAYOUT_GENERAL,
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
    const VkResult submitResult = vkQueueSubmit(*mDefaultGraphicsQueue, 1, &submitInfo, *submitFence);
    if(submitResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to submit command buffer. Result: " << submitResult << Krust::endlog;
      return;
    }

  }

  ~Compute1Application()
  {
    KRUST_LOG_DEBUG << "Compute1Application::~Compute1Application()" << Krust::endlog;
  }

private:
  // Data:
  kr::PipelineLayoutPtr mPipelineLayout;
  kr::DescriptorPoolPtr mDescriptorPool;
  /// One descriptor set per swapchain image.
  std::vector<kr::DescriptorSetPtr> mDescriptorSets;
  kr::ComputePipelinePtr mComputePipeline;
};

int main()
{
  Compute1Application application;
  application.SetName("Compute 1");
  application.SetVersion(1);

  // Request a busy loop which constantly repaints to show the
  // animation:
  const int status = application.Run(Krust::IO::MainLoopType::Busy, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  KRUST_LOG_INFO << "Exiting cleanly with code " << status << ".\n";
  return status;
}
