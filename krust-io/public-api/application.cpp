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

// Class header:
#include "application.h"

// External headers:
#include <vulkan/vulkan.h>
#include <iostream>
#include <algorithm>
#include <string.h> // for memset.
#include "krust-common/public-api/krust-common.h"
#include "krust-common/public-api/logging.h"
#include "krust-common/public-api/vulkan-logging.h"
#include "krust-common/public-api/vulkan-utils.h"

// Internal headers:
#include "window.h"
#include "krust-io/internal/vulkan-helpers.h"

namespace Krust {
namespace IO {
namespace {
/// At least this number of images will be requested for the present swapchain.
const int MIN_NUM_SWAPCHAIN_IMAGES = 1;
/// The max time to wait to acquire an image to draw to from the WSI presentation engine.
const uint64_t PRESENT_IMAGE_ACQUIRE_TIMEOUT = UINT64_MAX;//5000;

/// @name Layers Layers to enable.
/// These are not used yet (turn on layers with env vars at commandline).
///@{
#if 0
/// Set of layers to enable at the physical device level.
const char * const DEVICE_LAYERS[] = {
  "DrawState",
  "MemTracker",
  "ObjectTracker",
  "ParamChecker",
  "ShaderChecker",
  "Threading",
};
const size_t NUM_DEVICE_LAYERS = sizeof(DEVICE_LAYERS) / sizeof(DEVICE_LAYERS[0]);

/// Set of layers to enable at the logical device level.
const char * const INSTANCE_LAYERS[] = {
  "DrawState",
  "MemTracker",
  "ObjectTracker",
  "ParamChecker",
  "ShaderChecker",
  "Threading",
};
const size_t NUM_INSTANCE_LAYERS = sizeof(INSTANCE_LAYERS) / sizeof(INSTANCE_LAYERS[0]);
#endif
///@}
}

Application::Application()
: mDefaultWindow(0),
  mAppName("Krust Application"),
  mAppVersion(0),
  mInstance(0),
  mGpu(0),
  mDefaultDrawingQueueFamily(unsigned(-1)),
  mDefaultPresentQueueFamily(unsigned(-1)),
  mGpuInterface(0),
  mQuit(false),
  mPlatformApplication(*this)
{

}

bool Application::Init()
{
  // Do platform-specific initialisation:
  if(!mPlatformApplication.Init())
  {
    return false;
  }

  // Open a default window:
  mDefaultWindow = WindowPointer(new Window(*this, mAppName));
  mPlatformApplication.WindowCreated(*mDefaultWindow.Get());

  // Start up vulkan:
  if(!InitVulkan())
  {
    return false;
  }

  // Setup defaults:

  // Create a command pool:
  VkCommandPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    poolInfo.pNext = nullptr,
    // Make all command buffers in the default pool resettable:
    // (create specialised pools in derived apps if necessary)
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    poolInfo.queueFamilyIndex = 0;

  const VkResult poolResult = vkCreateCommandPool(mGpuInterface, &poolInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mCommandPool);
  if(poolResult != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create command pool. Error: " << poolResult << Krust::endlog;
    return false;
  }

  // Allow derived application class to do its own setup:
  if(!DoPostInit())
  {
    return false;
  }
  return true;
}

bool Application::InitVulkan()
{
  if(!InitVulkanInstance())
  {
    return false;
  }

  // Get ready for the window / surface binding:
  mSurface = mPlatformApplication.InitSurface(mInstance);

  if(!InitVulkanGpus())
  {
    return false;
  }

  // Choose an image space and format compatible with the presentable surface:
  if(!ChoosePresentableSurfaceFormat())
  {
    return false;
  }

  // Get a queue to draw and to present on:
  if(!InitDefaultQueue())
  {
    return false;
  }


  // Build a depth buffer to be shared by all entries in the swapchain of color
  // images:
  if(!InitDepthBuffer(mDepthFormat))
  {
    return false;
  }

  // Build a swapchain to get the results of rendering onto a display:
  if(!InitDefaultSwapchain())
  {
    return false;
  }

  // Build a default commandpool:
  if(!InitDefaultCommandPool())
  {
    return false;
  }

  return true;
}

bool Application::InitVulkanInstance()
{
  // Give Vulkan some basic info about the app:
  VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    appInfo.pNext = NULL,
    appInfo.pApplicationName = mAppName,
    appInfo.applicationVersion = mAppVersion,
    appInfo.pEngineName = KRUST_ENGINE_NAME,
    appInfo.engineVersion = KRUST_ENGINE_VERSION_NUMBER,
    appInfo.apiVersion = VK_API_VERSION;

  // Find the extensions to initialise the API instance with:
  std::vector<VkExtensionProperties> extensionProperties = GetGlobalExtensionProperties();
  KRUST_LOG_INFO << "Number of global extensions: " << extensionProperties.size() << endlog;
  for(const auto& extension : extensionProperties )
  {
    KRUST_LOG_INFO << "\t Extension: " << extension.extensionName << ", version: " << extension.specVersion << endlog;
  }
  std::vector<const char*> extensionNames;

  // Get the extensions which let us draw something to a screen:
  if(FindExtension(extensionProperties, VK_KHR_SURFACE_EXTENSION_NAME))
  {
    extensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
  } else {
    return false;
  }
  const char* platformSurfaceExtensionName = mPlatformApplication.GetPlatformSurfaceExtensionName();
  if(FindExtension(extensionProperties, platformSurfaceExtensionName))
  {
    extensionNames.push_back(platformSurfaceExtensionName);
  } else {
    return false;
  }

  // Get the extension which lets us get debug information out of the Vk
  // implementation such as the results of running debug / validation layers:
  if(FindExtension(extensionProperties, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
  {
    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  // Setup the struct telling Vulkan about layers, extensions, and memory allocation
  // callbacks:
  VkInstanceCreateInfo instanceInfo;
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    instanceInfo.pNext = NULL,
    instanceInfo.flags = 0,
    instanceInfo.pApplicationInfo = &appInfo,
    instanceInfo.enabledLayerCount = 0,
    instanceInfo.ppEnabledLayerNames = 0,
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
    instanceInfo.ppEnabledExtensionNames = &extensionNames[0];

  KRUST_COMPILE_ASSERT(!KRUST_GCC_64BIT_X86_BUILD || sizeof(VkInstanceCreateInfo) == 64U, "VkInstanceCreateInfo size changed: recheck init code.");

  const VkResult result = vkCreateInstance(&instanceInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mInstance);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "vkCreateInstance() failed with result = " << ResultToString(result) <<  endlog;
    if(result == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
      KRUST_LOG_ERROR << "No compatible Vulkan driver could be found. Please consult the "
          "setup instructions that came with your Vulkan implementation or SDK. "
          "There may be some things you need to do with config files, environment "
          "variables or commandline voodoo to allow the Vulkan loader to find an ICD." << endlog;
    }
    return false;
  }

  // Setup the debug reporting function:
  // This lets us get information out of validation layers.
  mCreateDebugReportCallback = KRUST_GET_INSTANCE_EXTENSION(mInstance, CreateDebugReportCallbackEXT); //(PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(mInstance, "vkCreateDebugReportCallbackEXT");
  mDestroyDebugReportCallback = KRUST_GET_INSTANCE_EXTENSION(mInstance, DestroyDebugReportCallbackEXT); //  (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(mInstance, "vkDestroyDebugReportCallbackEXT");

  // Get the instance WSI extension funcions:
  mGetPhysicalDeviceSurfaceSupportKHR = KRUST_GET_INSTANCE_EXTENSION(mInstance, GetPhysicalDeviceSurfaceSupportKHR);
  mGetPhysicalDeviceSurfaceCapabilitiesKHR =          KRUST_GET_INSTANCE_EXTENSION(mInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
  mGetSurfaceFormatsKHR =               KRUST_GET_INSTANCE_EXTENSION(mInstance, GetPhysicalDeviceSurfaceFormatsKHR);
  mGetSurfacePresentModesKHR =          KRUST_GET_INSTANCE_EXTENSION(mInstance, GetPhysicalDeviceSurfacePresentModesKHR);
  if(mGetPhysicalDeviceSurfaceSupportKHR == 0 || mGetPhysicalDeviceSurfaceCapabilitiesKHR == 0 ||
     mGetSurfaceFormatsKHR == 0 || mGetSurfacePresentModesKHR == 0 )
  {
    KRUST_LOG_ERROR << "Failed to get function pointers for WSI instanace extension." << endlog;
    return false;
  }

  if(mCreateDebugReportCallback)
  {
    // Ask Vulkan to pump errors out our callback function:
    VkDebugReportCallbackCreateInfoEXT debugCreateInfo;
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT,
      debugCreateInfo.pNext = nullptr,
      debugCreateInfo.flags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
      VK_DEBUG_REPORT_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_ERROR_BIT_EXT |
      VK_DEBUG_REPORT_DEBUG_BIT_EXT,
      debugCreateInfo.pfnCallback = DebugCallback,
      debugCreateInfo.pUserData = nullptr;

    mCreateDebugReportCallback(mInstance, &debugCreateInfo,
                               KRUST_DEFAULT_ALLOCATION_CALLBACKS,
                               &mDebugCallbackHandle);
    if(!mDebugCallbackHandle)
    {
      KRUST_LOG_WARN << "Failed to create debug callback object. You will not see validation output." << endlog;
    }
  }

  return true;
}

bool Application::InitQueueInfo()
{
  mPhysicalQueueFamilyProperties = GetPhysicalDeviceQueueFamilyProperties(mGpu);
  KRUST_LOG_INFO << "Number of physical device queue families: " << mPhysicalQueueFamilyProperties.size() << endlog;
  if(mPhysicalQueueFamilyProperties.size() < 1u)
  {
    return false;
  }

  for(auto queueProperties : mPhysicalQueueFamilyProperties)
  {
    KRUST_LOG_INFO << "\tPhysical queue family: (queueCount = " << queueProperties.queueCount <<
    ", queueFlags = [" << (queueProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT ? " Graphics |" : " |")
    << (queueProperties.queueFlags & VK_QUEUE_COMPUTE_BIT  ? " Compute |" : " |")
    << (queueProperties.queueFlags & VK_QUEUE_TRANSFER_BIT  ? " Transfer |" : " |")
    << (queueProperties.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT  ? " Sparse ]" : " ]")
    << ", timestampValidBits = " << queueProperties.timestampValidBits
    << ", minImageTransferGranularity = {"
       << queueProperties.minImageTransferGranularity.width << ", "
       << queueProperties.minImageTransferGranularity.height << ", "
       << queueProperties.minImageTransferGranularity.depth << "})." << endlog;
  }

  // Record which queues can present frames to the windowing system of the
  // platform:
  RecordPresentQueueFamilies();

  // Remember the queue family that can do graphics and also present:
  unsigned queueFamilyIndex = 0U-1;
  for (unsigned i = 0; i < mPhysicalQueueFamilyProperties.size(); ++i) {
    if ((mPhysicalQueueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        (mQueueFamilyPresentFlags[i])) {
      queueFamilyIndex = i;
      break;
    }
  }
  if (queueFamilyIndex == unsigned(-1)) {
    KRUST_LOG_ERROR << "Could not find a device queue which allows graphics and present." << endlog;
    return false;
  }
  mDefaultDrawingQueueFamily = queueFamilyIndex;
  mDefaultPresentQueueFamily = queueFamilyIndex;
  return true;
}

bool Application::InitVulkanGpus() {
  std::vector<VkPhysicalDevice> gpus = EnumeratePhysicalDevices(mInstance);
  KRUST_LOG_INFO << "Number of GPUs: " << gpus.size() << endlog;
  if(gpus.size() < 1u) {
    KRUST_LOG_ERROR << "No GPUs found" << endlog;
    return false;
  }
  // Just pick the first one for now: (later we probably want to choose one capable of displaying and with the right queue types)
  mGpu = gpus[0];

  // Get the info for our GPU:

  vkGetPhysicalDeviceProperties(mGpu, &mGpuProperties);
  LogVkPhysicalDeviceLimits(mGpuProperties.limits);

  vkGetPhysicalDeviceFeatures(mGpu, &mGpuFeatures);
  LogVkPhysicalDeviceFeatures(mGpuFeatures);

  vkGetPhysicalDeviceMemoryProperties(mGpu, &mGpuMemoryProperties);

  std::vector<VkLayerProperties> layerProperties =
    EnumerateDeviceLayerProperties(mGpu);
  KRUST_LOG_INFO << "Number of GPU layers: " << layerProperties.size() << endlog;

  // Grab the extensions for the GPU and make sure the WSI one is there:
  std::vector<VkExtensionProperties> extensionProperties =
    EnumerateDeviceExtensionProperties(mGpu, 0);
  KRUST_LOG_INFO << "Found "  << extensionProperties.size() << " GPU device extensions." << endlog;
  for(auto extension : extensionProperties)
  {

    KRUST_LOG_INFO << "\tExtension: " << extension.extensionName << ", version: " << extension.specVersion << endlog;
  }
  std::vector<const char*> extensionNames;
  if(!FindExtension(extensionProperties, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
  {
    KRUST_LOG_ERROR << "Unable to find " << VK_KHR_SWAPCHAIN_EXTENSION_NAME << endlog;
    return false;
  }
  extensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  // Get info for all queues on the GPU and log it:

  if(!InitQueueInfo())
  {
    return false;
  }

  // Create the "logical/software" GPU object:

  // Setup queue info object to tell Vulkan which queues we want:
  // This currently only asks for one combined graphics/present queue.
  KRUST_ASSERT1(mDefaultDrawingQueueFamily == mDefaultPresentQueueFamily, "Graphics and present queues not the same.");

  /// This should be an array, one per queue family I want.
  const float queuePriorities[1] = { 0.0f };
  VkDeviceQueueCreateInfo queueCreateInfo;
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    queueCreateInfo.pNext = 0,
    queueCreateInfo.flags = 0,
    queueCreateInfo.queueFamilyIndex = mDefaultPresentQueueFamily, ///< This is the family chosen after checking it is good for WSI present as well as for graphics.
    queueCreateInfo.queueCount = 1, ///< We only need one queue from the family.
    queueCreateInfo.pQueuePriorities = queuePriorities;
    // If above causes problems, use: mPhysicalQueueFamilyProperties[mDefaultPresentQueueFamily].queueCount;



  // Turn everything required by application on:
  VkPhysicalDeviceFeatures enabledPhysicalDeviceFeatures = DoDeviceFeatureConfiguration(mGpuFeatures);

  // Request a device with one queue, no layers enabled, and the extensions we setup above:
  VkDeviceCreateInfo deviceInfo;
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    deviceInfo.pNext = 0,
    deviceInfo.flags = 0,
    deviceInfo.queueCreateInfoCount = 1,
    deviceInfo.pQueueCreateInfos = &queueCreateInfo,
    deviceInfo.enabledLayerCount = 0,
    deviceInfo.ppEnabledLayerNames = 0,
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
    deviceInfo.ppEnabledExtensionNames = &extensionNames[0],
    deviceInfo.pEnabledFeatures = &enabledPhysicalDeviceFeatures;



  const VkResult result = vkCreateDevice(mGpu, &deviceInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mGpuInterface);
  if(VK_SUCCESS != result)
  {
    KRUST_LOG_ERROR << "Failed to create logical GPU. Error: " << ResultToString(result) << endlog;
    return false;
  }

  // Get the device WSI extensions:
  if( 0 == (mAcquireNextImageKHR = KRUST_GET_DEVICE_EXTENSION(mGpuInterface, AcquireNextImageKHR)) ||
      0 == (mCreateSwapChainKHR = KRUST_GET_DEVICE_EXTENSION(mGpuInterface, CreateSwapchainKHR)) ||
      0 == (mDestroySwapChainKHR = KRUST_GET_DEVICE_EXTENSION(mGpuInterface, DestroySwapchainKHR)) ||
      0 == (mGetSwapchainImagesKHR = KRUST_GET_DEVICE_EXTENSION(mGpuInterface, GetSwapchainImagesKHR)) ||
      0 == (mQueuePresentKHR = KRUST_GET_DEVICE_EXTENSION(mGpuInterface, QueuePresentKHR)) )
  {
    return false;
  }

  return true;
}

bool Application::InitDefaultQueue()
{
  KRUST_ASSERT1(mDefaultDrawingQueueFamily == mDefaultPresentQueueFamily,
                "Only one queue implemented presently");
  const uint32_t queueFamilyIndex = mDefaultPresentQueueFamily;

  // Create a default queue:
  vkGetDeviceQueue(
      mGpuInterface,
      queueFamilyIndex, //< queueFamilyIndex
      0, //< [queueindex] Default to using the first queue in the family.
      &mDefaultQueue);

  mDefaultPresentQueue = &mDefaultQueue;
  mDefaultGraphicsQueue = &mDefaultQueue;
  return true;
}

bool Application::ChoosePresentableSurfaceFormat()
{
  // Choose an image space and format compatible with the presentable surface:
  std::vector<VkSurfaceFormatKHR> surfaceFormats =
    GetSurfaceFormatsKHR(mGetSurfaceFormatsKHR,
                         mGpu,
                         mSurface);
  KRUST_LOG_INFO << "Num formats compatible with default window surface:" << surfaceFormats.size() << endlog;
  for(auto format : surfaceFormats)
  {
    KRUST_LOG_INFO << "\tFormat: " << FormatToString(format.format) << ", Colorspace: " << KHRColorspaceToString(format.colorSpace) << endlog;
  }
  if(surfaceFormats.size() == 0)
  {
    KRUST_LOG_ERROR << "Failed to find any surface-compatible formats." << endlog;
    return false;
  }
  VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
  VkColorSpaceKHR colorspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  bool found = false;
  for(auto surfaceFormat : surfaceFormats)
  {
    if((surfaceFormat.format != VK_FORMAT_UNDEFINED) &&
       (surfaceFormat.format <= VK_FORMAT_END_RANGE) &&
       (surfaceFormat.colorSpace >= VK_COLORSPACE_BEGIN_RANGE &&
        surfaceFormat.colorSpace <= VK_COLORSPACE_END_RANGE ))
    {
      // We currently just choose the first okay surfaceFormat we find:
      // (this should be configurable)
      format = surfaceFormat.format;
      colorspace = surfaceFormat.colorSpace;
      found = true;
      break;
    }
  }
  if(!found)
  {
    KRUST_LOG_WARN << "Using default format and colorspace for images to present to window surface [THIS MAY NOT WORK]." << endlog;
  }

  mFormat = format;
  mColorspace = colorspace;

  return true;
}

bool Application::InitDepthBuffer(const VkFormat depthFormat)
{
  // Create an image object for the depth buffer upfront so we can query the
  // amount of storage required for it from the Vulkan implementation:
  const unsigned width = mDefaultWindow->GetPlatformWindow().GetWidth();
  const unsigned height = mDefaultWindow->GetPlatformWindow().GetHeight();
  VkImage depthImage = CreateDepthImage(
    mGpuInterface,
    mDefaultPresentQueueFamily,
    depthFormat,
    width,
    height);

  if(!depthImage)
  {
    return false;
  }
  // Make sure we clean up if we exit off the expected path:
  ScopedImageOwner depthJanitor(mGpuInterface, depthImage);

  // Work out how much memory the depth image requires:
  VkMemoryRequirements depthMemoryRequirements;
  vkGetImageMemoryRequirements(mGpuInterface, depthImage, &depthMemoryRequirements);
  KRUST_LOG_INFO << "Depth buffer memory requirements: (Size = " << depthMemoryRequirements.size << ", Alignment = " << depthMemoryRequirements.alignment << ", Flags = " << depthMemoryRequirements.memoryTypeBits << ")." << endlog;

  // Work out which memory type we can use:
  ConditionalValue<uint32_t> memoryType = FindMemoryTypeWithProperties(mGpuMemoryProperties, depthMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if(!memoryType)
  {
    KRUST_LOG_ERROR << "No memory suitable for depth buffer." << endlog;
    return false;
  }

  // Allocate the memory to back the depth image:
  VkMemoryAllocateInfo depthAllocationInfo;
    depthAllocationInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    depthAllocationInfo.pNext = nullptr,
    depthAllocationInfo.allocationSize = depthMemoryRequirements.size,
    depthAllocationInfo.memoryTypeIndex = memoryType.GetValue();

  ScopedDeviceMemoryOwner depthMemory(mGpuInterface, 0);
  const VkResult allocResult = vkAllocateMemory(mGpuInterface, &depthAllocationInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &depthMemory.memory);
  if(allocResult != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to allocate storage for depth buffer. Error: " << allocResult << endlog;
    return false;
  }

  // Tie the memory to the image:
  VK_CALL_RET(vkBindImageMemory, mGpuInterface, depthImage, depthMemory.memory, 0);

  // Create a view for the depth buffer image:
  VkImageView depthView = CreateDepthImageView(mGpuInterface, depthJanitor.image, depthFormat);
  if(!depthView)
  {
    return false;
  }

  mDepthBufferView = depthView;
  mDepthBufferImage = depthJanitor.Release();
  mDepthBufferMemory = depthMemory.Release();
  return true;
}

bool Application::InitDefaultSwapchain()
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  const VkResult result = mGetPhysicalDeviceSurfaceCapabilitiesKHR(mGpu, mSurface, &surfaceCapabilities);
  if(VK_SUCCESS != result)
  {
    KRUST_LOG_ERROR << "Failed to get surface capabilities. Error: " << ResultToString(result) << endlog;
    return false;
  }
  LogVkSurfaceCapabilitiesKHR(surfaceCapabilities);

  // Choose the fastest non-tearing present mode available:
  auto presentMode = ChooseBestPresentMode(false);
  if(!presentMode)
  {
    return false;
  }
  KRUST_LOG_INFO << "Using present mode " << presentMode.GetValue() << endlog;

  // Choose an extent for the surface:
  VkExtent2D extent = surfaceCapabilities.currentExtent;
  if(extent.width == uint32_t(-1))
  {
    KRUST_LOG_WARN << "Undefined surface extent. @" << __FILE__ << ":" << __LINE__ << endlog;
    // Try to force the surface extent to match the window but this is untested:
    extent.width = this->mDefaultWindow->GetPlatformWindow().GetWidth();
    extent.height = this->mDefaultWindow->GetPlatformWindow().GetHeight();
  }
  else
  {
    // Warn if the Vk surface dimensions don't match the window's in release:
    if(extent.width != this->mDefaultWindow->GetPlatformWindow().GetWidth())
    {
      KRUST_LOG_WARN << "Surface width doesn't match window." << extent.width << " != " << this->mDefaultWindow->GetPlatformWindow().GetWidth() << endlog;
    }
    if(extent.height != this->mDefaultWindow->GetPlatformWindow().GetHeight())
    {
      KRUST_LOG_WARN << "Surface height doesn't match window." << extent.height << " != " << this->mDefaultWindow->GetPlatformWindow().GetHeight() << endlog;
    }
    // Die in debug:
    KRUST_ASSERT1(extent.width == this->mDefaultWindow->GetPlatformWindow().GetWidth(), "Surface doesn't match window.");
    KRUST_ASSERT1(extent.height == this->mDefaultWindow->GetPlatformWindow().GetHeight(), "Surface doesn't match window.");
  }

  // Work out a good number of framebuffers to juggle in the presentation queue:
  // (the higher, the more GPU memory used and the greater latency (bad), but the
  // higher the potential throughput (good)).
  uint32_t minNumPresentationFramebuffers = surfaceCapabilities.minImageCount + 1;
  if(surfaceCapabilities.maxImageCount > 0 && // 0 -> unlimited.
                                              surfaceCapabilities.maxImageCount < minNumPresentationFramebuffers)
  {
    minNumPresentationFramebuffers = surfaceCapabilities.maxImageCount;
  }
  // minNumPresentationFramebuffers = 2; ///< Try hardcoding this if there are any present issues.
  mNumSwapchainFramebuffers = minNumPresentationFramebuffers;
  KRUST_LOG_INFO << "Using " << mNumSwapchainFramebuffers << " swap chain framebuffer images." << endlog;

  VkSwapchainCreateInfoKHR swapChainCreateParams;
    swapChainCreateParams.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    swapChainCreateParams.pNext = 0,
    swapChainCreateParams.flags = 0,
    swapChainCreateParams.surface = mSurface,
    swapChainCreateParams.minImageCount = minNumPresentationFramebuffers,
    swapChainCreateParams.imageFormat = mFormat,
    swapChainCreateParams.imageColorSpace = mColorspace,
    swapChainCreateParams.imageExtent = extent,
    swapChainCreateParams.imageArrayLayers = 1,
    swapChainCreateParams.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    swapChainCreateParams.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    // We are not sharing the swap chain between queues so there are 0,null:
    swapChainCreateParams.queueFamilyIndexCount = 0,
    swapChainCreateParams.pQueueFamilyIndices = nullptr,
    swapChainCreateParams.preTransform = surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfaceCapabilities.currentTransform,
    swapChainCreateParams.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    swapChainCreateParams.presentMode = presentMode.GetValue(),
    swapChainCreateParams.clipped = true,
    swapChainCreateParams.oldSwapchain = 0;

  VkResult swapChainCreated = mCreateSwapChainKHR(mGpuInterface, &swapChainCreateParams, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mSwapChain);
  if(VK_SUCCESS != swapChainCreated)
  {
    KRUST_LOG_ERROR << "Failed to create swap chain. Error: " << ResultToString(swapChainCreated) << ". Numerical error code: " << int(swapChainCreated) << endlog;
    return false;
  }

  mSwapChainImages = GetSwapChainImages(mGetSwapchainImagesKHR, mGpuInterface, mSwapChain);
  if(mSwapChainImages.size() < MIN_NUM_SWAPCHAIN_IMAGES)
  {
    KRUST_LOG_ERROR << "Too few swap chain images. Got " << mSwapChainImages.size()
      << ", but need at least " << MIN_NUM_SWAPCHAIN_IMAGES << endlog;
    return false;
  }
  KRUST_LOG_INFO << "Got " << mSwapChainImages.size() << " swapchain images." << endlog;

  // Setup a views for swapchain images so they can be bound to FrameBuffer
  // objects as render targets.
  mSwapChainImageViews.reserve(mSwapChainImages.size());
  VkImageViewCreateInfo chainImageCreate;
    chainImageCreate.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    chainImageCreate.pNext = nullptr,
    chainImageCreate.flags = 0,
    chainImageCreate.image = nullptr,
    chainImageCreate.viewType = VK_IMAGE_VIEW_TYPE_2D,
    chainImageCreate.format = mFormat,
    chainImageCreate.components.r = VK_COMPONENT_SWIZZLE_R,
    chainImageCreate.components.g = VK_COMPONENT_SWIZZLE_G,
    chainImageCreate.components.b = VK_COMPONENT_SWIZZLE_B,
    chainImageCreate.components.a = VK_COMPONENT_SWIZZLE_A,
    chainImageCreate.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
    chainImageCreate.subresourceRange.baseMipLevel = 0,
    chainImageCreate.subresourceRange.levelCount = 1,
    chainImageCreate.subresourceRange.baseArrayLayer = 0,
    chainImageCreate.subresourceRange.layerCount = 1;

  unsigned imageIndex = 0;
  for(auto image : mSwapChainImages)
  {
    KRUST_LOG_DEBUG << "\tSwap chain image: " << image << endlog;
    chainImageCreate.image = mSwapChainImages[imageIndex++];
    VkImageView view;
    VK_CALL_RET(vkCreateImageView, mGpuInterface, &chainImageCreate, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &view);
    mSwapChainImageViews.push_back(view);
  }

  // Make a semaphore to control the swapchain:
  KRUST_ASSERT1(!mSwapChainSemaphore, "Semaphore already initialised.");
  VkSemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    semaphoreCreateInfo.pNext = nullptr,
    // Start signalled since the first time we wait, there will be no framebuffer
    // in flight for WSI signal on:
    semaphoreCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkResult semaphoreResult = vkCreateSemaphore(mGpuInterface, &semaphoreCreateInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mSwapChainSemaphore);
  if(semaphoreResult != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create the swapchain semaphore." << endlog;
    return false;
  }

  return true;
}

bool Application::InitDefaultCommandPool()
{
  VkCommandPoolCreateInfo poolInfo;
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    poolInfo.pNext = nullptr,
    poolInfo.flags = 0,
    poolInfo.queueFamilyIndex = mDefaultDrawingQueueFamily;

  VkResult result = vkCreateCommandPool(mGpuInterface, &poolInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &mDefaultCommandPool);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create command pool. Error: " << result << endlog;
    return false;
  }

  return true;
}

ConditionalValue<VkPresentModeKHR>
Application::ChooseBestPresentMode(const bool tearingOk) const
{
  std::vector<VkPresentModeKHR> surfacePresentModes =
    GetSurfacePresentModesKHR(mGetSurfacePresentModesKHR, mGpu, mSurface);
  if(surfacePresentModes.size() < 1)
  {
    KRUST_LOG_ERROR << "No surface present modes found." << endlog;
    return ConditionalValue<VkPresentModeKHR>(VK_PRESENT_MODE_FIFO_KHR, false);
  }

  std::sort(surfacePresentModes.begin(), surfacePresentModes.end(),
            [tearingOk](const VkPresentModeKHR& l, const VkPresentModeKHR& r) -> bool
            { return SortMetric(l, tearingOk) < SortMetric(r, tearingOk); }
  );

  KRUST_LOG_INFO << "Present modes found:" << endlog;
  for(auto &mode : surfacePresentModes)
  {
    KRUST_LOG_INFO << "\t" << mode << endlog;
  }

  const VkPresentModeKHR presentMode = surfacePresentModes[0];
  return ConditionalValue<VkPresentModeKHR>(presentMode, true);
}

void Application::RecordPresentQueueFamilies()
{
  KRUST_ASSERT1(0 != mGetPhysicalDeviceSurfaceSupportKHR, "NULL function pointer");
  KRUST_ASSERT1(mQueueFamilyPresentFlags.size() == 0, "Initing queues more than once.");
  for(unsigned queueFamily = 0; queueFamily < mPhysicalQueueFamilyProperties.size(); ++queueFamily)
  {
    VkBool32 supportsPresent = false;
    mGetPhysicalDeviceSurfaceSupportKHR(mGpu, queueFamily, mSurface, &supportsPresent);
    mQueueFamilyPresentFlags.push_back(supportsPresent != 0);
  }
}

bool Application::DeInit()
{
  // Give derived application first chance to cleanup:
  DoPreDeInit();

  if(mCommandPool)
  {
    vkDestroyCommandPool(mGpuInterface, mCommandPool, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }

  if(mSwapChainSemaphore)
  {
    vkDestroySemaphore(mGpuInterface, mSwapChainSemaphore, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }

  // No need to vkDestroyImage() as these images came from the swapchain extension:
  mSwapChainImages.clear();
  mDestroySwapChainKHR(mGpuInterface, mSwapChain, KRUST_DEFAULT_ALLOCATION_CALLBACKS);

  if(mDepthBufferView)
  {
    vkDestroyImageView(mGpuInterface, mDepthBufferView, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }
  if(mDepthBufferImage)
  {
    vkDestroyImage(mGpuInterface, mDepthBufferImage, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }
  if(mDepthBufferMemory)
  {
    vkFreeMemory(mGpuInterface, mDepthBufferMemory, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }
  if(mDefaultQueue)
  {
    /// No funcion vkDestroyDeviceQueue() [It came from a GetX(), not a CreateX(), so maybe no destruction is required.]
  }
  if(mSurface)
  {
    vkDestroySurfaceKHR(mInstance, mSurface, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }
  if(mGpuInterface)
  {
    vkDestroyDevice(mGpuInterface, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }

  if(mDebugCallbackHandle && mDestroyDebugReportCallback)
  {
    mDestroyDebugReportCallback(mInstance, mDebugCallbackHandle, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }

  if(mInstance)
  {
    vkDestroyInstance(mInstance, KRUST_DEFAULT_ALLOCATION_CALLBACKS);
  }

  mPlatformApplication.WindowClosing(*mDefaultWindow.Get());
  mDefaultWindow.Reset(0);
  mPlatformApplication.DeInit();
  return true;
}

int Application::Run(MainLoopType loopType)
{
  if(!Init())
  {
    KRUST_LOG_ERROR <<  "Initialization failed." << endlog;
    return -1;
  }

  mPlatformApplication.PreRun();

  // Pre-pump a few frames before settling down into the event loop:
  for (unsigned i = 0; i < this->mSwapChainImageViews.size(); ++i)
  {
    this->OnRedraw(*mDefaultWindow.Get());
  }

  if(loopType == MainLoopType::Busy)
  {
    // Poll input until no events left, dispatching them, then render:
    // Note, this busy loop only works properly on Win32 at the moment.
    KRUST_LOG_INFO << "Running MAIN_LOOP_TYPE::Busy.\n";
    
    while (!mQuit) {
      while(mPlatformApplication.PeekAndDispatchEvent()){
        // Drain all events so we are up to date before rendering.
      }
      // Render:
      this->OnRedraw(*mDefaultWindow.Get());
    }
  }
  else
  {
    // Simple loop, receives and dispatches events. Drawing only happens when the
    // window system prompts it by sending a repaint event.
    KRUST_LOG_INFO << "Running MAIN_LOOP_TYPE::Reactive.\n";
  
    while(!mQuit){
      mPlatformApplication.WaitForAndDispatchEvent();
    }
  }

  DeInit();
  return 0;
}

VkPhysicalDeviceFeatures Application::DoDeviceFeatureConfiguration(
  const VkPhysicalDeviceFeatures &features)
{
  // Default to leaving everything turned on:
  return features;
}

bool Application::DoPostInit()
{
  return true;
}

bool Application::DoPreDeInit()
{
  KRUST_LOG_INFO << "Application did not do any cleanup." << endlog;
  return true;
}

void Application::OnResize(Window&, unsigned, unsigned) {
    KRUST_LOG_INFO << "Default OnResize() called.\n";
}

void Application::OnRedraw(Window& window) {
  // Verbose: KRUST_LOG_INFO << "Default OnRedraw() called.\n";
  surpress_unused(&window);

  if(!mAcquireNextImageKHR || !mQueuePresentKHR)
  {
    KRUST_LOG_DEBUG << "WSI extension pointers not initialised." << endlog;
    return;
  }

  // Acquire an image to draw into from the WSI:
  const VkResult acquireResult = mAcquireNextImageKHR(mGpuInterface, mSwapChain,
    PRESENT_IMAGE_ACQUIRE_TIMEOUT, mSwapChainSemaphore,
    nullptr /* no fence used! */, &mCurrentTargetImage);

  if(VK_SUCCESS != acquireResult)
  {
    if(acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
      KRUST_LOG_WARN << "Need to handle resize." << endlog;
    }
    else if(acquireResult == VK_SUBOPTIMAL_KHR)
    {
      static bool logged = false;
      if(!logged)
      {
        KRUST_LOG_WARN << "Suboptimal swapchain." << endlog;
        logged = true;
      }
    }
    else
    {
      KRUST_LOG_ERROR << "Failed to acquire an image from the swapchain. Error: " << acquireResult << endlog;
    }
  }

  if(acquireResult == VK_SUCCESS || acquireResult == VK_SUBOPTIMAL_KHR)
  {
    // Defer the actual drawing to an overridable template function:
    DoDrawFrame();
  }

  // Hand the finished frame back to the WSI for presentation / display:
  VkResult swapResult = VK_RESULT_MAX_ENUM;
  VkPresentInfoKHR presentInfo;
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    presentInfo.pNext = nullptr,
    // No need to wait for any
    presentInfo.waitSemaphoreCount = 0,
    presentInfo.pWaitSemaphores = nullptr,
    presentInfo.swapchainCount = 1,
    presentInfo.pSwapchains = &mSwapChain,
    presentInfo.pImageIndices = &mCurrentTargetImage;
    presentInfo.pResults = &swapResult;


  const VkResult presentResult = mQueuePresentKHR(*mDefaultPresentQueue, &presentInfo);
  
  if(presentResult != VK_SUCCESS)
  {
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
      KRUST_LOG_WARN << "Need to resize the framebuffer chain." << endlog;
    }
    else if(presentResult == VK_SUBOPTIMAL_KHR)
    {
      KRUST_LOG_INFO << "The swapchain is suboptimal." << endlog;
    }
    else
    {
      KRUST_LOG_ERROR << "Failed to present a swapchain image through WSI. Error: " << presentResult << endlog;
    }
  }
}

void Application::OnKey(Window&, bool, KeyCode) {
  KRUST_LOG_INFO << "Default OnKey() called.\n";
}

void Application::OnClose(Window &window)
{
  surpress_unused(&window);
  KRUST_LOG_INFO << "Default OnClose() called.\n";
  mQuit = true;
}

// This is a template. You should copy what is in here into your own overriden
// function and fill it out with code to draw something.
void Application::DoDrawFrame()
{
  KRUST_ASSERT2(mDefaultPresentQueue, "Can't present to a null present queue.");
  static int callCount = 0;
  KRUST_LOG_WARN << "Default DoRedraw() called " << (++callCount) << " times. You should override this for your app." << endlog;

  // Do per-frame work like building dynamic command buffers and updating
  // uniforms here.

  // Your app should call vkQueueSubmit();
}

} /* namespace IO */
} /* namespace Krust */
