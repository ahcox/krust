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

// External includes:
#include "krust/public-api/scoped-free.h"
#include "krust/public-api/logging.h"
#include <string.h>

namespace Krust {

namespace IO {

/**
 * @brief A helper struct to init a values array to be passed to XCB.
 */
struct XcbValueList
{
  XcbValueList()
  {
    // Zero the values array in the absence of documentation for how
    // the list must be terminated:
    memset(values, 0, sizeof(values[0]) * 32);
  }

  // Have a nice long list of values in the absence of documentation for how
  // long the list must be:
  uint32_t values[32];
};

WindowPlatform::WindowPlatform(
  Krust::IO::Application& application,
  Krust::IO::Window& container,
  const char* title) :
    mApplication(application),
    mDeleteWindowEventAtom(0)
{
  KRUST_LOG_INFO << "WindowPlatform created.\n";
  suppress_unused(&container, &title); ///@ToDo: Set the window title through XCB

  // Create the X11 window:
  KRUST_ASSERT1(Connection(), "Null connection.");
  if(Connection())
  {
    mXcbWindow = xcb_generate_id(Connection());
    KRUST_ASSERT1(mXcbWindow, "No window id.");
    if(!mXcbWindow)
    {
      KRUST_LOG_ERROR << "Failed to generate window ID." << endlog;
    }
    else
    {
      const uint32_t  mask =
          // Not sure having this background is a good idea for Vulkan:
          XCB_CW_BACK_PIXEL |
          XCB_CW_EVENT_MASK;
      XcbValueList values;
      values.values[0] = Screen()->black_pixel;
      values.values[1] =
        XCB_EVENT_MASK_EXPOSURE         |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_FOCUS_CHANGE     |
        // The app should control which of these are subscribed to:
        XCB_EVENT_MASK_BUTTON_PRESS     | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_POINTER_MOTION   |
        XCB_EVENT_MASK_ENTER_WINDOW     | XCB_EVENT_MASK_LEAVE_WINDOW   |
        XCB_EVENT_MASK_KEY_PRESS        | XCB_EVENT_MASK_KEY_RELEASE
      ;

      // Calculate a reasonable width and height:
      // Note in multimonitor setups this screen is likely the whole virtual
      // screen encompassing both monitors. We should really figure out the
      // primary monitor and size ourself to fit onto that:
      // https://stackoverflow.com/questions/36966900/xcb-get-all-monitors-ands-their-x-y-coordinates
      const unsigned windowClientWidth  = std::min(1800u, Screen()->width_in_pixels  / 4u * 3u);
      const unsigned windowClientHeight = std::min(windowClientWidth * 100000 / 177778,  Screen()->height_in_pixels / 4u * 3u);
      KRUST_LOG_INFO << "windowClientWidth: "  << windowClientWidth  << '.' << endlog;
      KRUST_LOG_INFO << "windowClientHeight: " << windowClientHeight << '.' << endlog;
      mWidth = windowClientWidth;
      mHeight = windowClientHeight;

      xcb_create_window(Connection(),
          XCB_COPY_FROM_PARENT, ///< Depth
          mXcbWindow,
          Screen()->root, ///< Parent to root of screen
          0, ///< X
          0, ///< Y
          windowClientWidth, ///< Width
          windowClientHeight, ///< Height
          0, ///< Border
          XCB_WINDOW_CLASS_INPUT_OUTPUT, ///< Class
          Screen()->root_visual, ///> Visual
          mask, values.values );

      // Setup the window to receive window close events:
      // (see: http://tronche.com/gui/x/icccm/sec-4.html#s-4.1.2.7 )

      // We need to specify window properties using X11 Atoms which are interned
      // strings, so lets get the atoms we need (WM_PROTOCOLS, WM_DELETE_WINDOW):
      const xcb_intern_atom_cookie_t wm_protocols_cookie =
          xcb_intern_atom(Connection(), 1, sizeof("WM_PROTOCOLS")-1, "WM_PROTOCOLS");
      const xcb_intern_atom_cookie_t wm_delete_window_cookie =
          xcb_intern_atom(Connection(), 0, sizeof("WM_DELETE_WINDOW")-1, "WM_DELETE_WINDOW");
      xcb_generic_error_t* wm_protocols_error = 0;
      xcb_intern_atom_reply_t* wm_protocols_reply = xcb_intern_atom_reply(Connection(), wm_protocols_cookie, &wm_protocols_error);
      ScopedFree free_wm_protocols_reply(wm_protocols_reply);
      xcb_generic_error_t* wm_delete_window_error = 0;
      xcb_intern_atom_reply_t* wm_delete_window_reply = xcb_intern_atom_reply(Connection(), wm_delete_window_cookie, &wm_delete_window_error);
      ScopedFree free_wm_delete_window_reply(wm_delete_window_reply);
      mDeleteWindowEventAtom = wm_delete_window_reply->atom;

      // Finally append the WM_DELETE_WINDOW atom to the WM_PROTOCOLS property of the
      // window so client message events will be sent:
      xcb_change_property(
        Connection(),
        XCB_PROP_MODE_APPEND, /// XCB_PROP_MODE_REPLACE
        mXcbWindow,
        wm_protocols_reply->atom,
        XCB_ATOM_ATOM/*4*/, sizeof(mDeleteWindowEventAtom) * 8, 1,
        &mDeleteWindowEventAtom
      );

      // Set the title of the window:
      if(title && *title)
      {
      //xcb_void_cookie_t title_cookie =
      xcb_change_property(
        Connection(),
        XCB_PROP_MODE_REPLACE,
        mXcbWindow,
        XCB_ATOM_WM_NAME,
        XCB_ATOM_STRING,
        8,
        strlen(title),
        title);
      }

      // Show the window:
      xcb_map_window(Connection(), mXcbWindow);

      // Position the window centred in screen:
      const uint32_t winXY[] = { (windowClientWidth / 3) / 2, (windowClientHeight / 3) / 2 };
      xcb_configure_window(Connection(), mXcbWindow, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, winXY);

      // Sync:
      xcb_flush(Connection());
    }
  }
}

WindowPlatform::~WindowPlatform() {
  KRUST_LOG_INFO << "WindowPlatform destroyed.\n";
}

xcb_connection_t* WindowPlatform::Connection() const {
  return mApplication.mPlatformApplication.mXcbConnection;
}

xcb_screen_t* WindowPlatform::Screen() const {
  return mApplication.mPlatformApplication.mXcbScreen;
}

} /* namespace Krust::IO */

} /* namespace Krust */
