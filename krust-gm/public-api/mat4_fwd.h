#ifndef KRUST_GM_PUBLIC_API_MAT4_FWD_H_
#define KRUST_GM_PUBLIC_API_MAT4_FWD_H_

// Copyright (c) 2021,2024 Andrew Helge Cox
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
/**
 * @file In-memory representation of a 4x4 matrix type.
 * Load it into an in-registers form using experimental standard SIMD types when
 * to perform computations.
 */
#include "vec4_fwd.h"

namespace Krust {

/**
 * @brief 4x4 matrix in memory layout.
 * This is useful for declaring member variables and forward declaring functions
 * in headers without pulling in the full SIMD templates everywhere.
 */
struct Mat4;
struct Mat4InMemory {
  Vec4InMemory rows[4];
};

bool operator == (const Mat4InMemory& l, const Mat4InMemory& r);
inline void store(const Mat4 &mat, Mat4InMemory &matmem);
inline void store(const Mat4 &mat, Vec4InMemory matmem[4]);
inline void store(const Mat4 &mat, float matmem[4][4]);
void make_identity_mat4(float matmem[4][4]);
void make_identity_mat4(Mat4InMemory &matmem);
/// @brief Append a translation to an existing matrix.
/// @param m The matrix to append the translation to.
/// @param x X translation.
/// @param y Y translation.
/// @param z Z translation.
/// @note This probably doesn't make sense to prototype since calling it non-inlined
/// implies having a Mat4 somewhere in memory to reference.
inline void append_translation(Mat4 &m, const float x, const float y, const float z);
void append_translation(Mat4InMemory &m, const float x, const float y, const float z);

} // namespace Krust

#endif // KRUST_GM_PUBLIC_API_MAT4_FWD_H_