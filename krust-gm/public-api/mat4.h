#ifndef KRUST_GM_PUBLIC_API_MAT4_H_
#define KRUST_GM_PUBLIC_API_MAT4_H_

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
 * @file 4x4 mmatrix type, using experimental standard SIMD types when
 * available and a scalar fallback otherwise.
 * @note Scalar fallback will be allowed to languish until it is needed.
 *
 * @todo y-axis rotation matrix
 * @todo z-axis rotation matrix
 * @todo arbitrary-axis rotation matrix
 * @todo transpose matrix
 * @todo scaling matrix
 * @note Some of these can at least be started as wrappers around glm functions
 * that hide glm in a single compilation unit away from the rest of the system.
 * @todo look-at matrix
 * @todo to/from quaternions.
 * @todo perspective projection matrix
 * @todo orthographic projection matrix
 */
#include "mat4_fwd.h"
#include "krust-kernel/public-api/debug.h"
#include "krust/public-api/krust-assertions.h"
#include "vec4_inl.h"
#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
#include <experimental/simd>
#endif

namespace Krust {

// =============================================================================
// SIMD version
#if defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
/**
 * @brief 4x4 float matrix with a row-major physical layout and operator []
 * access. This allows loads of a matrix from memory to be four packed loads and
 * transformations of Vec4s to be four Vec4 by Vec4 dot products.
 *
 * Ideally, the Mat4 type only ever exists transiently in <= 4 SIMD registers
 * and is never stored to memory. We provide a Mat4InMemory type for that
 * purpose.
 * @note GLSL and the GLM library use column-major layout, so some adjustment
 * may be required from code using those.
 * @see Mat4InMemory for a type to be stored to memory.
 */
struct Mat4 {
  Vec4 rows[4];

  const Vec4 &operator[](const size_t i) const { return rows[i]; }

  Vec4 &operator[](const size_t i) { return rows[i]; }

  bool operator==(const Mat4 &other) const {
    return all_of(rows[0] == other.rows[0]) &
           all_of(rows[1] == other.rows[1]) &
           all_of(rows[2] == other.rows[2]) &
           all_of(rows[3] == other.rows[3]);
  }

  template <typename M>
  bool operator==(const M &other) const {
    return (rows[0][0] == other[0][0]) & (rows[0][1] == other[0][1]) &
           (rows[0][2] == other[0][2]) & (rows[0][3] == other[0][3]) &
           (rows[1][0] == other[1][0]) & (rows[1][1] == other[1][1]) &
           (rows[1][2] == other[1][2]) & (rows[1][3] == other[1][3]) &
           (rows[2][0] == other[2][0]) & (rows[2][1] == other[2][1]) &
           (rows[2][2] == other[2][2]) & (rows[2][3] == other[2][3]) &
           (rows[3][0] == other[3][0]) & (rows[3][1] == other[3][1]) &
           (rows[3][2] == other[3][2]) & (rows[3][3] == other[3][3]);
  }
};

// =============================================================================
// Scalar fall-back

#else // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)
#error The scalar fallback for matrix types is unfinished. If you have gcc, enable the cached variable KRUST_GM_BUILD_CONFIG_ENABLE_SIMD in cmake config step. Otherwise, ask and I will try to finish it.
#endif // defined(KRUST_GM_BUILD_CONFIG_ENABLE_SIMD)

inline Mat4 make_identity_mat4() {
  Mat4 mat{
    make_vec4(1.0f, 0.0f, 0.0f, 0.0f),
    make_vec4(0.0f, 1.0f, 0.0f, 0.0f),
    make_vec4(0.0f, 0.0f, 1.0f, 0.0f),
    make_vec4(0.0f, 0.0f, 0.0f, 1.0f)
  };
  return mat;
}

inline Mat4 load_mat4(const Mat4InMemory &mmem) {
  Mat4 mat{
    load(mmem.rows[0]),
    load(mmem.rows[1]),
    load(mmem.rows[2]),
    load(mmem.rows[3])
  };
  return mat;
}

inline Mat4 load_mat4(const float v[4][4]) {
  Mat4 mat{
    loadf4(v[0]),
    loadf4(v[1]),
    loadf4(v[2]),
    loadf4(v[3]),
  };
  return mat;
}

inline Mat4 load_mat4(const Vec4InMemory vmem[4]) {
  Mat4 mat{
    load(vmem[0]),
    load(vmem[1]),
    load(vmem[2]),
    load(vmem[3])
  };
  return mat;
}

inline void store(const Mat4 &mat, Mat4InMemory &matmem)
{
  store(mat.rows[0], matmem.rows[0]);
  store(mat.rows[1], matmem.rows[1]);
  store(mat.rows[2], matmem.rows[2]);
  store(mat.rows[3], matmem.rows[3]);
}

inline void store(const Mat4 &mat, Vec4InMemory matmem[4]) {
  store(mat.rows[0], matmem[0]);
  store(mat.rows[1], matmem[1]);
  store(mat.rows[2], matmem[2]);
  store(mat.rows[3], matmem[3]);
}

inline void store(const Mat4 &mat, float matmem[4][4]) {
  store(mat.rows[0], matmem[0]);
  store(mat.rows[1], matmem[1]);
  store(mat.rows[2], matmem[2]);
  store(mat.rows[3], matmem[3]);
}

inline Vec4 transform(const Mat4 &m, const Vec4 &v) {
  Vec4 result;
  result[0] = dot(m.rows[0], v);
  result[1] = dot(m.rows[1], v);
  result[2] = dot(m.rows[2], v);
  result[3] = dot(m.rows[3], v);
  return result;
}

/// @brief c = a * b, return c.
/// @todo Move to cpp file? (means considering whether this return by value
/// makes sense versus blatting the output into 4 aligned vec4s in memory)
inline Mat4 concatenate(const Mat4 &a, const Mat4 &b) {
  Mat4 c;
  // Gather the columns of b into vectors for SIMD dot products for the rows of
  // a which default to SIMD compatible vectors: (whether all this shenanigans
  // overwhelms the benefit of the SIMD-friendly dots would be worth checking)
  const Vec4 b_column0 = make_vec4(
    b.rows[0][0],
    b.rows[1][0],
    b.rows[2][0],
    b.rows[3][0]); // Pull first element out of each SIMD short vector.
  const Vec4 b_column1 = make_vec4(
    b.rows[0][1],
    b.rows[1][1],
    b.rows[2][1],
    b.rows[3][1]); // Pull second element out of each SIMD short vector
  const Vec4 b_column2 = make_vec4(
    b.rows[0][2],
    b.rows[1][2],
    b.rows[2][2],
    b.rows[3][2]); // Pull third element out of each SIMD short vector
  const Vec4 b_column3 = make_vec4(
    b.rows[0][3],
    b.rows[1][3],
    b.rows[2][3],
    b.rows[3][3]); // Pull fourth element out of each SIMD short vector

  // 16 dot products of rows versus columns to fill the destination matrix:
  for (int i = 0; i < 4; ++i)
  {
    const Vec4 a_row = a.rows[i];

    c.rows[i][0] = dot(a_row, b_column0);
    c.rows[i][1] = dot(a_row, b_column1);
    c.rows[i][2] = dot(a_row, b_column2);
    c.rows[i][3] = dot(a_row, b_column3);
  }
  return c;
}

/// @brief Make a translation matrix.
/// @param x X translation.
/// @param y Y translation.
/// @param z Z translation.
/// @return A translation matrix.
inline Mat4 make_translation_mat4(const float x, const float y, const float z)
{
  Mat4 mat{
    make_vec4(1.0f, 0.0f, 0.0f, x),
    make_vec4(0.0f, 1.0f, 0.0f, y),
    make_vec4(0.0f, 0.0f, 1.0f, z),
    make_vec4(0.0f, 0.0f, 0.0f, 1.0f)};
  return mat;
}

/// @brief Append a translation to an existing matrix.
/// @param m The matrix to append the translation to.
/// @param x X translation.
/// @param y Y translation.
/// @param z Z translation.
inline void append_translation(Mat4 &m, const float x, const float y, const float z)
{
  m.rows[0][3] += x;
  m.rows[1][3] += y;
  m.rows[2][3] += z;
}

/// @brief Make a matrix for rotation around the x-axis.
/// @param angle_radians Angle of rotation in radians.
/// @return A rotation matrix.
inline Mat4 make_rotation_x_mat4(const float angle_radians) {
  const float c = cos(angle_radians);
  const float s = sin(angle_radians);
  Mat4 mat{
    make_vec4(1.0f, 0.0f, 0.0f, 0.0f),
    make_vec4(0.0f, c,   -s,    0.0f),
    make_vec4(0.0f, s,    c,    0.0f),
    make_vec4(0.0f, 0.0f, 0.0f, 1.0f)};
  return mat;
}

} // namespace Krust

#endif // KRUST_GM_PUBLIC_API_MAT4_H_