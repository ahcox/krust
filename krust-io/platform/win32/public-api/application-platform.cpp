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

#include "application-platform.h"

// Internal includes:
#include "krust-io/public-api/application-interface.h"

// External includes:
#include "krust/public-api/compiler.h"
#include "krust/public-api/vulkan-utils.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include "krust/public-api/vulkan_struct_init.h"
#include "krust/public-api/scoped-free.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/krust.h"
#include <vector>
#include <utility>

namespace Krust {
namespace IO {
  
extern const char * const KRUST_WINDOW_CLASS_NAME = "Krust Window";

/**
* @brief Table mapping from Windows window handles to the application
* objects waiting for their events.
*
* There should really only be one Application object in the system but we go
* through this table anyway just so we are not enforcing that restriction
* artificially.
*/
using DispatchEntry = std::pair<HWND, std::pair<ApplicationInterface*, Window*> >;
std::vector<DispatchEntry> sDispatchTable;

DispatchEntry MakeEntry(const HWND windowHandle, ApplicationInterface* application, Window* window)
{
  return DispatchEntry(windowHandle, std::pair<ApplicationInterface*, Window*>(application, window));
}

namespace
{

/**
 * Remove all references to a given window from the dispatch table.
 */
void RemoveTrackedWindow(Window* window)
{
  if (window)
  {
restartLoop:
    auto i = sDispatchTable.begin();
    auto end = sDispatchTable.end();
    for (; i != end; ++i)
    {
      if (i->second.second == window)
      {
        sDispatchTable.erase(i);
        goto restartLoop;
      }
    }
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  ApplicationInterface* application = 0;
  Window* window = 0;

  /// @note I see the sequence WM_GETMINMAXINFO, WM_NCCREATE, before CreateWindow() has even returned.

  // Get the application object to dispatch to:
  for(auto p : sDispatchTable)
  {
    if (p.first == hWnd && p.second.first != nullptr && p.second.second != nullptr)
    {
      application = p.second.first;
      window = p.second.second;
    }
  }
  if (application == nullptr)
  {
    KRUST_LOG_INFO << "Failed to dispatch message for window: " << hWnd << " (MSG: " << message << ")" << endlog;
    goto callDefault;
  }

  switch (message)
  {
  case WM_COMMAND:
  {
    const int wmId = LOWORD(wParam);
    // Parse the menu selections:
    switch (wmId)
    {
    case 0:
    default:
      KRUST_LOG_INFO << "Recieved unknown WM_COMMAND: " << wmId << endlog;
      goto callDefault;
    }
    break;
  }

  case WM_PAINT:
  {
    // KRUST_LOG_DEBUG << "WM_PAINT" << endlog;

    application->OnRedraw(*window);
    
    // We need to tell windows we have drawn the window contents or it will keep
    // sending WM_PAINT messages until we do:
    if (!ValidateRect(hWnd, nullptr))
    {
      KRUST_LOG_DEBUG << "Failed to validate window client area." << endlog;
    }
    break;
  }

  case WM_SIZE:
  {
    application->OnResize(*window, lParam & 0xffff, lParam & 0xffff0000 >> 16U);
    break;
  }

  case WM_CLOSE:
  {
    KRUST_LOG_DEBUG << "WM_CLOSE" << endlog;
    application->OnClose(*window);
    RemoveTrackedWindow(window);
    // Let the default handler call DestroyWindow() etc.:
    goto callDefault;
  }

  case WM_DESTROY:
  {
    KRUST_LOG_DEBUG << "WM_DESTROY" << endlog;
    // Exit if this is the last window:
    if(sDispatchTable.size() == 0U)
    {
      PostQuitMessage(0);
    }
    break;
  }

  default:
    // Verbose: KRUST_LOG_DEBUG << "WM_ message not handled: " << message << endlog;
    goto callDefault;
  }

  return 0;
callDefault:
  return DefWindowProc(hWnd, message, wParam, lParam);
}

ATOM RegisterWindowClass(HINSTANCE hInstance)
{
  WNDCLASSEX wcex;

  wcex.cbSize = sizeof(WNDCLASSEX);

  wcex.style = CS_HREDRAW | CS_VREDRAW;
  wcex.lpfnWndProc = WndProc;
  wcex.cbClsExtra = 0;
  wcex.cbWndExtra = 0;
  wcex.hInstance = hInstance;
  wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wcex.lpszMenuName = nullptr;
  wcex.lpszClassName = KRUST_WINDOW_CLASS_NAME;
  wcex.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

  return RegisterClassEx(&wcex);
}

}

} /* namespace IO */
} /* namespace Krust */

Krust::IO::ApplicationPlatform::ApplicationPlatform(ApplicationInterface& callbacks) :
    mCallbacks(callbacks)
{
}

bool Krust::IO::ApplicationPlatform::Init()
{
  // Get the HINSTANCE of the process manually:
  mInstanceHandle = GetModuleHandleA(nullptr);
  if(!mInstanceHandle)
  {
    KRUST_LOG_ERROR << "Failed to get the app HINSTANCE." << endlog;
    return false;
  }
  if(!RegisterWindowClass(mInstanceHandle))
  {
    KRUST_LOG_ERROR << "Failed to register a window class." << endlog;
    return false;
  }
  return true;
}

VkSurfaceKHR Krust::IO::ApplicationPlatform::InitSurface(VkInstance instance)
{
  auto createInfo = Win32SurfaceCreateInfoKHR(0, mInstanceHandle,
    this->mDefaultWindow->GetPlatformWindow().mWindow);

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  const VkResult result = vkCreateWin32SurfaceKHR(instance, &createInfo, Krust::GetAllocationCallbacks(), &surface);
  if (result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create Vk surface for window. Result: " << result << "." << endlog;
    surface = VK_NULL_HANDLE;

  }
  return surface;
}

void Krust::IO::ApplicationPlatform::DeInit()
{
  
}

void Krust::IO::ApplicationPlatform::PreRun()
{
}

void Krust::IO::ApplicationPlatform::WindowCreated(Window& window) {
  KRUST_ASSERT1(mDefaultWindow == 0, "Window created previously.");
  mDefaultWindow = &window;
}

void Krust::IO::ApplicationPlatform::WindowClosing(Window& window) {
  KRUST_ASSERT1(mDefaultWindow == &window, "We only support one window at the moment.");
  &window;
  mDefaultWindow = 0;
}

void Krust::IO::ApplicationPlatform::WaitForAndDispatchEvent()
{
  MSG msg;

  if (!GetMessage(&msg, 0, 0, 0)) /// @ToDo: Set min and max message filters to lessen the churn.
  {
    this->mCallbacks.OnClose(*mDefaultWindow);
  }
  
  TranslateMessage(&msg);
  DispatchMessage(&msg);
}

bool Krust::IO::ApplicationPlatform::PeekAndDispatchEvent()
{
  MSG msg;
  
  if(PeekMessage(&msg, 0, 0, 0, TRUE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    return true;
  }
  return false;
}
