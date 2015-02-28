#ifndef KRUST_IO_PLATFORM_XCB_PUBLIC_API_WINDOW_PLATFORM_H_
#define KRUST_IO_PLATFORM_XCB_PUBLIC_API_WINDOW_PLATFORM_H_

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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Krust {
namespace IO {

class Window;
class Application;
class ApplicationPlatform;

/**
 * \brief A component encompassing the platform-specific portion of an
 * Window object.
 */
class WindowPlatform {
  friend class Krust::IO::Window;
  friend class Krust::IO::ApplicationPlatform;
  WindowPlatform(Application& application, Window& container, const char* title = 0);
  ~WindowPlatform();

public:
  unsigned GetWidth() const { return mWidth; }
  unsigned GetHeight() const { return mHeight; }

private:
  Application& mApplication;

  HWND mWindow = 0;
  // Simple state:
  unsigned mWidth = 0;
  unsigned mHeight = 0;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PLATFORM_XCB_PUBLIC_API_WINDOW_PLATFORM_H_ */
