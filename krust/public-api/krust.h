#ifndef KRUST_H_INCLUDED_E26EF
#define KRUST_H_INCLUDED_E26EF

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
#include "krust/public-api/compiler.h"
#include "krust/public-api/conditional-value.h"
#include "krust/public-api/intrusive-pointer.h"
#include "krust/public-api/krust-assertions.h"
#include "krust/public-api/krust-errors.h"
#include "krust/public-api/logging.h"
#include "krust/public-api/ref-object.h"
#include "krust/public-api/thread-base.h"
#include "krust/public-api/vulkan_struct_init.h"
#include "krust/public-api/vulkan-logging.h"
#include "krust/public-api/vulkan-objects.h"
#include "krust/public-api/vulkan-utils.h"

// External includes:
#include <vulkan/vulkan.h>

/**
 * @file
 *
 * The main header file for Krust, pulling in all the non-optional pieces.
 * @note Several pieces of Krust can also be used alone such as the simple
 * helper functions in `krust/public-api/vulkan_struct_init.h`.
 */

namespace Krust
{

/**
 * @brief Initialization for Krust library to be called once on main thread
 * before first use of Krust.
 *
 * @oaram[in] errorPolicy A specialization of ErrorPolicy to control how errors
 * are reported.
 * @oaram[in] allocator If you need to control CPU/host memory allocation of
 * Vulkan data structures, you can pass in an allocator here. This must be
 * thread-safe if you use Krust from multiple threads. For instance if you
 * generate one-shot command buffers on worker threads, sugmit them on a submit
 * thread and let Krust's automatic lifetime management clean them up when they
 * complete on the GPU, the cleanup will happen on the 
 * @returns True if Krust was initialised, else false.
 * @note Duplicates Krust namespace in function name as Init is a common name
 * and tools like Doxygen can fail where namespaces are used to distinguish
 * symbols.
 */
bool InitKrust(ErrorPolicy* errorPolicy = nullptr, VkAllocationCallbacks* allocator = nullptr);

/**
 * Retrieve the default error policy that was passed to InitKrust().
 * This will be used in the absence of a per-thread policy.
 */
ErrorPolicy* GetGlobalErrorPolicy();

/**
 * Retrieve the allocation callbacks set by Krust::InitKrust().  
 */
VkAllocationCallbacks* GetAllocationCallbacks();

}

#endif // #ifndef KRUST_H_INCLUDED_E26EF
