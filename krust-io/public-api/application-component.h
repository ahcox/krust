#ifndef KRUST_IO_PUBLIC_API_APPLICATION_COMPONENT_H_
#define KRUST_IO_PUBLIC_API_APPLICATION_COMPONENT_H_

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

namespace Krust {
namespace IO {

class Application;

/**
 * @brief Abstract base class for components which expose some reusable Vulkan
 * machinery that needs to know about the application lifecycle.
 */
class ApplicationComponent {
public:
  ApplicationComponent(Application& app);
  /**
   * @brief Called once Vulkan and a window are up and running so any extra
   * initialisation of the component can be done.
   */
  virtual bool Init(Application& app) = 0;

  /**
   * @brief Called while Vulkan and a window are still up and running so
   * the component can cleanup before the Application base class does.
   */
  virtual bool DeInit(Application& app) = 0;

  /**
   * @brief Called after the application base class has responded to the window
   * resizing and before the derived concrete application has.
   */
  virtual void OnResize(Application& app, unsigned width, unsigned height) = 0;
};

} /* namespace IO */
} /* namespace Krust */

#endif /* KRUST_IO_PUBLIC_API_APPLICATION_COMPONENT_H_ */
