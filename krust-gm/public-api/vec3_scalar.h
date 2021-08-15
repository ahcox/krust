#ifndef KRUST_GM_PUBLIC_API_VEC3_SCALAR_H_
#define KRUST_GM_PUBLIC_API_VEC3_SCALAR_H_

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
 * @file A version of the Vec3 type that uses straightforward scalar floating
 * point as a fallback for when SIMD is not available or not performant,
 * and potentially when SIMD units are utilised by scheduling separate scalar
 * instances to each lane (GPU or ISPC-style).
 */
#include <cstddef>

namespace Krust
{

class Vec3Scalar {
    float v[3];
public:
    float operator [] (const size_t i) const {
        return v[i];
    }

    operator float* () {
        return v;
    }

    operator const float* () const {
        return v;
    }
};

}
#endif // KRUST_GM_PUBLIC_API_VEC3_SCALAR_H_