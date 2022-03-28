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
#include "krust/public-api/scoped-free.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/krust.h"
#include <xcb/xcb.h>
#include <limits>

namespace Krust {
namespace IO {

/** @return Textual representation of XCB event code. */
const char* XcbEventCodeToString(unsigned eventCode);

} /* namespace IO */
} /* namespace Krust */

Krust::IO::ApplicationPlatform::ApplicationPlatform(ApplicationInterface& callbacks) :
    mCallbacks(callbacks),
    mXcbConnection(0),
    mXcbScreen(0),
    mWindow(0)
{

}

bool Krust::IO::ApplicationPlatform::Init()
{
  // Bring up the connection to the X-Server:
  int screen = 0;
  mXcbConnection = xcb_connect(0, &screen);
  KRUST_LOG_INFO << "xcb_connect -> " << mXcbConnection << "." << endlog;

  // Get the first screen:
  // If we have a multi-monitor setup, this is likely a virtual screen encompassing
  // both of them. See xcb_randr_[...] for querying individual displays:
  // https://stackoverflow.com/questions/36966900/xcb-get-all-monitors-ands-their-x-y-coordinates
  const xcb_setup_t* setup  = xcb_get_setup(mXcbConnection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  mXcbScreen = iter.data;
  KRUST_LOG_INFO << "mXcbScreen == " << mXcbScreen << "." << endlog;

  KRUST_LOG_INFO << "Screen width = " << mXcbScreen->width_in_pixels << " pixels, " << mXcbScreen->width_in_millimeters << " millimetres." << endlog;
  KRUST_LOG_INFO << "Screen height = " << mXcbScreen->height_in_pixels << " pixels, " << mXcbScreen->height_in_millimeters << " millimetres." << endlog;

  return (mXcbConnection && mXcbScreen);
}

VkSurfaceKHR Krust::IO::ApplicationPlatform::InitSurface(VkInstance instance)
{
  KRUST_ASSERT1(mXcbConnection, "XCB connection should alread be up.");
  KRUST_ASSERT1(mXcbScreen && mXcbScreen->root, "XCB Window should already be up.");

  VkXcbSurfaceCreateInfoKHR createInfo = {
    .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0,
    .connection = mXcbConnection,
    .window = mWindow->GetPlatformWindow().mXcbWindow
  };
  VkSurfaceKHR surface = nullptr;
  const VkResult result = vkCreateXcbSurfaceKHR(instance, &createInfo, KRUST_DEFAULT_ALLOCATION_CALLBACKS, &surface);
  if(result != VK_SUCCESS)
  {
    KRUST_LOG_ERROR << "Failed to create Vk surface for window. Result: " << ResultToString(result) << "." << endlog;
    surface = nullptr;
  }
  return surface;
}

void Krust::IO::ApplicationPlatform::ListenToScancodes(uint8_t* keycodes, const size_t numKeys)
{
  for(size_t i = 0; i < numKeys; ++i)
  {
    mRegisteredKeys.set(keycodes[i]);
  }

}

void Krust::IO::ApplicationPlatform::DeInit() {
  if(mXcbConnection)
  {
    xcb_disconnect(mXcbConnection);
  }
}

void Krust::IO::ApplicationPlatform::PreRun() {
  xcb_flush(mXcbConnection);
}

void Krust::IO::ApplicationPlatform::WindowCreated(Window& window) {
  KRUST_ASSERT1(mWindow == 0, "Window created previously.");

  mWindow = &window;
}

void Krust::IO::ApplicationPlatform::WindowClosing(Window& window) {
  KRUST_ASSERT1(mWindow == &window, "We only support one window at the moment.");
  mWindow = 0;
}

bool Krust::IO::ApplicationPlatform::PeekAndDispatchEvent()
{
  xcb_generic_event_t *event;
  event = xcb_poll_for_event(mXcbConnection);
  if(event){
    ScopedFree event_deleter(event);
    ProcessEvent(event);
    return true;
  }
  return false;
}

void Krust::IO::ApplicationPlatform::WaitForAndDispatchEvent()
{
  xcb_generic_event_t *event;
  event = xcb_wait_for_event(mXcbConnection);
  ScopedFree event_deleter(event);
  ProcessEvent(event);

}

const char* Krust::IO::ApplicationPlatform::GetPlatformSurfaceExtensionName() {
   return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
}

void Krust::IO::ApplicationPlatform::ProcessEvent(const xcb_generic_event_t *event)
{
  if (event) {
    unsigned eventCode = event->response_type & 0x7f;
    switch (eventCode)
    {
      case XCB_EXPOSE: {
        xcb_expose_event_t *expose = (xcb_expose_event_t *) event;
        KRUST_LOG_INFO << "Expose event: window = " << expose->window << ", x,y = " << expose->x << "," << expose->y <<
        "." << endlog;
        if (mWindow && mWindow->GetPlatformWindow().mXcbWindow == expose->window) {
          mCallbacks.OnRedraw();
        }
        break;
      }
      case XCB_FOCUS_IN: {
        KRUST_LOG_INFO << "Gaining input focus so turning off key repeats." << endlog;
        uint32_t mask = XCB_KB_AUTO_REPEAT_MODE;
        uint32_t values[] = {XCB_AUTO_REPEAT_MODE_OFF, 0};
        xcb_void_cookie_t off_cookie = xcb_change_keyboard_control_checked(mXcbConnection, mask, values);
        xcb_generic_error_t* off_error = xcb_request_check(mXcbConnection, off_cookie);
        if(off_error) {
            KRUST_LOG_ERROR << "off_error: " << off_error->error_code << endlog;
        }
        break;
      }
      case XCB_FOCUS_OUT: {
        KRUST_LOG_INFO << "Losing input focus event so turning on key repeats." << endlog;
        uint32_t mask = XCB_KB_AUTO_REPEAT_MODE;
        uint32_t values[] = {XCB_AUTO_REPEAT_MODE_ON, 0};
        xcb_void_cookie_t on_cookie = xcb_change_keyboard_control_checked(mXcbConnection, mask, values);
        xcb_generic_error_t* on_error = xcb_request_check(mXcbConnection, on_cookie);
        if(on_error){
          KRUST_LOG_ERROR << "\tError: " << on_error->error_code << endlog;
        }
        // Send key-ups for all registered keys.
        /// @todo Consider passing the focus message to the app as a new callback instead.
        /// These keys might not be down currently so we are relying on the app not to
        /// care about that. Alternatively, track what keys are down in
        /// ApplicationPlatform, duplicating that work in the app, and only send key-ups
        /// for keys that are currently down.
        for(unsigned scancode = 0, end = mRegisteredKeys.size(); scancode < end; ++scancode)
        {
          if(mRegisteredKeys[scancode]){
              mCallbacks.OnKey(KeyUp, scancode);
          }
        }
        break;
      }
      case XCB_KEY_RELEASE: {
        const auto keyEvent = reinterpret_cast<const xcb_key_release_event_t *>(event);
        KRUST_COMPILE_ASSERT(sizeof(keyEvent->detail) == 1u, "Need to adjust mRegisteredKeys");
        const uint8_t scancode = keyEvent->detail;
        KRUST_LOG_INFO << "Key released in window. Code: " << int(scancode) << "." << endlog;
        if(mRegisteredKeys[scancode]){
            mCallbacks.OnKey(KeyUp, scancode);
        }
        break;
      }
      case XCB_KEY_PRESS: {
        const auto keyEvent = reinterpret_cast<const xcb_key_press_event_t *>(event);
        const uint8_t scancode = keyEvent->detail;
        KRUST_LOG_INFO << "Key pressed in window. Code: " << int(scancode) << "." << endlog;
        if(mRegisteredKeys[scancode]){
            mCallbacks.OnKey(KeyDown, scancode);
        }
        break;
      }
      case XCB_DESTROY_NOTIFY: {
        KRUST_LOG_INFO << "XCB_DESTROY_NOTIFY ignored." << endlog;
        break;
      }
      case XCB_BUTTON_PRESS: {
        const auto *buttonPresss = reinterpret_cast<const xcb_button_press_event_t *>(event);
        KRUST_LOG_INFO << "XCB_BUTTON_PRESS, detail: " << int(buttonPresss->detail) << endlog;
        break;
      }
      case XCB_BUTTON_RELEASE: {
        const auto buttonRelease = reinterpret_cast<const xcb_button_release_event_t *>(event);
        KRUST_LOG_INFO << "Button release: " << int(buttonRelease->detail) << endlog;
        break;
      }
      case XCB_MOTION_NOTIFY: {
        const auto motion = reinterpret_cast<const xcb_motion_notify_event_t *>(event);
        //KRUST_LOG_INFO << "Mouse moved in window ." << endlog;
        //KRUST_LOG_INFO << "\t :" << motion->sequence << endlog;
        //KRUST_LOG_INFO << "\t time:\t" << motion->time << endlog;
        // window:KRUST_LOG_INFO << "\t :" << motion->root << endlog;
        // window: KRUST_LOG_INFO << "\t event:\t" << motion->event << endlog;
        // window:KRUST_LOG_INFO << "\t child:\t" << motion->child << endlog;
        // Screen coords (in multimon it is the whole virtual screen encompassing them all) KRUST_LOG_INFO << "\t (root_x, root_y):\t(" << motion->root_x << ", " << motion->root_y << ')' << endlog;
        //KRUST_LOG_INFO << "\t (event_x, event_y):\t(" << motion->event_x << ", " << motion->event_y << ')' << endlog;
        //KRUST_LOG_INFO << "\t Button and special key state:\t" << motion->state << endlog;
        ///KRUST_LOG_INFO << "\t same screen\t:" << unsigned(motion->same_screen) << endlog;
        mCallbacks.OnMouseMove(motion->time, motion->event_x, motion->event_y, motion->state);
        break;
      }
      case XCB_ENTER_NOTIFY: {
        //xcb_enter_notify_event_t *enter = reinterpret_cast<xcb_enter_notify_event_t *>(event);
        KRUST_LOG_INFO << "Mouse entered window ." << endlog;
        break;
      }
      case XCB_LEAVE_NOTIFY: {
        //xcb_leave_notify_event_t *leave = reinterpret_cast<xcb_leave_notify_event_t *>(event);
        KRUST_LOG_INFO << "Mouse left window ." << endlog;
        break;
      }
      case XCB_CONFIGURE_NOTIFY: {
        const auto* notifyEvent = reinterpret_cast<const xcb_configure_notify_event_t*>(event);
        KRUST_LOG_INFO << "Window configuration changed (x = " << notifyEvent->x << ", y = " << notifyEvent->y << ", width = " << notifyEvent->width << ", height = " << notifyEvent->height <<")." << endlog;
        auto& platformWindow = mWindow->GetPlatformWindow();
        if (platformWindow.mXcbWindow == notifyEvent->window) {
            if(notifyEvent->width != platformWindow.GetWidth() || notifyEvent->height != platformWindow.GetHeight())
            {
              platformWindow.SetWidth(notifyEvent->width);
              platformWindow.SetHeight(notifyEvent->height);
              mCallbacks.DispatchResize(notifyEvent->width, notifyEvent->height);
            }
        }
        break;
      }

      case XCB_CLIENT_MESSAGE: {
        KRUST_LOG_DEBUG << "XCB_CLIENT_MESSAGE recieved. Attempting a clean shutdown." << endlog;
        // Reset the keyboard repeat mode for other apps before we die:
        /// @todo Reset the keyboard in an atexit handler (set_terminate(), at_quick_exit()) so it always runs.
        uint32_t mask = XCB_KB_AUTO_REPEAT_MODE;
        uint32_t values[] = {XCB_AUTO_REPEAT_MODE_ON, 0};
        xcb_void_cookie_t on_cookie = xcb_change_keyboard_control_checked(mXcbConnection, mask, values);
        xcb_generic_error_t* on_error = xcb_request_check(mXcbConnection, on_cookie);
        if(on_error){
          KRUST_LOG_INFO << "on_error: " << on_error->error_code << endlog;
        }

        const auto * clientEvent = reinterpret_cast<const xcb_client_message_event_t*>(event);
        if(mWindow)
        {
          if(mWindow->GetPlatformWindow().mDeleteWindowEventAtom == clientEvent->data.data32[0])
          {
            mCallbacks.OnClose();
          }
        }
        break;
      }

      default: {
        KRUST_LOG_DEBUG << "Non-handled event received. Response type(" << eventCode << "): " << XcbEventCodeToString(eventCode) << ", Sequence: " <<
        event->sequence << ", Full sequence: " << event->full_sequence << endlog;
        // We'll get XCB_MAPPING_NOTIFY ("keyboard mapping changed") if two keyboards are plugged in when switching between them.
        break;
      }
    }
  }
}

const char* Krust::IO::XcbEventCodeToString(unsigned eventCode)
{
  const char* text = " ";
  switch(eventCode)
  {
    case XCB_KEY_PRESS: { text = "XCB_KEY_PRESS"; break; }
    case XCB_KEY_RELEASE: { text = "XCB_KEY_RELEASE"; break; }
    case XCB_BUTTON_PRESS: { text = "XCB_BUTTON_PRESS"; break; }
    case XCB_BUTTON_RELEASE: { text = "XCB_BUTTON_RELEASE"; break; }
    case XCB_MOTION_NOTIFY: { text = "XCB_MOTION_NOTIFY"; break; }
    case XCB_ENTER_NOTIFY: { text = "XCB_ENTER_NOTIFY"; break; }
    case XCB_LEAVE_NOTIFY: { text = "XCB_LEAVE_NOTIFY"; break; }
    case XCB_FOCUS_IN: { text = "XCB_FOCUS_IN"; break; }
    case XCB_FOCUS_OUT: { text = "XCB_FOCUS_OUT"; break; }
    case XCB_KEYMAP_NOTIFY: { text = "XCB_KEYMAP_NOTIFY"; break; }
    case XCB_EXPOSE: { text = "XCB_EXPOSE"; break; }
    case XCB_GRAPHICS_EXPOSURE: { text = "XCB_GRAPHICS_EXPOSURE"; break; }
    case XCB_NO_EXPOSURE: { text = "XCB_NO_EXPOSURE"; break; }
    case XCB_VISIBILITY_NOTIFY: { text = "XCB_VISIBILITY_NOTIFY"; break; }
    case XCB_CREATE_NOTIFY: { text = "XCB_CREATE_NOTIFY"; break; }
    case XCB_DESTROY_NOTIFY: { text = "XCB_DESTROY_NOTIFY"; break; }
    case XCB_UNMAP_NOTIFY: { text = "XCB_UNMAP_NOTIFY"; break; }
    case XCB_MAP_NOTIFY: { text = "XCB_MAP_NOTIFY"; break; }
    case XCB_MAP_REQUEST: { text = "XCB_MAP_REQUEST"; break; }
    case XCB_REPARENT_NOTIFY: { text = "XCB_REPARENT_NOTIFY"; break; }
    case XCB_CONFIGURE_NOTIFY: { text = "XCB_CONFIGURE_NOTIFY"; break; }
    case XCB_CONFIGURE_REQUEST: { text = "XCB_CONFIGURE_REQUEST"; break; }
    case XCB_GRAVITY_NOTIFY: { text = "XCB_GRAVITY_NOTIFY"; break; }
    case XCB_RESIZE_REQUEST: { text = "XCB_RESIZE_REQUEST"; break; }
    case XCB_CIRCULATE_NOTIFY: { text = "XCB_CIRCULATE_NOTIFY"; break; }
    case XCB_CIRCULATE_REQUEST: { text = "XCB_CIRCULATE_REQUEST"; break; }
    case XCB_PROPERTY_NOTIFY: { text = "XCB_PROPERTY_NOTIFY"; break; }
    case XCB_SELECTION_CLEAR: { text = "XCB_SELECTION_CLEAR"; break; }
    case XCB_SELECTION_REQUEST: { text = "XCB_SELECTION_REQUEST"; break; }
    case XCB_SELECTION_NOTIFY: { text = "XCB_SELECTION_NOTIFY"; break; }
    case XCB_COLORMAP_NOTIFY: { text = "XCB_COLORMAP_NOTIFY"; break; }
    case XCB_CLIENT_MESSAGE: { text = "XCB_CLIENT_MESSAGE"; break; }
    case XCB_MAPPING_NOTIFY: { text = "XCB_MAPPING_NOTIFY"; break; }
    case XCB_GE_GENERIC: { text = "XCB_GE_GENERIC"; break; }
    default: { text = "<<<UNKNOWN EVENTCODE>>>"; break; }
  };
  return text;
}

float Krust::IO::durationBetween(const InputTimestamp start, const InputTimestamp end)
{
  if(end >= start){
      [[likely]]
      return float(end - start) / 1000.0f;
  } else {
      // We can roll over once, but not twice:
      // This should be fine up to 49 days so don't suspend your laptop for 50 days.
      [[unlikely]]
      return float(end) / 1000.0f + (std::numeric_limits<InputTimestamp>::max() - start) / 1000.0f;
  }
}