#ifndef KRUST_IO_PUBLIC_API_APPLICATION_GRAPHICS_SWAPCHAIN_H_
#define KRUST_IO_PUBLIC_API_APPLICATION_GRAPHICS_SWAPCHAIN_H_

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

// Module-internal headers:
#include "application-component.h"

// Module-external Krust headers
#include "krust/public-api/vulkan-objects-fwd.h"

// System headers:
#include <krust/public-api/vulkan_types_and_macros.h>
#include <vector>

namespace Krust {
namespace IO {

class Application;

/**
 * @brief An application component adding vulkan resources to allow
 * the swapchain to be drawn-into by by a graphics pipeline.
 */
class ApplicationGraphicsSwapchain : public ApplicationComponent {
public:
  ApplicationGraphicsSwapchain(Application& app);
  virtual bool Init(Application& app) override;
  virtual bool DeInit(Application& app) override;
  virtual void OnResize(Application& app, unsigned width, unsigned height) override;
private:
  /// @brief Build a depth buffer.
  bool InitDepthBuffer(Application& app, VkFormat depthFormat);

public:
  /// FrameBuffers for each image in swapchain:
  std::vector<VkFramebuffer> mSwapChainFramebuffers;
  /// Render Passes for each image in the swapchain:
  std::vector<VkRenderPass> mRenderPasses;
  /// Depth buffer image format.
  VkFormat mDepthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
  /** Depth buffer logical image. */
  ImagePtr mDepthBufferImage = 0;
  /** Depth buffer handle for backing device memory storage. */
  DeviceMemoryPtr mDepthBufferMemory = 0;
  /** View of depth buffer image. */
  VkImageView mDepthBufferView = 0;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PUBLIC_API_APPLICATION_GRAPHICS_SWAPCHAIN_H_ */
