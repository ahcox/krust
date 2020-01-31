#ifndef KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_
#define KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_

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

// Internal includes:
#include "krust-io/public-api/window.h"

// External includes:
#include <xcb/xcb.h>
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>


namespace Krust {
namespace IO {

class Window;

class ApplicationInterface;

/**
 * \brief A component encompassing the platform-specific portion of an
 * Application object.
 */
class ApplicationPlatform {
public:
  ApplicationPlatform(ApplicationInterface& callbacks);
  bool Init();
  /**
   * @brief Create a surface for the window.
   *
   * This is what we present rendered frames onto.
   * Should really be in Window class. */
  VkSurfaceKHR InitSurface(VkInstance instance);
  void DeInit();
  void PreRun();
  /** Call this so events can be dispatched by window. */
  void WindowCreated(Window& window);
  void WindowClosing(Window& window);
  /** Blocking event get. */
  void WaitForAndDispatchEvent();
  /** Non-blocking event get.
   * @return true if an event was dispatched, else false.*/
  bool PeekAndDispatchEvent();
private:
  void ProcessEvent(const xcb_generic_event_t *event);
public:

  const char* GetPlatformSurfaceExtensionName() { return VK_KHR_XCB_SURFACE_EXTENSION_NAME; }

  ApplicationInterface& mCallbacks;
  xcb_connection_t *mXcbConnection;
  xcb_screen_t *mXcbScreen;
  /** Observe window creation and destruction so can dispatch events by window
   * Later, this will be an unordered map or short array of windows.
   */
  Window* mDefaultWindow;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_ */
