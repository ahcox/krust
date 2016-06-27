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

// Internal includes
#include "krust/public-api/krust.h"
#include "krust/internal/krust-internal.h"

namespace Krust
{

/**
* @brief The error policy used if the user doesn't set one.
* Defaults to throwing KrustException on errors.
*/
ExceptionsErrorPolicy sDefaultErrorPolicy;

/**
* @brief The global error policy to use if the user doesn't set a
* thread-specific one.
*
* Set in call to Krust::InitKrust
*/
ErrorPolicy* sErrorPolicy = &sDefaultErrorPolicy;


bool InitKrust(ErrorPolicy * errorPolicy, VkAllocationCallbacks * allocator)
{
  sErrorPolicy = errorPolicy ? errorPolicy : &sDefaultErrorPolicy;
  Internal::sAllocator = allocator ? allocator : KRUST_DEFAULT_ALLOCATION_CALLBACKS;
  return false;
}

ErrorPolicy* GetGlobalErrorPolicy()
{
  KRUST_ASSERT1(sErrorPolicy != nullptr, "Null global error policy. Did you call InitKrust()?");
  if (!sErrorPolicy)
  {
    KRUST_LOG_ERROR << "Null global error policy. Did you call InitKrust()?" << endlog;
  }
  return sErrorPolicy;
}

}
