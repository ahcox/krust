#include "vulkan-logging.h"

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


namespace Krust
{

LogBuilder& operator << (LogBuilder& logBuilder, const VkExtent2D& vkExtent2D)
{
  logBuilder << "VkExtent2D(" << &vkExtent2D << "){"
  << " width = " << vkExtent2D.width << ", height = " << vkExtent2D.height
  << " }";
  return logBuilder;
}

LogBuilder& operator << (LogBuilder& logBuilder, const VkPresentModeKHR& mode)
{
  logBuilder << ((mode == VK_PRESENT_MODE_IMMEDIATE_KHR) ? "VK_PRESENT_MODE_IMMEDIATE_KHR" :
                 ((mode == VK_PRESENT_MODE_MAILBOX_KHR) ? "VK_PRESENT_MODE_MAILBOX_KHR" :
                  ((mode == VK_PRESENT_MODE_FIFO_KHR) ? "VK_PRESENT_MODE_FIFO_KHR" :
                   ((mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR) ? "VK_PRESENT_MODE_FIFO_RELAXED_KHR" :
                   "<<UNKNOWN_PRESENT_MODE>>"))));
  return logBuilder;
}
}
