#ifndef KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_
#define KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_

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
/**
 * @file Platform-specific aspects of the Application class and supporting
 * types and functions for the XCB / Linux X11 platform.
 */

// Internal includes:
#include "krust-io/public-api/application-interface.h"
#include "krust-io/public-api/window.h"

// External includes:
#include <xcb/xcb.h>
#include "krust/public-api/vulkan.h"
#include <bitset>


namespace Krust {
namespace IO {

class Window;
class ApplicationInterface;

/// @todo define this here: using InputTimestamp = xcb_timestamp_t;

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
  /// Register the derived application's interest in the specified keyboard scancodes.
  void ListenToScancodes(uint8_t* keycodes, size_t numKeys);
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
  const char* GetPlatformSurfaceExtensionName();
 
  ApplicationInterface& mCallbacks;
  xcb_connection_t *mXcbConnection;
  xcb_screen_t *mXcbScreen;
  /// The platform-neutral window.
  Window* mWindow;
  /// Raw scancodes that the app has registered interest in.
  std::bitset<256> mRegisteredKeys;
};

/**
 * @name MouseStateXCB Helpers to query the state passed with mouse move events.
 * @note Hardcoded numbers determined by experiment, my mouse, my PC, Ubuntu 21.04.
 */
///@{
  constexpr bool left(uint16_t state)   { return state & 256U; }
  constexpr bool middle(uint16_t state) { return state & 512U; }
  constexpr bool right(uint16_t state)  { return state & 1024U; }
  constexpr bool shift(uint16_t state)  { return state & 1U; }
  constexpr bool ctrl(uint16_t state)   { return state & 4U; }
  /// The key with the windows icon.
  constexpr bool super(uint16_t state)  { return state & 64U; }
  constexpr bool alt(uint16_t state)    { return state & 8U; }
  constexpr bool altgr(uint16_t state)  { return state & 128U; }
  constexpr bool caps(uint16_t state)   { return state & 2U; }
  constexpr bool numlock(uint16_t state){ return state & 16U; }
///@}

/// Convert two opaque timestamps into a duration in seconds.
float durationBetween(const InputTimestamp start, const InputTimestamp end);

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PLATFORM_XCB_PUBLIC_API_APPLICATION_PLATFORM_H_ */
