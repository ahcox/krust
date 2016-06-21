#ifndef KRUST_PUBLIC_API_COMPILER_H_
#define KRUST_PUBLIC_API_COMPILER_H_

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
#include <cstdint>

/**
 * @file
 * @brief Utilities with compile time effects.
 */

namespace Krust {

/// @brief True when building for 64 bit using GCC.
///
/// Allows asserting some things when building for this compiler using a priori
/// knowledge that would break for compilers with different type sizes, packing,
/// or alignment.
#if defined(__x86_64__) && defined(__GNUC__) && !defined(__INTEL_COMPILER) && !defined(__clang__)
#define KRUST_GCC_64BIT_X86_BUILD true
#else
#define KRUST_GCC_64BIT_X86_BUILD false
#endif

/**
 * @name CompileTimeSizeof Size of struct in compiler console output.
 * Usage:
 * @code{.cpp}
 *     struct BigStruct { char chars[MAX_INT]; bool b; double d; }
 *     KRUST_SHOW_SIZEOF_STRUCT_AS_WARNING(BigStruct);
 * @endcode{.cpp}
 * Find the output buried in the compiler output. For example, on GCC 4.9 as
 * something like: "In instantiation of
 * ‘Krust::DeliberateOverflow<number>::operator char() [with int number = 32]",
 * where the size is 32 in this case.
 */
///@{
template<unsigned int n>
struct TemplateParamToEnum {
  enum { value = n };
};

template<int number>
struct DeliberateOverflow{ operator char() { return number + 256; } };

#define KRUST_SHOW_UINT_AS_WARNING(constant) char(Krust::DeliberateOverflow<constant>())
#define KRUST_SHOW_SIZEOF_STRUCT_AS_WARNING(StructType) KRUST_SHOW_UINT_AS_WARNING(Krust::TemplateParamToEnum<sizeof(StructType)>::value)
///@}

/// Suppress unused parameter warning.
template<typename T>
void suppress_unused(const T&) {}

/// Suppress unused parameter warning for two parameters.
template<typename T, typename S>
void suppress_unused(const T& t, const S& s)
{
  suppress_unused(t);
  suppress_unused(s);
}

/// Suppress unused parameter warning for three parameters.
template<typename T, typename S, typename R>
void suppress_unused(const T& t, const S& s, const R& r)
{
  suppress_unused(t, s);
  suppress_unused(r);
}

template<typename T, typename S, typename R, typename Q>
void suppress_unused(const T& t, const S& s, const R& r, const Q q )
{
  suppress_unused(t, s);
  suppress_unused(r, q);
}

template<typename T, typename S, typename R, typename Q, typename P>
void suppress_unused(const T& t, const S& s, const R& r, const Q& q, const P& p)
{
  suppress_unused(t, s, r, q);
  suppress_unused(p);
}

} /* namespace Krust */

#endif /* KRUST_PUBLIC_API_COMPILER_H_ */
