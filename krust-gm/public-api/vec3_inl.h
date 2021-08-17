#ifndef KRUST_GM_PUBLIC_API_VEC3_INL_H_
#define KRUST_GM_PUBLIC_API_VEC3_INL_H_

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
 * @file Inline functions related to the Vec3 type.
 */
#include "vec3_fwd.h"
#include "vec3.h"

namespace Krust
{

    inline Vec3 cross(const Vec3 v, const Vec3 w)
    {
        Vec3 result;
        result[0] = v[1] * w[2] - v[2] * w[1];
        result[1] = v[2] * w[0] - v[0] * w[2];
        result[2] = v[0] * w[1] - v[1] * w[0];
        return result;
    }

#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    inline Vec3 load(const Vec3InMemory& vmem)
    {
        Vec3 vec;
        //vec.copy_from(&vmem.v[0], std::experimental::vector_aligned); // compiles
        //vec.copy_from(&vmem.v[0], std::experimental::overaligned<16>); // compiles
        vec.copy_from(&vmem.v[0], std::experimental::overaligned<alignof(Vec3InMemory)>); // compiles. Hope it is same as vector_aligned when we align the in-memory type to 16.
        return vec;
    }

    inline Vec3 load(const float vmem[3])
    {
        Vec3 vec;
        vec.copy_from(vmem, std::experimental::element_aligned);
        return vec;
    }

    inline void store(const Vec3 vec, Vec3InMemory& vmem)
    {
        vec.copy_to(&vmem.v[0], std::experimental::overaligned<alignof(Vec3InMemory)>);
    }

    inline void store(const Vec3 vec, float vmem[3])
    {
        vec.copy_to(vmem, std::experimental::element_aligned);
    }

    /// Horizontal add components in vector.
    inline float hadd(const Vec3 vec)
    {
        return reduce(vec);
    }

    inline float dot(const Vec3 v1, const Vec3 v2)
    {
        return reduce(v1 * v2);
    }

    inline Vec3 make_vec3(const float v[3])
    {
        Vec3 vec(v, std::experimental::element_aligned);
        return vec;
    }

    inline Vec3 make_vec3(const float x, const float y, const float z)
    {
        Vec3 vec;
        vec[0] = x;
        vec[1] = y;
        vec[2] = z;
        return vec;
    }

    inline Vec3 make_vec3(const Vec3 vec)
    {
        return vec;
    }

#else // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
inline Vec3 load(Vec3InMemory& vmem)
    {
        Vec3 vec {vmem.v[0], vmem.v[1], vmem.v[2]};
        return vec;
    }

    inline void store(const Vec3 vec, Vec3InMemory& vmem)
    {
        vmem.v[0] = vec[0];
        vmem.v[1] = vec[1];
        vmem.v[2] = vec[2];
    }

    inline float dot(const Vec3 v1, const Vec3 v2)
    {
        return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
    }

#endif // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

}

#endif // KRUST_GM_PUBLIC_API_VEC3_INL_H_