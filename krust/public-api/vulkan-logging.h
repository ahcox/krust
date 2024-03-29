#ifndef KRUST_VULKAN_LOGGING_H_H
#define KRUST_VULKAN_LOGGING_H_H

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

// Internal Headers:
#include "logging.h"
#include "vulkan-utils.h"


// External Headers:
#include <krust/public-api/vulkan_types_and_macros.h>

/**
 * @file
 *
 * Krust logging helpers for Vulkan types.
 */

namespace Krust

{

/**
 * @name LoggingOperators Krust log output operators for Vulkan types.
 */
///@{
inline LogBuilder& operator << (LogBuilder& logBuilder, const VkResult& result) { logBuilder << ResultToString(result); return logBuilder; }
LogBuilder& operator << (LogBuilder& logBuilder, const VkExtent2D& vkExtent2D);
LogBuilder& operator << (LogBuilder& logBuilder, const VkPresentModeKHR& mode);
///@]

}

#endif //KRUST_VULKAN_LOGGING_H_H
