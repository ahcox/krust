#ifndef KRUST_IO_PUBLIC_API_APPLICATION_INTERFACE_H_
#define KRUST_IO_PUBLIC_API_APPLICATION_INTERFACE_H_

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

#include "keyboard.h"
#include <cstdint>

namespace Krust {
namespace IO {

class Window;

using KeyCode = std::uint8_t;
constexpr bool KeyUp   {true};
constexpr bool KeyDown {false};
using InputTimestamp = std::uint32_t;

/**
 * @brief The interface used by the platform specific application to dispatch
 * events back to the portable Application class encompassing it.
 *
 * @note Although in public-api this is really an implementation detail.
 * Its only value to an application is to show all the OnX() template methods
 * which can be overridden in one place.
 */
class ApplicationInterface {
public:
  virtual void DispatchResize(unsigned width, unsigned height) = 0;
  virtual void OnRedraw() = 0;
  virtual void OnKey(bool up, KeyCode keycode) = 0;
  /** Called when the mouse moves within the window
   * @param state Platform specific indicator of mouse button and special key
   * up/down. Special keys include CTRL, ALT, Windows, Shift, ...
   */
  virtual void OnMouseMove(InputTimestamp when, int x, int y, unsigned state) = 0;
  /**
   * @brief The user has issued a close request for the window (e.g. by clicking
   * the little cross in its corner decoration).
   */
  virtual void OnClose() = 0;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PUBLIC_API_APPLICATION_INTERFACE_H_ */
