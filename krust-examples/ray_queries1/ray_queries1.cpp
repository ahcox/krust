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
#include "krust-gm/public-api/vec3_fwd.h"
#include "krust-gm/public-api/vec3_inl.h"
#include "krust/public-api/krust.h"
#include "krust/public-api/line-printer.h"
#include "krust/public-api/vulkan-utils.h"
#include "krust/public-api/conditional-value.h"
#include "krust-kernel/public-api/floats.h"
#include <chrono>

namespace kr = Krust;

namespace
{
/** Number of samples per framebuffer pixel. */
constexpr VkSampleCountFlagBits NUM_SAMPLES = VK_SAMPLE_COUNT_1_BIT;
constexpr VkAllocationCallbacks* ALLOCATION_CALLBACKS = nullptr;
constexpr unsigned WORKGROUP_X = 8u;
constexpr unsigned WORKGROUP_Y = 8u;
constexpr const char* const RT1_SHADER = "rt1.comp.spv";
constexpr const char* const RT2_SHADER = "rt2.comp.spv";
constexpr const char* const GREY_SHADER = "rtow_diffuse_grey.comp.spv";
constexpr const char* const MATERIALS_SHADER = "rtow_ray_query.comp.spv";

struct Pushed
{
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t frame_no;
    float pad0;
    float ray_origin[3];
    float padding1;
    float ray_target_origin[3];
    float padding2;
    float ray_target_right[3];
    float padding3;
    float ray_target_up[3];
    float padding4;
};
KRUST_COMPILE_ASSERT(sizeof(Pushed) <= 128u, "Push Constants are larger than the minimum guaranteed space.")

/// Values which vary between shaders that this app can run.
struct ShaderParams {
  Pushed push_defaults;
  float  move_scale;
};

void viewVecsFromAngles(
  const float pitch, const float yaw,
  kr::Vec3& rightOut, kr::Vec3& upOut, kr::Vec3& fwdOut)
{
  const float cosPitch = cosf(pitch);
  const float cosYaw   = cosf(yaw);
  const float cosRoll  = 1.0f;
  const float sinPitch = sinf(pitch);
  const float sinYaw   = sinf(yaw);
  const float sinRoll  = 0.0f;

  fwdOut = kr::make_vec3(sinYaw * cosPitch, sinPitch, cosPitch * (-cosYaw));
  upOut  = kr::make_vec3(-cosYaw * sinRoll - sinYaw * sinPitch * cosRoll, cosPitch * cosRoll, -sinYaw * sinRoll - sinPitch * cosRoll * -cosYaw);
  rightOut = kr::cross(fwdOut, upOut);
}

}

/**
 * Fills its window with a scene generated by ray tracing in a compute shader
 * each frame.
 */
class RayQueries1Application : public Krust::IO::Application
{
  uint32_t DoChooseVulkanVersion() const override
  {
    // Request version 1.2 so we can use later shader features and ray queries:
    return VK_API_VERSION_1_2;
  }

  void DoAddRequiredDeviceExtensions(std::vector<const char*>& extensionNames) const override
  {
    extensionNames.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    extensionNames.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    extensionNames.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
  }

  void DoExtendDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features) override
  {
    mDeviceFeature11.pNext = &mDeviceFeature12;
    mDeviceFeature12.pNext = &mDeviceRayQueryFeatures;
    mDeviceRayQueryFeatures.pNext = &mDeviceAccelerationStructureFeatures;
    mDeviceAccelerationStructureFeatures.pNext = features.pNext;
    features.pNext = &mDeviceFeature11;
  }

  void DoCustomizeDeviceFeatureChain(VkPhysicalDeviceFeatures2 &f2) override
  {
    #define REQUIRE_VK_FEATURE(FEATURE, MSG) \
    KRUST_ASSERT1((FEATURE), (MSG)); \
    if(!(FEATURE)){ \
      KRUST_LOG_ERROR << (MSG); \
    } \
    (FEATURE) = VK_TRUE;

    // Check we have things we do need:
    REQUIRE_VK_FEATURE(f2.features.shaderInt16, "16 bit ints are required in shaders.");
    REQUIRE_VK_FEATURE(mDeviceFeature12.storagePushConstant8, "8 bit ints are required in shader push Constant buffers.");
    REQUIRE_VK_FEATURE(mDeviceFeature12.shaderInt8, "Eight bit integers in shader code required.");
    REQUIRE_VK_FEATURE(mDeviceRayQueryFeatures.rayQuery, "This is a ray query demo so we gotta have the ray query extension.");
    REQUIRE_VK_FEATURE(mDeviceAccelerationStructureFeatures.accelerationStructure, "Ray tracing acceleration structures required.");
    // Need them?
    //VkBool32           accelerationStructureCaptureReplay;
    //VkBool32           accelerationStructureIndirectBuild;
    //VkBool32           accelerationStructureHostCommands;
    //VkBool32           descriptorBindingAccelerationStructureUpdateAfterBind;

    // Turn off things we don't need:
    f2.features.independentBlend = VK_FALSE;
    f2.features.geometryShader = VK_FALSE;
    f2.features.tessellationShader = VK_FALSE;
    f2.features.sampleRateShading = VK_FALSE;
    f2.features.dualSrcBlend = VK_FALSE;
    f2.features.logicOp = VK_FALSE;
    f2.features.multiDrawIndirect = VK_FALSE;
    f2.features.drawIndirectFirstInstance = VK_FALSE;
    f2.features.depthClamp = VK_FALSE;
    f2.features.depthBiasClamp = VK_FALSE;
    f2.features.fillModeNonSolid = VK_FALSE;
    f2.features.depthBounds = VK_FALSE;
    f2.features.wideLines = VK_FALSE;
    f2.features.largePoints = VK_FALSE;
    f2.features.alphaToOne = VK_FALSE;
    f2.features.multiViewport = VK_FALSE;
    f2.features.occlusionQueryPrecise = VK_FALSE;
    f2.features.shaderClipDistance = VK_FALSE;
    f2.features.shaderCullDistance = VK_FALSE;
    f2.features.shaderResourceResidency = VK_FALSE;
    f2.features.shaderResourceMinLod = VK_FALSE;
    // Might want these turned back on at some point:
    f2.features.sparseBinding = VK_FALSE;
    f2.features.sparseResidencyBuffer = VK_FALSE;
    f2.features.sparseResidencyImage2D = VK_FALSE;
    f2.features.sparseResidencyImage3D = VK_FALSE;
    f2.features.sparseResidency2Samples = VK_FALSE;
    f2.features.sparseResidency4Samples = VK_FALSE;
    f2.features.sparseResidency8Samples = VK_FALSE;
    f2.features.sparseResidency16Samples = VK_FALSE;
    f2.features.sparseResidencyAliased = VK_FALSE;
    f2.features.variableMultisampleRate = VK_FALSE;
    f2.features.inheritedQueries = VK_FALSE;
  }


public:
  /**
   * Called by the default initialization once Krust is initialised and a window
   * has been created. Now is the time to do any additional setup.
   */
  virtual bool DoPostInit()
  {
    KRUST_LOG_DEBUG << "DoPostInit() entered." << Krust::endlog;

    if( 0 == (mCmdBuildAccelerationStructuresKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, CmdBuildAccelerationStructuresKHR)))
    {
      KRUST_LOG_ERROR << "Failed to get pointers to required extension functions.";
      return false;
    }

    BuildFences(*mGpuInterface, VK_FENCE_CREATE_SIGNALED_BIT, mSwapChainImageViews.size(), mSwapChainFences);

    // Allocate a command buffer per swapchain entry:
    KRUST_ASSERT1(mCommandBuffers.size() == 0, "Double init of command buffers.");
    kr::CommandBuffer::Allocate(*mCommandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, unsigned(mSwapChainImageViews.size()), mCommandBuffers);

    // Build all resources required to run the compute shader:

    // Load the spir-v shader code into a module:
    kr::ShaderBuffer spirv { kr::loadSpirV(mShaderName) };
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
    mPipelineLayout = kr::PipelineLayout::New(*mGpuInterface,
      0,
      *descriptorSetLayout,
      kr::PushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Pushed))
      );

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

    mLinePrinter = std::make_unique<kr::LinePrinter>(*mGpuInterface, kr::span {mSwapChainImageViews});

    return true;
  }

  bool DoPreDeInit()
  {
    mComputePipeline.Reset();
    mPipelineLayout.Reset();;
    mDescriptorSets.clear();
    mDescriptorPool.Reset();
    mLinePrinter.reset();

    return true;
  }

  virtual void OnKey(bool up, Krust::IO::KeyCode keycode) override
  {
    // 25, 39, 38, 40, 24, 26, // WSADQE
    // 111, 116, 113, 114, 112, 117 // up,down,left,right arrows, pgup, pgdn
    switch(keycode)
    {
      case 25:
      case 111:{
        KRUST_LOG_DEBUG << "Forward key " << (up ? "up" : "down") << kr::endlog;
        mKeyFwd = !up;
        break;
      }
      case 39:
      case 116:{
        KRUST_LOG_DEBUG << "Backwards key " << (up ? "up" : "down") << kr::endlog;
        mKeyBack = !up;
        break;
      }
      case 38:
      case 113:{
        KRUST_LOG_DEBUG << "Left key " << (up ? "up" : "down") << kr::endlog;
        mKeyLeft = !up;
        break;
      }
      case 40:
      case 114:{
        KRUST_LOG_DEBUG << "Right key " << (up ? "up" : "down") << kr::endlog;
        mRightKey = !up;
        break;
      }
      case 26:
      case 112:{
        KRUST_LOG_DEBUG << "Up key " << (up ? "up" : "down") << kr::endlog;
        mKeyUp = !up;
        break;
      }
      case 24:
      case 117:{
        KRUST_LOG_DEBUG << "Down key " << (up ? "up" : "down") << kr::endlog;
        mKeyDown = !up;
        break;
      }
      default:
        KRUST_LOG_WARN << "Unknown key with scancode " << unsigned(keycode) << (up ? " up" : " down") << kr::endlog;
    }
  }

  void OnMouseMove(const Krust::IO::InputTimestamp when, const int x, const int y, const unsigned state) override
  {
    /// @todo Time.
    if(kr::IO::left(state)){
      if(!mLeftMouse){
        mLeftMouse = true;
        mLastX = x;
        mLastY = y;
      }
      float yawRads   = (x - mLastX) * 0.005f;
      float pitchRads = (y - mLastY) * 0.005f;
      /// @todo clamp range of pitch.
      mCameraYaw   += yawRads;
      mCameraPitch -= pitchRads;
      mCameraPitch = std::min(1.55f, std::max(mCameraPitch, -1.55f));
      while(mCameraYaw < 0.0f){
        mCameraYaw += float(3.14159265358979323846 * 2);
      }
      while(mCameraYaw > float(3.14159265358979323846 * 2)){
        mCameraYaw -= float(3.14159265358979323846 * 2);
      }
    } else {
      mLeftMouse = false;
    }
    mLastX = x;
    mLastY = y;
  }

  /**
   * Called by the default run loop to repaint the screen.
   */
  virtual void DoDrawFrame()
  {
    auto start = std::chrono::high_resolution_clock::now();

    static unsigned frameNumber = -1;
    ++frameNumber;
    // KRUST_LOG_INFO << "   ------------ Ray Tracing Example 1: draw frame! frame: " << frameNumber << ". currImage: " << mCurrentTargetImage << ". handle: " << mSwapChainImages[mCurrentTargetImage] << "  ------------\n";

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
    const unsigned win_width  { mWindow->GetPlatformWindow().GetWidth()};
    const unsigned win_height { mWindow->GetPlatformWindow().GetHeight()};

    mPushed.fb_width  = win_width;
    mPushed.fb_height = win_height;
    mPushed.frame_no = frameNumber;
    const float MOVE_SCALE = mMoveScale;

    if(mKeyLeft) {
      kr::store(kr::load(mPushed.ray_origin) + (-kr::load(mPushed.ray_target_right) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mRightKey) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_right) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mKeyFwd) {
      kr::store(kr::load(mPushed.ray_origin) + kr::cross(kr::load(mPushed.ray_target_up), kr::load(mPushed.ray_target_right)) * MOVE_SCALE, mPushed.ray_origin);
    }
    if(mKeyBack) {
      kr::store(kr::load(mPushed.ray_origin) + kr::cross(kr::load(mPushed.ray_target_right), kr::load(mPushed.ray_target_up)) * MOVE_SCALE, mPushed.ray_origin);
    }
    if(mKeyUp) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_up) * MOVE_SCALE) , mPushed.ray_origin);
    }
    if(mKeyDown) {
      kr::store(kr::load(mPushed.ray_origin) + (kr::load(mPushed.ray_target_up) * -MOVE_SCALE) , mPushed.ray_origin);
    }
    mPushed.ray_origin[1] = kr::clamp(mPushed.ray_origin[1], -30.0f, 1500.0f);

    // Work out camera direction from mouse-defined angles:
    kr::Vec3 right, up, fwd;
    viewVecsFromAngles(mCameraPitch, mCameraYaw, right, up, fwd);
    auto ray_origin = kr::make_vec3(mPushed.ray_origin);
    // The ray target origin is the bottom-left corner of the worldspace 2d grid,
    // equivalent to the pixels of the framebuffer, that we will shoot rays at.
    auto ray_target_origin = ray_origin
        //+ fwd * (win_width * 0.5f)
        + fwd * (win_height * 0.5f)
        + (-right) * (win_width * 0.5f)
        + (-up)    * (win_height * 0.5f);

    kr::store(ray_target_origin, mPushed.ray_target_origin);
    kr::store(right,             mPushed.ray_target_right);
    kr::store(up,                mPushed.ray_target_up);


    vkCmdPushConstants(*commandBuffer, *mPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Pushed), &mPushed);

    vkCmdDispatch(*commandBuffer,
      win_width / WORKGROUP_X + (win_width % WORKGROUP_X ? 1 : 0 ),
      win_height / WORKGROUP_Y + (win_width % WORKGROUP_Y ? 1 : 0 ),
      1);

    /// @todo Memory barrier here waiting for writes to the framebuffer from the main kernel to complete.

    mLinePrinter->SetFramebuffer(mSwapChainImageViews[mCurrentTargetImage], mCurrentTargetImage);
    mLinePrinter->BindCommandBuffer(*commandBuffer, mCurrentTargetImage);

    // These Xs in bottom corners flicker, showing we need to pause after main kernel.
    mLinePrinter->PrintLine(*commandBuffer, 0, 49, 6, 2, false, false, "X");
    mLinePrinter->PrintLine(*commandBuffer, 224, 49, 7, 3, false, true, "X");
    mLinePrinter->PrintLine(*commandBuffer, 224, 0, 5, 4, true, false, "X");
    std::chrono::duration<double> diff = start - mFrameInstant;
    const float fps = 1.0f / diff.count();
    mAmortisedFPS = (mAmortisedFPS * 7 + fps) * 0.125f;
    char buffer[125];
    snprintf(buffer, sizeof(buffer), "FPS: %.1f", mAmortisedFPS);
    mLinePrinter->PrintLine(*commandBuffer, 0, 0, 3, 0, true, true, buffer);
    snprintf(buffer, sizeof(buffer), "MS: %.2f", float(diff.count() * 1000));
    mLinePrinter->PrintLine(*commandBuffer, 0, 1, 3, 0, true, true, buffer);

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
    // KRUST_LOG_DEBUG << "Submitting command buffer " << mCurrentTargetImage << "(" << *(mCommandBuffers[mCurrentTargetImage]) << ")." << Krust::endlog;
    const VkResult submitResult = vkQueueSubmit(*mDefaultGraphicsQueue, 1, &submitInfo, *submitFence);
    if(submitResult != VK_SUCCESS)
    {
      KRUST_LOG_ERROR << "Failed to submit command buffer. Result: " << submitResult << Krust::endlog;
      return;
    }

    mFrameInstant = start;
  }

  ~RayQueries1Application()
  {
    KRUST_LOG_DEBUG << "RayQueries1Application::~RayQueries1Application()" << Krust::endlog;
  }

private:
  // Data:
  VkPhysicalDeviceVulkan11Features    mDeviceFeature11 = kr::PhysicalDeviceVulkan11Features();
  VkPhysicalDeviceVulkan12Features    mDeviceFeature12 = kr::PhysicalDeviceVulkan12Features();
  VkPhysicalDeviceRayQueryFeaturesKHR mDeviceRayQueryFeatures = kr::PhysicalDeviceRayQueryFeaturesKHR();
  VkPhysicalDeviceAccelerationStructureFeaturesKHR mDeviceAccelerationStructureFeatures = kr::PhysicalDeviceAccelerationStructureFeaturesKHR();
  // Functions from required extensions:
  PFN_vkCmdBuildAccelerationStructuresKHR mCmdBuildAccelerationStructuresKHR = nullptr;
  kr::PipelineLayoutPtr mPipelineLayout;
  kr::DescriptorPoolPtr mDescriptorPool;
  /// One descriptor set per swapchain image.
  std::vector<kr::DescriptorSetPtr> mDescriptorSets;
  kr::ComputePipelinePtr mComputePipeline;
  std::unique_ptr<kr::LinePrinter> mLinePrinter;
  std::chrono::time_point<std::chrono::high_resolution_clock> mFrameInstant = std::chrono::high_resolution_clock::now();
  float mAmortisedFPS = 30;

  // Camera euler angles in radians. The mouse will drive these.
  float mCameraPitch = 0;
  float mCameraYaw = 0;
  // Keys currently depressed.
  bool mKeyLeft     = false;
  bool mRightKey    = false;
  bool mKeyFwd      = false;
  bool mKeyBack     = false;
  bool mKeyUp    = false;
  bool mKeyDown  = false;
  // Mouse state:
  bool mLeftMouse = false;
  int mLastX = 0;
  int mLastY = 0;
public:
  // Push Constants for first 2 shaders over checkerboard.
  Pushed mPushed1 = {
    1, // width
    1, // height
    0, //frame number
    0.0f, // padding
    {0,405,900}, 1,
    {-900,0,0}, 1,
    {1,0,0}, 1,
    {0,1,0}, 1,
  };
  // Push Constants for third shader based on RTIOW.
  Pushed mPushed = {
    1, // width
    1, // height
    0, //frame number
    0.0f, // padding
    {0,0,0 + 10}, 1, // ray_origin
    {-900,-405,-900 + 10}, 1,  // ray_target_origin
    {1,0,0}, 1,     //  ray_target_right
    {0,1,0}, 1,     // ray_target_up
  };
  float mMoveScale = 0.0625f;
  std::unordered_map<std::string, ShaderParams> mShaderParamsOptions {
    {RT1_SHADER,  {mPushed1, 7.5f}},
    {RT2_SHADER,  {mPushed1, 6.5f}},
    {GREY_SHADER, {mPushed,  0.0625f}},
    {MATERIALS_SHADER, {mPushed,  0.0625f}},
  };
  ShaderParams* mShaderParams {&mShaderParamsOptions[MATERIALS_SHADER]};
  const char* mShaderName = MATERIALS_SHADER;
};


int main(const int argc, const char* argv[])
{
  puts("Ray Tracing in a GPU compute shader using ray queries.\n"
      "Usage:\n"
      "1. rt1\n"
      "2. rt1 compiled_spirv_shader_filename\n"
      "3. rt1 compiled_spirv_shader_filename true # To disable vsync\n"
  );

  RayQueries1Application application;
  application.SetName("Ray Queries 1");
  application.SetVersion(1);
  uint8_t keycodes[] = {
    25, 39, 38, 40, 24, 26, // WSADQE
    111, 116, 113, 114, 112, 117 // up,down,left,right arrows, pgup, pgdn
  };
  application.ListenToScancodes(keycodes, sizeof(keycodes));
  if(argc > 1){
    application.mShaderName = argv[1];
    const ShaderParams& params = application.mShaderParamsOptions[application.mShaderName];
    application.mPushed = params.push_defaults;
    application.mMoveScale = params.move_scale;
  }
  bool allowTearing = false;
  if(argc > 2){
    const char * tr = "true";
    if(!strcasecmp(tr, argv[2])){
      allowTearing = true;
    }
  }

  // Request a busy loop which constantly repaints to show the
  // animation:
  const int status = application.Run(Krust::IO::MainLoopType::Busy, VK_IMAGE_USAGE_STORAGE_BIT, allowTearing);

  KRUST_LOG_INFO << "Exiting cleanly with code " << status << ".\n";
  return status;
}
