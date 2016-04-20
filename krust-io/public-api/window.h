#ifndef KRUST_IO_PUBLIC_API_WINDOW_H_
#define KRUST_IO_PUBLIC_API_WINDOW_H_

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
#include "public-api/window-platform.h"

// External includes:
#include "krust/public-api/ref-object.h"


namespace Krust {
namespace IO {

class Application;

class Window : public Krust::RefObject {
public:
  Window(Application& application, const char * title = 0);
  const WindowPlatform& GetPlatformWindow() const { return mPlatformWindow; }
  WindowPlatform& GetPlatformWindow() { return mPlatformWindow; }
private:
  Application& mApplication;
  WindowPlatform mPlatformWindow;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PUBLIC_API_WINDOW_H_ */
