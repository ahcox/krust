// Copyright (c) 2016-2022 Andrew Helge Cox
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

#ifndef KRUST_IO_PUBLIC_API_APPLICATION_H_
#define KRUST_IO_PUBLIC_API_APPLICATION_H_

#include "public-api/application-platform.h"

// Internal includes:
#include "application-interface.h"
#include "application-component.h"
#include "keyboard.h"

// External includes:
#include "krust/public-api/krust.h"
#include "krust/public-api/intrusive-pointer.h"
#include "krust/public-api/conditional-value.h"
#include "krust-kernel/public-api/span.h"
#include <krust/public-api/vulkan_types_and_macros.h>
#include <vector>
#include <bitset>

namespace Krust {

class QueueJanitor;
using QueueJanitorPtr = IntrusivePointer<QueueJanitor>;

namespace IO {

/// The name of the engine passed to Vulkan.
extern const char* const KRUST_ENGINE_NAME;

/// The version number of the engine passed to Vulkan.
extern const uint32_t KRUST_ENGINE_VERSION_NUMBER;

class Window;
using WindowPointer = IntrusivePointer<Window>;

enum class MainLoopType
{
  Busy,    ///< Run the main loop as fast as possible (game mode).
  Reactive ///< Run in a purely reactive fashion in response to external events
           ///< (application mode).
};

enum class MainLoopFlagBits
{
  Redraw = 1,
  Quit = 2,
};
using MainLoopFlags = uint32_t;

/**
 * @brief Krust::IO applications are based on a subclass of this.
 *
 * Usage:
 * <code>
 * #include "krust-io.h"
 *
 * class MyApp : public Krust::IO::Application {
 *
 *   void OnKey(...) { react to input }
 *   // Implement all OnX() event handling functions required
 *
 *   void DoDrawFrame(...) { draw scene }
 *   // Implement all DoX() hook functions required.
 *
 * }
 *
 * int main(char** argv, int argc)
 * {
 *   MyApp myApp;
 *   return myApp.Run(argv, argc);
 * }
 * </code>
 *
 * # Overriding Virtual Functions
 *
 * There are three classes of functions which application derived classes can
 * override to implement their own functionality.
 *
 *     1. `OnSomeEvent()`: These functions with an `On` prefix are callback handlers
 *        for events such as key presses or window invalidations that the application
 *        needs to know about.
 *     2. `DoSomeThing()`: These functions with a `Do` prefix are hooks explicitly
 *        placed to allow the application to do something specific to it at key points.
 *        For Example `DoPostinit()` give the application the chance to build its own
 *        resources and do one-time init once a window is up and Vulkan is running.
 *     3. `OtherVirtualFunction()`: Virtual functions without either of the two
 *        prefixes should only be overridden if absolutely necessary as they probably
 *        do work which the derived class would have to replicate.
 */
class Application : protected ApplicationInterface
{
public:
  Application();

  /**
   * @brief runs the main loop of the application and only returns on exit.
   * @return An integer status code: 0 for success, or an
   * implementation-specific error code otherwise.
   * @note Only override if the application has special advanced needs.
   */
  virtual int Run(MainLoopType loopType = MainLoopType::Reactive, VkImageUsageFlags swapchainUsageOverrides = 0, bool allowTearing = false);

  void SetName(const char * const appName) { mAppName = appName; }
  void SetVersion(uint32_t appVersion) { mAppVersion = appVersion; }
  void AddComponent(ApplicationComponent& component);
  void ListenToScancodes(uint8_t* keycodes, size_t numKeys);

protected:
  virtual bool Init(VkImageUsageFlags swapchainUsageOverrides = 0, bool allowTearing = false);
  virtual bool DeInit();

  /**
   * Do everything to get Vulkan up and running.
   */
  virtual bool InitVulkan(VkImageUsageFlags swapchainUsageOverrides = 0, bool allowTearing = false);

  /**
   * Setup layers and extensions and create a Vulkan instance.
   */
  virtual bool InitVulkanInstance();

  /**
   * Investigate the queues available.
   */
  virtual bool InitQueueInfo();

  /**
   * Setup a Physical Device (GPU) to draw with.
   */
  virtual bool InitVulkanGpus();

  /**
   * Setup a queue that we can draw and present on.
   */
  virtual bool InitDefaultQueue();

  /**
   * Make a swapchain to get images displayed in the default window.
   */
  virtual bool InitDefaultSwapchain(VkImageUsageFlags swapchainUsageOverrides = 0, bool allowTearing = false);

  /**
   * Choose a surface format and colorspace pair supported by the hardware.
   * @Note SRGB formats may not be supported for image stores.
   */
  bool ChoosePresentableSurfaceFormatSpacePair(VkFormat format = VK_FORMAT_B8G8R8A8_UNORM);

  /**
   * Choose a present mode.
   * @return The best available present mode and true if successful,
   * otherwise an undefined mode and false.
   */
  ConditionalValue<VkPresentModeKHR> ChooseBestPresentMode(bool tearingOk) const;

  /// Record which queue families can present frames to the windowing system of
  /// the platform:
  void RecordPresentQueueFamilies();

  /**
   * @name Callbacks Callbacks for events such as input and window
   * resize, repaint required, closed, etc.
   * Implement them in your Application-derived class to handle window events.
   */
  ///@{

  /** The window has been resized. */
  virtual void OnResize(unsigned width, unsigned height);
  /** The window needs to be redrawn. */
  void OnRedraw() override;
  /** A key was pressed or released. */
  void OnKey(bool up, KeyCode keycode ) override;
  void OnMouseMove(InputTimestamp when, int x, int y, unsigned state) override;
  /** The window is being closed. You may wish
   * to close the app. */
  virtual void OnClose();

  ///@}

  /** The window has been resized. */
  void DispatchResize(unsigned width, unsigned height);

  /**
   * @name Overridables Template member functions for applications to override
   * to implement their own behaviours. These hooks are called by the Application
   * base class when it needs some policy guidance from the derived application.
   */
  ///@{

  virtual uint32_t DoChooseVulkanVersion() const;

  virtual uint32_t DoChoosePhysicalDevice(span<VkPhysicalDevice> gpus, span<VkPhysicalDeviceProperties> deviceProperties, span<VkPhysicalDeviceFeatures2> features, span<VkPhysicalDeviceMemoryProperties> memoryProperties) const;

  /**
   * @brief Allow derived applications to turn on required extensions.
   *
   * If a requested extension is not available, initialisation will fail.
   * @param extensionNames A list of already-enabled extensions which may be
   * added-to.
   */
  virtual void DoAddRequiredDeviceExtensions(std::vector<const char*>& extensionNames) const;

  /**
   * @brief Allow derived application classes to add additional groups of features
   *        to the chain that this base Application class will query for.
   * @see DoCustomizeDeviceFeatureChain,
   *      VkPhysicalDeviceVulkan11Features,
   *      VkPhysicalDeviceVulkan12Features,
   *      VkPhysicalDeviceProtectedMemoryFeatures,
   *      VkPhysicalDevice16BitStorageFeatures,
   *      VkPhysicalDeviceShaderSubgroupExtendedTypesFeatures,
   *      VkPhysicalDeviceShaderAtomicInt64Features,
   *      and countless others.
   */
  virtual void DoExtendDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features);

  /**
   * @brief Called during init so app can turn off GPU features it won't use.
   *
   * For example, if doing simple GLES 2.0 rendering, turn off geometry and
   * tesselation shaders etc. for potential speed ups.
   * Only necessary if the implementation runs faster / lighter with some disabled.
   * The default implementation simply returns the parameter passed in
   * unmodified, thus leaving all features turned-on.
   * @param[in] features Listed list chain of features supported by GPU.
   * @see DoExtendDeviceFeatureChain
   */
  virtual void DoCustomizeDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features);

  /**
   * @brief Called once Vulkan and a window are up and running.
   */
  virtual bool DoPostInit();

  /**
   * @brief Called while Vulkan and a window are still up and running so
   * application can cleanup.
   */
  virtual bool DoPreDeInit();

  /**
   * @brief Called by busy loop and by default OnRedraw() to paint the window
   * interior.
   *
   * Implementors must call vkQueueWaitSemaphore() on the parameter passed in
   * before they send any work to the gpu that targets the main framebuffer,
   * i.e. before it calls vkQueueSubmit() with an image in the default swapchain
   * bound to the renderstate.
   *
   * The derived app could call vkQueueWaitSemaphore() immediately but given that
   * it might block, for performance reasons it should wait as long as possible
   * before doing that and call it immediately before it needs to submit the
   * first command buffer to the queue.
   * It can prepare dynamic command buffers, update resources, etc. before making
   * that call. The longer it waits (while doing other work), the longer there is
   * for the GPU and presentation engine to asynchronously complete its own use
   * of an image in the swapchain and so have it ready to return from the call
   * without blocking.
   */
  virtual void DoDrawFrame();

  ///@}
public:
  friend class WindowPlatform;
  WindowPointer mWindow;
protected:
  const char * mAppName = 0;
  uint32_t mAppVersion = 0;

  /**
   * @name VkState Vulkan state.
   */
  ///@{
public:
  InstancePtr mInstance;
  VkPhysicalDevice  mGpu; ///< Physical GPU
  VkPhysicalDeviceProperties mGpuProperties;
  VkPhysicalDeviceFeatures2 mGpuFeatures = PhysicalDeviceFeatures2();
  std::vector<VkQueueFamilyProperties> mPhysicalQueueFamilyProperties;
  /// Addendum to mPhysicalQueueFamilyProperties: Records whether the
  /// corresponding queue families can present through WSI.
  std::vector<bool> mQueueFamilyPresentFlags;
  unsigned  mDefaultDrawingQueueFamily = 0;
  unsigned  mDefaultPresentQueueFamily = 0;
  VkPhysicalDeviceMemoryProperties mGpuMemoryProperties;
  DevicePtr mGpuInterface; ///< Logical GPU.
  QueueJanitorPtr   mDefaultQueue;
  /// Draw through this.
  QueueJanitor*  mDefaultGraphicsQueue = 0;
  /// Present using this.
  QueueJanitor*  mDefaultPresentQueue = 0;
  ///@}

protected:
  /**
   * @name VkInstanceExtensions Vulkan instance extensions.
   */
  ///@{
  PFN_vkCreateDebugReportCallbackEXT mCreateDebugReportCallback = 0;
  PFN_vkDestroyDebugReportCallbackEXT mDestroyDebugReportCallback = 0;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR mGetPhysicalDeviceSurfaceSupportKHR = 0;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR mGetPhysicalDeviceSurfaceCapabilitiesKHR = 0;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR mGetSurfaceFormatsKHR = 0;
  PFN_vkGetPhysicalDeviceSurfacePresentModesKHR mGetSurfacePresentModesKHR = 0;
  ///@}

  /** A handle to an API object pointing back to our debug info printing
   * callback. */
  VkDebugReportCallbackEXT mDebugCallbackHandle = 0;

  /**
   * @name VkDeviceExtensions Vulkan device extensions
   */
  ///@{
  PFN_vkAcquireNextImageKHR mAcquireNextImageKHR = 0;
  PFN_vkCreateSwapchainKHR mCreateSwapChainKHR = 0;
  PFN_vkDestroySwapchainKHR mDestroySwapChainKHR = 0;
  PFN_vkGetSwapchainImagesKHR mGetSwapchainImagesKHR = 0;
  PFN_vkQueuePresentKHR mQueuePresentKHR = 0;
  ///@}

public:
  /**
   * @name WindowData Data that is associated with the window.
   */
  ///@{
  /// Surface used when binding Vulkan surfaces to a window.
  VkSurfaceKHR mSurface;
  /// Framebuffer color image format.
  VkFormat mFormat = VK_FORMAT_B8G8R8A8_UNORM;
  /// The color space for framebuffers.
  VkColorSpaceKHR mColorspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  /// Object representing the series of images to render into and display.
  VkSwapchainKHR mSwapChain = 0;
  /// One or more image to render into for display to screen.
  std::vector<VkImage> mSwapChainImages;
  /// Image views into swapchain images for binding them to FrameBuffer objects
  /// as render targets. Correspond to entries in mSwapChainImages.
  std::vector<VkImageView> mSwapChainImageViews;
  /// Fence for each image in swapchain, this is signalled by last queue submit of a frame by apps,
  /// and waited for at start of frame by them:
  std::vector<FencePtr> mSwapChainFences;
  /// Command pool for all command buffers:
  CommandPoolPtr mCommandPool = nullptr;
  /// CommandBuffers for each image in swapchain:
  std::vector<CommandBufferPtr> mCommandBuffers;
  /// The entry in the mSwapChainImages array that was most recently acquired
  /// from WSI.
  uint32_t mCurrentTargetImage = 0;
  /// Used by WSI to signal to us that an image in the swapchain is available to
  /// render into.
  SemaphorePtr mSwapChainSemaphore = nullptr;

  ///@}

private:
  /// The event loops check this and exits cleanly when it goes to true.
  bool mQuit = false;
  /// Platform-specific features such as an X11 connection on Linux.
  ApplicationPlatform mPlatformApplication;
  /// Modules of functionality that need to be plumbed-in to the
  /// init/deinit/resize events of the app lifecycle.
  std::vector<ApplicationComponent*> mComponents;

  // Ban copying of objects of this type by making it a private operation:
private:
  Application(const Application&);
  Application& operator=(const Application&);
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PUBLIC_API_APPLICATION_H_ */
