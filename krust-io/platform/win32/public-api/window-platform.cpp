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

// Class include:
#include "window-platform.h"

// Internal includes:
#include "application.h"
#include "window.h"

// External includes:
#include "krust/public-api/scoped-free.h"
#include "krust/public-api/logging.h"
#include <string.h>

namespace Krust {

namespace IO {

extern const char * const KRUST_WINDOW_CLASS_NAME;
constexpr const char * KRUST_DEFAULT_WINDOW_TITLE = "Krust Application";

using DispatchEntry = std::pair<HWND, std::pair<ApplicationInterface*, Window*> >;
extern std::vector<DispatchEntry> sDispatchTable;
extern DispatchEntry MakeEntry(const HWND windowHandle, ApplicationInterface* application, Window* window);

namespace
{


}

WindowPlatform::WindowPlatform(Application& application, Window& container, const char* const title) :
    mApplication(application)
{
  const HWND hWindow = CreateWindow(
    KRUST_WINDOW_CLASS_NAME,
    title ? title : KRUST_DEFAULT_WINDOW_TITLE,
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
    nullptr, nullptr,
    mApplication.mPlatformApplication.mInstanceHandle,
    nullptr);
  if (!hWindow)
  {
    KRUST_UNRECOVERABLE_ERROR("Failed to create a window.", __FILE__, __LINE__);
  }
  mWindow = hWindow;
  sDispatchTable.push_back(MakeEntry(mWindow, &mApplication, &container));
  
  ShowWindow(hWindow, SW_SHOWDEFAULT);
  UpdateWindow(hWindow);

  // Remember the dimensions of the drawable internal space of the window:
  RECT internalDims;
  const BOOL gotInternalDimensions = GetClientRect(hWindow,&internalDims);
  if (!gotInternalDimensions)
  {
    KRUST_UNRECOVERABLE_ERROR("FATAL: Failed to retrieve window internal dimensions "
      "so will not be able to build matching surfaces to present.",
      __FILE__, __LINE__);
  }
  mWidth = internalDims.right - internalDims.left;
  mHeight = internalDims.bottom - internalDims.top;

  KRUST_LOG_INFO << "WindowPlatform created." << endlog;
}

WindowPlatform::~WindowPlatform() {
  KRUST_LOG_INFO << "WindowPlatform destroyed." << endlog;
}

} /* namespace Krust::IO */

} /* namespace Krust */
