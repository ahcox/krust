#ifndef KRUST_GM_PUBLIC_API_VEC3_FWD_H_
#define KRUST_GM_PUBLIC_API_VEC3_FWD_H_

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
 * @file Forward declarations for short vector types so code that doesn't want
 * to pull in all that template malarkey from the standard SIMD types can still
 * work with them.
 * @note Forward declaring the template clases failed in August 2021 on gcc due
 * to some macro magic that is used to open the namespaces in the std headers
 * being incompatible with just writing them out here in plain text.
 * @note It is worth having a type for the in-memory representation of vec3s
 * so that buffers of them can be allocated and member variables declared without
 * pulling in the full SIMD templates everywhere.
 * @note Are there alignment reqirements for SIMD loads on anticipated platforms
 * that make it worth algning members at the cost of some padding?
 * @todo Delete this if I can't come up with a reasonable use for it.
 */


namespace Krust
{
class Vec3Scalar;

#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
    /**
     * vec3_inl.h has helper funcs:
     * Vec3 load(&Vec3InMemory vec);
     * void store(Vec3, &Vec3InMemory vec);
     */
    struct alignas(16) Vec3InMemory {
        float v[3];
    };
#else
    struct Vec3InMemory {
        float v[3];
    };
#endif








// Examples of how these can be useful:

/**
 * Dot products of pairs of vectors, done with templated SIMD internally, but interface deals with concrete types: structs of floats in memory.
 **/
void dot_all(const Vec3InMemory* begin, const Vec3InMemory* end, const Vec3InMemory* begin2, float* results);

// Usage:

namespace Foo {
/** Class using vectors that we can include the header for without including the SIMD templates and whatever they include.*/
class Bah {
    Vec3InMemory fwd;
    Vec3InMemory right;
    Vec3InMemory up;
    /// Defined elsewhere in a place that includes the SIMD template headers.
    void regenerate_basis_vecs(float euler_x, float euler_y, float euler_z);
};
}

}
#endif // KRUST_GM_PUBLIC_API_VEC3_FWD_H_