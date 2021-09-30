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
#include "krust/public-api/compiler.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/vulkan-logging.h"
#include "krust/public-api/vulkan-utils.h"
#include <vulkan/vulkan.h>
#include <iostream>
#include <algorithm>
#include <string.h> // for memset.

// Internal headers:
#include "window.h"
#include "krust-io/internal/vulkan-helpers.h"

namespace Krust {
namespace IO {

const char* const KRUST_ENGINE_NAME = "Krust";
const uint32_t KRUST_ENGINE_VERSION_NUMBER = 0;

namespace {

/// At least this number of images will be requested for the present swapchain.
constexpr int MIN_NUM_SWAPCHAIN_IMAGES = 1;
/// The max time to wait to acquire an image to draw to from the WSI presentation engine.
constexpr uint64_t PRESENT_IMAGE_ACQUIRE_TIMEOUT = UINT64_MAX;

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
constexpr size_t NUM_DEVICE_LAYERS = sizeof(DEVICE_LAYERS) / sizeof(DEVICE_LAYERS[0]);

/// Set of layers to enable at the logical device level.
const char * const INSTANCE_LAYERS[] = {
  "DrawState",
  "MemTracker",
  "ObjectTracker",
  "ParamChecker",
  "ShaderChecker",
  "Threading",
};
constexpr size_t NUM_INSTANCE_LAYERS = sizeof(INSTANCE_LAYERS) / sizeof(INSTANCE_LAYERS[0]);
#endif
///@}
}

Application::Application()
: mWindow(0),
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

/// @todo Move this. Need a full rejig of the physical design of the project.
void push_back_unique(std::vector<ApplicationComponent*>& components, ApplicationComponent* const component)
{
  if(components.end() == std::find(begin(components), end(components), component)){
    components.push_back(component);
  }
}

void Application::AddComponent(ApplicationComponent& component)
{
  push_back_unique(mComponents, &component);
}

void Application::ListenToScancodes(uint8_t* keycodes, const size_t numKeys)
{
  if(keycodes && numKeys){
    mPlatformApplication.ListenToScancodes(keycodes, numKeys);
  }
}

bool Application::Init(const VkImageUsageFlags swapchainUsageOverrides)
{
  // Do platform-specific initialisation:
  if(!mPlatformApplication.Init())
  {
    return false;
  }

  // Open a default window:
  mWindow = WindowPointer(new Window(*this, mAppName));
  mPlatformApplication.WindowCreated(*mWindow.Get());

  // Start up vulkan:
  if(!InitVulkan(swapchainUsageOverrides))
  {
    return false;
  }

  for(auto component : mComponents)
  {
    component->Init(*this);
  }

  // Allow derived application class to do its own setup:
  if(!DoPostInit())
  {
    return false;
  }
  return true;
}

bool Application::InitVulkan(const VkImageUsageFlags swapchainUsageOverrides)
{
  if(!InitVulkanInstance())
  {
    return false;
  }

  // Get ready for the window / surface binding:
  mSurface = mPlatformApplication.InitSurface(*mInstance);
  if (mSurface == VK_NULL_HANDLE)
  {
    KRUST_LOG_ERROR << "Surface init returned a null surface." << endlog;
    return false;
  }

  if(!InitVulkanGpus())
  {
    return false;
  }

  // Choose an image space and format compatible with the presentable surface:
  if(!ChoosePresentableSurfaceFormatSpacePair(VK_FORMAT_B8G8R8A8_UNORM)) ///< @todo Let the app tell us what format it wants.
  {
    return false;
  }

  // Get a queue to draw and to present on:
  if(!InitDefaultQueue())
  {
    return false;
  }

  // Create a command pool:
  // (Make all command buffers in the default pool resettable)
  mCommandPool = CommandPool::New(*mGpuInterface, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0);


  // Build a swapchain to get the results of rendering onto a display:
  if(!InitDefaultSwapchain(swapchainUsageOverrides))
  {
    return false;
  }

  return true;
}

bool Application::InitVulkanInstance()
{
  // Give Vulkan some basic info about the app:
  auto appInfo = ApplicationInfo(
    mAppName,
    mAppVersion,
    KRUST_ENGINE_NAME,
    KRUST_ENGINE_VERSION_NUMBER,
    DoChooseVulkanVersion());

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
    KRUST_LOG_ERROR << "Failed to find extension " << VK_KHR_SURFACE_EXTENSION_NAME << endlog;
    return false;
  }
  const char* platformSurfaceExtensionName = mPlatformApplication.GetPlatformSurfaceExtensionName();
  if(FindExtension(extensionProperties, platformSurfaceExtensionName))
  {
    extensionNames.push_back(platformSurfaceExtensionName);
  } else {
    KRUST_LOG_ERROR << "Failed to find platform surface extension " << platformSurfaceExtensionName << endlog;
    return false;
  }

  // Get the extension which lets us get debug information out of the Vk
  // implementation such as the results of running debug / validation layers:
  if(FindExtension(extensionProperties, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
  {
    extensionNames.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
  }

  std::vector<const char*> layerNames;
  {
    auto availableLayers { EnumerateInstanceLayerProperties() };
    KRUST_LOG_INFO << "Number of instance layers: " << availableLayers.size() << endlog;
    for(const auto& layer : availableLayers )
    {
      KRUST_LOG_INFO << "\tLayer: " << layer.layerName <<
        "\n\t  spec version: " << layer.specVersion << 
        "\n\t  impl version: " << layer.implementationVersion <<
        "\n\t  description: \"" << layer.description << "\"" << endlog;

    }

    if(FindLayer(availableLayers, "VK_LAYER_KHRONOS_validation")){
      layerNames.push_back("VK_LAYER_KHRONOS_validation");
    }
  }



  // Setup the struct telling Vulkan about layers, extensions, and memory allocation
  // callbacks:
  auto instanceInfo = InstanceCreateInfo(0, &appInfo, layerNames.size(), &layerNames[0],
    static_cast<uint32_t>(extensionNames.size()), &extensionNames[0]);

  KRUST_COMPILE_ASSERT(!KRUST_GCC_64BIT_X86_BUILD || sizeof(VkInstanceCreateInfo) == 64U, "VkInstanceCreateInfo size changed: recheck init code.");

  try {
    mInstance = Instance::New(instanceInfo);
  }
  catch (KrustVulkanErrorException& ex)
  {
    const VkResult result = ex.mResult;
    KRUST_LOG_ERROR << "CreateInstance() failed with result = " << ResultToString(result) << endlog;
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER)
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
  mCreateDebugReportCallback = KRUST_GET_INSTANCE_EXTENSION((*mInstance), CreateDebugReportCallbackEXT);
  mDestroyDebugReportCallback = KRUST_GET_INSTANCE_EXTENSION(*mInstance, DestroyDebugReportCallbackEXT);

  // Get the instance WSI extension funcions:
  mGetPhysicalDeviceSurfaceSupportKHR = KRUST_GET_INSTANCE_EXTENSION(*mInstance, GetPhysicalDeviceSurfaceSupportKHR);
  mGetPhysicalDeviceSurfaceCapabilitiesKHR =          KRUST_GET_INSTANCE_EXTENSION(*mInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
  mGetSurfaceFormatsKHR =               KRUST_GET_INSTANCE_EXTENSION(*mInstance, GetPhysicalDeviceSurfaceFormatsKHR);
  mGetSurfacePresentModesKHR =          KRUST_GET_INSTANCE_EXTENSION(*mInstance, GetPhysicalDeviceSurfacePresentModesKHR);
  if(mGetPhysicalDeviceSurfaceSupportKHR == 0 || mGetPhysicalDeviceSurfaceCapabilitiesKHR == 0 ||
     mGetSurfaceFormatsKHR == 0 || mGetSurfacePresentModesKHR == 0 )
  {
    KRUST_LOG_ERROR << "Failed to get function pointers for WSI instanace extension." << endlog;
    return false;
  }

  if(mCreateDebugReportCallback)
  {
    // Ask Vulkan to pump errors out our callback function:
    auto debugCreateInfo = DebugReportCallbackCreateInfoEXT(
      VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
      VK_DEBUG_REPORT_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
      VK_DEBUG_REPORT_ERROR_BIT_EXT |
      VK_DEBUG_REPORT_DEBUG_BIT_EXT,
      PFN_vkDebugReportCallbackEXT(DebugCallback),
      nullptr);

    mCreateDebugReportCallback(*mInstance, &debugCreateInfo,
                               Krust::GetAllocationCallbacks(),
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
  std::vector<VkPhysicalDevice> gpus = EnumeratePhysicalDevices(*mInstance);
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

  DoExtendDeviceFeatureChain(mGpuFeatures);
  vkGetPhysicalDeviceFeatures2(mGpu, &mGpuFeatures);
  LogVkPhysicalDeviceFeatures(mGpuFeatures.features);

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
  auto queueCreateInfo = DeviceQueueCreateInfo(
    0,
    mDefaultPresentQueueFamily, ///< This is the family chosen after checking it is good for WSI present as well as for graphics.
    1, ///< We only need one queue from the family.
    queuePriorities);

  // Turn everything required by application on:
  DoCustomizeDeviceFeatureChain(mGpuFeatures);

  // Request a device with one queue, no layers enabled, and the extensions we
  // setup above:
  auto deviceInfo = DeviceCreateInfo(0, 1, &queueCreateInfo, 0, 0,
    static_cast<uint32_t>(extensionNames.size()), &extensionNames[0], nullptr);
  deviceInfo.pNext = &mGpuFeatures;

  mGpuInterface = Device::New(*mInstance, mGpu, deviceInfo);

  // Get the device WSI extensions:
  if( 0 == (mAcquireNextImageKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, AcquireNextImageKHR)) ||
      0 == (mCreateSwapChainKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, CreateSwapchainKHR)) ||
      0 == (mDestroySwapChainKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, DestroySwapchainKHR)) ||
      0 == (mGetSwapchainImagesKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, GetSwapchainImagesKHR)) ||
      0 == (mQueuePresentKHR = KRUST_GET_DEVICE_EXTENSION(*mGpuInterface, QueuePresentKHR)) )
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
      *mGpuInterface,
      queueFamilyIndex, //< queueFamilyIndex
      0, //< [queueindex] Default to using the first queue in the family.
      &mDefaultQueue);

  mDefaultPresentQueue = &mDefaultQueue;
  mDefaultGraphicsQueue = &mDefaultQueue;
  return true;
}

bool Application::ChoosePresentableSurfaceFormatSpacePair(VkFormat format)
{
  // Choose an image space and format compatible with the presentable surface:
  std::vector<VkSurfaceFormatKHR> surfaceFormats =
    GetSurfaceFormatsKHR(mGetSurfaceFormatsKHR,
                         mGpu,
                         mSurface);
  KRUST_LOG_INFO << "Num formats compatible with default window surface:" << surfaceFormats.size() << endlog;
  if(surfaceFormats.size() == 0)
  {
    KRUST_LOG_ERROR << "Failed to find any surface-compatible formats." << endlog;
    return false;
  }
  for(auto format : surfaceFormats)
  {
    KRUST_LOG_INFO << "\tFormat: " << FormatToString(format.format) << ", Colorspace: " << KHRColorspaceToString(format.colorSpace) << endlog;
  }
  VkColorSpaceKHR colorspace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  bool found = false;
  // Try to find a pair using the format passed in:
  for(auto surfaceFormat : surfaceFormats)
  {
    // We assume they are listed best to worst and return the first one:
    if(surfaceFormat.format == format)
    {
      colorspace = surfaceFormat.colorSpace;
      found = true;
      break;
    }
  }
  // Grab _any_ format/space pair to at least try and run:
  if(!found){
    for(auto surfaceFormat : surfaceFormats)
    {
      if(surfaceFormat.format != VK_FORMAT_UNDEFINED)
      {
        // We currently just choose the first okay surfaceFormat we find:
        format = surfaceFormat.format;
        colorspace = surfaceFormat.colorSpace;
        found = true;
        break;
      }
    }
  }
  if(!found)
  {
    KRUST_LOG_WARN << "Using unsupported format and colorspace for images to present to window surface [THIS MAY NOT WORK]." << endlog;
  }

  mFormat = format;
  mColorspace = colorspace;

  return true;
}


bool Application::InitDefaultSwapchain(const VkImageUsageFlags swapchainUsageOverrides)
{
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  const VkResult result = mGetPhysicalDeviceSurfaceCapabilitiesKHR(mGpu, mSurface, &surfaceCapabilities);
  if(VK_SUCCESS != result)
  {
    KRUST_LOG_ERROR << "Failed to get surface capabilities. Error: " << ResultToString(result) << endlog;
    return false;
  }
  LogVkSurfaceCapabilitiesKHR(surfaceCapabilities);

  if((surfaceCapabilities.supportedUsageFlags & swapchainUsageOverrides) != swapchainUsageOverrides)
  {
    KRUST_LOG_ERROR << "Swapchain images do not support required usage flags (" << swapchainUsageOverrides << ')' << endlog;
    return false;
  }

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
    extent.width = this->mWindow->GetPlatformWindow().GetWidth();
    extent.height = this->mWindow->GetPlatformWindow().GetHeight();
  }
  else
  {
    // Warn if the Vk surface dimensions don't match the window's in release:
    if(extent.width != this->mWindow->GetPlatformWindow().GetWidth())
    {
      KRUST_LOG_WARN << "Surface width doesn't match window." << extent.width << " != " << this->mWindow->GetPlatformWindow().GetWidth() << endlog;
    }
    if(extent.height != this->mWindow->GetPlatformWindow().GetHeight())
    {
      KRUST_LOG_WARN << "Surface height doesn't match window." << extent.height << " != " << this->mWindow->GetPlatformWindow().GetHeight() << endlog;
    }
    // Die in debug:
    KRUST_ASSERT1(extent.width == this->mWindow->GetPlatformWindow().GetWidth(), "Surface doesn't match window.");
    KRUST_ASSERT1(extent.height == this->mWindow->GetPlatformWindow().GetHeight(), "Surface doesn't match window.");
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
  KRUST_LOG_INFO << "Using at least " << minNumPresentationFramebuffers << " swap chain framebuffer images." << endlog;

  auto swapChainCreateParams = SwapchainCreateInfoKHR();
    swapChainCreateParams.flags = 0,
    swapChainCreateParams.surface = mSurface,
    swapChainCreateParams.minImageCount = minNumPresentationFramebuffers,
    swapChainCreateParams.imageFormat = mFormat,
    swapChainCreateParams.imageColorSpace = mColorspace,
    swapChainCreateParams.imageExtent = extent,
    swapChainCreateParams.imageArrayLayers = 1,
    swapChainCreateParams.imageUsage = swapchainUsageOverrides ? swapchainUsageOverrides : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    swapChainCreateParams.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    // We are not sharing the swap chain between queues so there are 0,null:
    swapChainCreateParams.queueFamilyIndexCount = 0,
    swapChainCreateParams.pQueueFamilyIndices = nullptr,
    swapChainCreateParams.preTransform = surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : surfaceCapabilities.currentTransform,
    swapChainCreateParams.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    swapChainCreateParams.presentMode = presentMode.GetValue(),
    swapChainCreateParams.clipped = true,
    swapChainCreateParams.oldSwapchain = 0;

  VkResult swapChainCreated = mCreateSwapChainKHR(*mGpuInterface, &swapChainCreateParams, Krust::GetAllocationCallbacks(), &mSwapChain);
  if(VK_SUCCESS != swapChainCreated)
  {
    KRUST_LOG_ERROR << "Failed to create swap chain. Error: " << ResultToString(swapChainCreated) << ". Numerical error code: " << int(swapChainCreated) << endlog;
    return false;
  }

  mSwapChainImages = GetSwapChainImages(mGetSwapchainImagesKHR, *mGpuInterface, mSwapChain);
  if(mSwapChainImages.size() < MIN_NUM_SWAPCHAIN_IMAGES)
  {
    KRUST_LOG_ERROR << "Too few swap chain images. Got " << mSwapChainImages.size()
      << ", but need at least " << MIN_NUM_SWAPCHAIN_IMAGES << endlog;
    return false;
  }
  KRUST_LOG_INFO << "Got " << mSwapChainImages.size() << " swapchain images." << endlog;

  // Transition the framebuffer images from undefined layout to one for presentation
  // so that the first use in a render loop is the same as subsequent uses:
  for(auto& image : mSwapChainImages)
  {
    auto imb = ImageMemoryBarrier();
    imb.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    imb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    imb.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    imb.image = image,
    imb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    const auto layoutResult = ApplyImageBarrierBlocking(*mGpuInterface, image, mDefaultQueue, *mCommandPool, imb);
    if (VK_SUCCESS != layoutResult)
    {
      KRUST_LOG_ERROR << "Failed to change colour framebuffer image layout: " << ResultToString(layoutResult) << " [" __FILE__", " << __LINE__ << "]." << Krust::endlog;
      return false;
    }
  }

  // Setup a views for swapchain images so they can be bound to FrameBuffer
  // objects as render targets.
  mSwapChainImageViews.reserve(mSwapChainImages.size());
  auto chainImageCreate = ImageViewCreateInfo();
    chainImageCreate.flags = 0,
    chainImageCreate.image = nullptr,
    chainImageCreate.viewType = VK_IMAGE_VIEW_TYPE_2D,
    chainImageCreate.format = mFormat,
    chainImageCreate.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
    chainImageCreate.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
    chainImageCreate.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
    chainImageCreate.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
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
    VK_CALL_RET(vkCreateImageView, *mGpuInterface, &chainImageCreate, Krust::GetAllocationCallbacks(), &view);
    mSwapChainImageViews.push_back(view);
  }

  // Make a semaphore to control the swapchain:
  KRUST_ASSERT1(!mSwapChainSemaphore, "Semaphore already initialised.");
  auto semaphoreCreateInfo = SemaphoreCreateInfo(0);

  VkResult semaphoreResult = vkCreateSemaphore(*mGpuInterface, &semaphoreCreateInfo, Krust::GetAllocationCallbacks(), &mSwapChainSemaphore);
  if(semaphoreResult != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create the swapchain semaphore." << endlog;
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
  // Wait for all fences to signal end of frame:
  for(auto& fence : mSwapChainFences)
  {
    const VkResult fenceWaitResult = vkWaitForFences(*mGpuInterface, 1, fence->GetVkFenceAddress(), true, 100000000); // Wait one tenth of a second.
    if(VK_SUCCESS != fenceWaitResult)
    {
      KRUST_LOG_ERROR << "Wait for fences protecting resources in main render loop did not succeed: " << fenceWaitResult << Krust::endlog;
    }
  }
  
  // Give derived application first chance to cleanup:
  DoPreDeInit();

  for(auto i = rbegin(mComponents), e = rend(mComponents); i != e; ++i)
  {
    (*i)->DeInit(*this);
  }

  mCommandPool.Reset(nullptr);

  // Eagerly throw away command buffers which may be keeping alive lots of other
  // Vulkan objects:
  mCommandBuffers.clear();

  mSwapChainFences.clear();
  
  if(mSwapChainSemaphore)
  {
    vkDestroySemaphore(*mGpuInterface, mSwapChainSemaphore, Krust::GetAllocationCallbacks());
  }

  for(auto view : mSwapChainImageViews) {
    vkDestroyImageView(*mGpuInterface, view, Krust::GetAllocationCallbacks());
  };

  // No need to vkDestroyImage() as these images came from the swapchain extension:
  mSwapChainImages.clear();
  mDestroySwapChainKHR(*mGpuInterface, mSwapChain, Krust::GetAllocationCallbacks());

  if(mDefaultQueue)
  {
    /// No funcion vkDestroyDeviceQueue() [It came from a GetX(), not a CreateX(), so maybe no destruction is required.]
  }
  if(mSurface)
  {
    vkDestroySurfaceKHR(*mInstance, mSurface, Krust::GetAllocationCallbacks());
  }
  
  mGpuInterface.Reset(nullptr);


  if(mDebugCallbackHandle && mDestroyDebugReportCallback)
  {
    mDestroyDebugReportCallback(*mInstance, mDebugCallbackHandle, Krust::GetAllocationCallbacks());
  }

  // Explicitly release the instance now:
  KRUST_ASSERT1(mInstance->Count() == 1u, "Only the Application should still hold a reference to the Instance: we are going down.");
  mInstance.Reset(nullptr);

  mPlatformApplication.WindowClosing(*mWindow.Get());
  mWindow.Reset(nullptr);
  mPlatformApplication.DeInit();
  return true;
}

int Application::Run(const MainLoopType loopType, const VkImageUsageFlags swapchainUsageOverrides)
{
  // Init the Krust core:
  InitKrust(/* Default error policy and allocator for CPU structures. */);

  /// Sit on the main thread.
  ThreadBase threadBase(Krust::GetGlobalErrorPolicy());

  // Init ourselves:
  bool initialized = false;
  try {
    initialized = Init(swapchainUsageOverrides);
  }
  catch (KrustException& ex)
  {
    auto logBuilder = KRUST_LOG_ERROR;
    logBuilder << "Application initialization failed with Krust exception: ";
    ex.Log(logBuilder);
    logBuilder << endlog;
  }
  catch (std::exception& ex)
  {
    KRUST_LOG_ERROR << "Application initialization failed with standard exception: " << ex.what() << endlog;
  }
  if(!initialized)
  {
    KRUST_LOG_ERROR <<  "Initialization failed." << endlog;
    return -1;
  }

  // Lets go:
  try {
    mPlatformApplication.PreRun();

    // Pre-pump a few frames before settling down into the event loop:
    for (unsigned i = 0; i < this->mSwapChainImageViews.size(); ++i)
    {
      this->OnRedraw();
    }

    if (loopType == MainLoopType::Busy)
    {
      // Poll input until no events left, dispatching them, then render:
      // Note, this busy loop only works properly on Win32 at the moment.
      KRUST_LOG_INFO << "Running MAIN_LOOP_TYPE::Busy.\n";

      while (!mQuit) {
        while (mPlatformApplication.PeekAndDispatchEvent()) {
          // Drain all events so we are up to date before rendering.
        }
        // Render:
        this->OnRedraw();
      }
    }
    else
    {
      // Simple loop, receives and dispatches events. Drawing only happens when the
      // window system prompts it by sending a repaint event.
      KRUST_LOG_INFO << "Running MAIN_LOOP_TYPE::Reactive.\n";

      while (!mQuit) {
        mPlatformApplication.WaitForAndDispatchEvent();
      }
    }
  }
  catch (KrustException& ex)
  {
    auto logBuilder = KRUST_LOG_ERROR;
    logBuilder << "Krust exception thrown during Application main loop: ";
    ex.Log(logBuilder);
    logBuilder << endlog;
  }
  catch (std::exception& ex)
  {
    KRUST_LOG_ERROR << "Standard exception thrown during Application main loop: " << ex.what() << endlog;
  }

  DeInit();
  return 0;
}

uint32_t Application::DoChooseVulkanVersion() const
{
  // We need 1.1 to use the VkPhysicalDeviceFeatures2 configuration mechanism.
  return VK_API_VERSION_1_1;
}

void Application::DoExtendDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features)
{
}
void Application::DoCustomizeDeviceFeatureChain(VkPhysicalDeviceFeatures2 &features)
{
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

void Application::DispatchResize(unsigned width, unsigned height)
{
  /// @todo Respond to resize.
  KRUST_LOG_INFO << "Application::DispatchResize(): Need to resize swapchain [ToDo]" << endlog;

  // Let any optional components respond to the size:
  for(auto comp : mComponents){
    comp->OnResize(*this, width, height);
  }

  // Let derived concrete application class know about the resize:
  this->OnResize(width, height);
}

void Application::OnResize(unsigned w, unsigned h) {
    KRUST_LOG_INFO << "Default Application::OnResize() called (" << w << ", " << h << ").\n";
}

void Application::OnRedraw() {
  // Verbose: KRUST_LOG_INFO << "Default OnRedraw() called.\n";

  if(!mAcquireNextImageKHR || !mQueuePresentKHR)
  {
    KRUST_LOG_DEBUG << "WSI extension pointers not initialised." << endlog;
    return;
  }

  // Acquire an image to draw into from the WSI:
  const VkResult acquireResult = mAcquireNextImageKHR(
    *mGpuInterface,
    mSwapChain,
    PRESENT_IMAGE_ACQUIRE_TIMEOUT,
    mSwapChainSemaphore,
    nullptr, // no fence used!
    &mCurrentTargetImage);

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
  auto presentInfo = PresentInfoKHR();
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

void Application::OnKey(const bool up, const KeyCode keycode) {
  KRUST_LOG_INFO << "Default OnKey() called: key scancode: " << int(keycode) << (up ? ", up." : ", down." ) << endlog;
}

void Application::OnMouseMove(InputTimestamp when, int x, int y, unsigned state)
{
  KRUST_LOG_INFO << "Default OnMouseMove() called: when: " << when << ", (x,y): (" << x << ", " << y << "), state: " << state  << endlog;// "  \r";
  if(left(state)){
    KRUST_LOG_INFO << "\tLEFT" << endlog;
  }
  if(middle(state)){
    KRUST_LOG_INFO << "\tMIDDLE" << endlog;
  }
  if(right(state)){
    KRUST_LOG_INFO << "\tRIGHT" << endlog;
  }
  if(shift(state)){
    KRUST_LOG_INFO << "\tSHIFT" << endlog;
  }
  if(ctrl(state)){
    KRUST_LOG_INFO << "\tCTRL" << endlog;
  }
  if(super(state)){
    KRUST_LOG_INFO << "\tSUPER" << endlog;
  }
    if(alt(state)){
    KRUST_LOG_INFO << "\tALT" << endlog;
  }
}

void Application::OnClose()
{
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
