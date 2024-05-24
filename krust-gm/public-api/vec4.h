#ifndef KRUST_GM_PUBLIC_API_VEC4_H_
#define KRUST_GM_PUBLIC_API_VEC4_H_

// Copyright (c) 2021,2024 Andrew Helge Cox
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
 * @file 4-component vector type, using experimental standard SIMD types when
 * available and a scalar fallback otherwise.
 * @note Scalar fallback will be allowed to languish until it is needed.
 */
#include "vec4_scalar.h"
#include "vec4_fwd.h"

// =============================================================================
// SIMD version
#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

#include <experimental/simd>

namespace Krust
{
    using Vec4 = std::experimental::simd<float, std::experimental::simd_abi::fixed_size<4>>;
}

// =============================================================================
// Scalar fall-back

#else // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
namespace Krust
{
    #error The scalar fallback for vector types is unfinished. If you have gcc, enable the cached variable KRUST_GM_BUILD_CONFIG_ENABLE_SIMD in cmake config step. Otherwise, ask and I will try to finish it.
    using Vec4 = Vec4Scalar;
}
#endif // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

#endif // KRUST_GM_PUBLIC_API_VEC4_H_