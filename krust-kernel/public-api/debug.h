#ifndef KRUST_KERNEL_PUBLIC_API_DEBUG_H_
#define KRUST_KERNEL_PUBLIC_API_DEBUG_H_

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

/**
 * @file Macros to enable additional code to be run in non-release build
 * configurations.
 */

#if defined(KRUST_DEBUG_CODE_ON)
    constexpr unsigned KRUST_DEBUG_LEVEL = 1u;
    #define KRUST_DEBUG_CODE(code) code
    #define KRUST_DEBUG_CODE_BLOCK(code) { code }
    #define KRUST_RELEASE_CODE(code)
#else
    constexpr unsigned KRUST_DEBUG_LEVEL = 0u;
    #define KRUST_DEBUG_CODE(code)
    #define KRUST_DEBUG_CODE_BLOCK(code)
    #define KRUST_RELEASE_CODE(code) code
#endif // KRUST_DEBUG_CODE_ON

#define KRUST_BEGIN_DEBUG_BLOCK if (KRUST_DEBUG_LEVEL){
#define KRUST_END_DEBUG_BLOCK }


#endif /* KRUST_KERNEL_PUBLIC_API_DEBUG_H_ */
