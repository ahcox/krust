#ifndef KRUST_GM_PUBLIC_API_VEC4_INL_H_
#define KRUST_GM_PUBLIC_API_VEC4_INL_H_

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
 * @file Inline functions related to the Vec4 type.
 */
#include "vec4_fwd.h"
#include "vec4.h"

namespace Krust
{

#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    inline Vec4 load(const Vec4InMemory& vmem)
    {
        Vec4 vec;
        vec.copy_from(&vmem.v[0], std::experimental::overaligned<alignof(Vec4InMemory)>); // compiles. Hope it is same as vector_aligned when we align the in-memory type to 16.
        return vec;
    }

    inline Vec4 loadf4(const float vmem[4])
    {
        Vec4 vec;
        vec.copy_from(vmem, std::experimental::element_aligned);
        return vec;
    }

    inline void store(const Vec4 vec, Vec4InMemory& vmem)
    {
        vec.copy_to(&vmem.v[0], std::experimental::overaligned<alignof(Vec4InMemory)>);
    }

    inline void store(const Vec4 vec, float vmem[4])
    {
        vec.copy_to(vmem, std::experimental::element_aligned);
    }

    /// Horizontal add components in vector.
    inline float hadd(const Vec4 vec)
    {
        return reduce(vec);
    }

    inline float dot(const Vec4 v1, const Vec4 v2)
    {
        return reduce(v1 * v2);
    }

    inline Vec4 make_vec4(const float v[4])
    {
        Vec4 vec(v, std::experimental::element_aligned);
        return vec;
    }

    inline Vec4 make_vec4(const float x, const float y, const float z, const float w)
    {
        Vec4 vec;
        vec[0] = x;
        vec[1] = y;
        vec[2] = z;
        vec[3] = w;
        return vec;
    }

    inline Vec4 make_vec4(const Vec4 vec)
    {
        return vec;
    }

#else // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
inline Vec4 load(Vec4InMemory& vmem)
    {
        Vec4 vec {vmem.v[0], vmem.v[1], vmem.v[2]};
        return vec;
    }

    inline void store(const Vec4 vec, Vec4InMemory& vmem)
    {
        vmem.v[0] = vec[0];
        vmem.v[1] = vec[1];
        vmem.v[2] = vec[2];
    }

    inline float dot(const Vec4 v1, const Vec4 v2)
    {
        return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
    }

#endif // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

}

#endif // KRUST_GM_PUBLIC_API_VEC4_INL_H_