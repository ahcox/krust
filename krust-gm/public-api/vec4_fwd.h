#ifndef KRUST_GM_PUBLIC_API_VEC4_FWD_H_
#define KRUST_GM_PUBLIC_API_VEC4_FWD_H_

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
 * @file Forward declarations for short vector types so code that doesn't want
 * to pull in all that template malarkey from the standard SIMD types can still
 * work with them.
 * @note Forward declaring the template clases failed in August 2021 on gcc due
 * to some macro magic that is used to open the namespaces in the std headers
 * being incompatible with just writing them out here in plain text.
 * @note It is worth having a type for the in-memory representation of vec4s
 * so that buffers of them can be allocated and member variables declared without
 * pulling in the full SIMD templates everywhere.
 */


namespace Krust
{
class Vec4Scalar;

    /**
     * vec4_inl.h has helper funcs:
     * Vec4 load(&Vec4InMemory vec);
     * void store(Vec4, &Vec4InMemory vec);
     */
#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    struct alignas(16) Vec4InMemory {
#else
    struct Vec4InMemory {
#endif
        Vec4InMemory() = default;
        Vec4InMemory(const float x, const float y, const float z, const float w) : v{x, y, z, w} {}
        float v[4];
    };

constexpr bool operator == (const Vec4InMemory& l, const Vec4InMemory& r)
{
    if(l.v[0] != r.v[0]) return false;
    if(l.v[1] != r.v[1]) return false;
    if(l.v[2] != r.v[2]) return false;
    if(l.v[3] != r.v[3]) return false;
    return true;
}

}
#endif // KRUST_GM_PUBLIC_API_VEC4_FWD_H_