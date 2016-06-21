#ifndef KRUST_COMMON_PUBLIC_API_KRUST_ASSERTIONS_H_
#define KRUST_COMMON_PUBLIC_API_KRUST_ASSERTIONS_H_

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


// External includes:
#include <cassert>

/**
 * @brief Assertion of a compile-time expression.
 */
#define KRUST_COMPILE_ASSERT(exp, msg) static_assert((exp), msg);

/**
 * @brief Assert that struct is of certain size.
 *
 * If one of these fails, it suggests the definition of a struct has changed.
 * You need to look at all usages of that struct in your code and see if there
 * any fields you are leaving uninitialised.
 */
#define KRUST_ASSERT_STRUCT_SIZE(STRUCT_NAME, EXPECTED_SIZE) \
  KRUST_COMPILE_ASSERT(!KRUST_GCC_64BIT_X86_BUILD || sizeof(STRUCT_NAME) == (EXPECTED_SIZE), #STRUCT_NAME" size changed: recheck init code.");

/* -----------------------------------------------------------------------------
 * KRUST_ASSERT1() -
 * KRUST_ASSERT5()
 *//**
 * @name MultilevelAssertions
 * Assertions at increasing levels of granularity. The higher the number, the
 * more often the assertion is expected to be called.
 * For example, KRUST_ASSERT1 should be used for the most-rarely called code
 * such as one-time system startup functions, and KRUST_ASSERT5 could be used
 * to bounds check an address inside an image processing per-pixel loop.
 * (1) would be used for `ASSERT(Program_init())` and (5) for
 * `ASSERT(shift_width < 32u)` for example.
 * Within Krust assertions are intended to represent extra checks such as pre
 * and post conditions for debugging and development.
 * They should not be used for error checking, parameter validation, etc.
 * The system and any app built from it should function identically with them
 * compiled-in or compiled out provided the tests they make do not fail.
 */
// > Define these according to a config macro (LEVEL).
///@{
#ifndef KRUST_ASSERT_LEVEL
#   error "Define the macro KRUST_ASSERT_LEVEL, giving it a value from 1 to 5, or zero to turn off all assertions."
#   define KRUST_ASSERT_LEVEL 5
#endif
#if(KRUST_ASSERT_LEVEL >= 1)
#define KRUST_ASSERT1(exp, msg) assert((exp) && (msg));
#else
#define KRUST_ASSERT1(exp, msg)
#endif
#if(KRUST_ASSERT_LEVEL >= 2)
#define KRUST_ASSERT2(exp, msg) assert((exp) && (msg));
#else
#define KRUST_ASSERT2(exp, msg)
#endif
#if(KRUST_ASSERT_LEVEL >= 3)
#define KRUST_ASSERT3(exp, msg) assert((exp) && (msg));
#else
#define KRUST_ASSERT3(exp, msg)
#endif
#if(KRUST_ASSERT_LEVEL >= 4)
#define KRUST_ASSERT4(exp, msg) assert((exp) && (msg));
#else
#define KRUST_ASSERT4(exp, msg)
#endif
#if(KRUST_ASSERT_LEVEL >= 5)
#define KRUST_ASSERT5(exp, msg) assert((exp) && (msg));
#else
#define KRUST_ASSERT5(exp, msg)
#endif
///@}

/**
 * @brief Take appropriate action for an event that cannot happen.
 */
#define KRUST_UNRECOVERABLE_ERROR(MSG, FILE, LINE) \
  KRUST_LOG_ERROR << (MSG) << " " << (FILE) << ":" << (LINE) << "." << endlog; \
  KRUST_ASSERT1(true == false, (MSG));

#endif /* KRUST_COMMON_PUBLIC_API_KRUST_ASSERTIONS_H_ */
