// Copyright (c) 2024 Andrew Helge Cox
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
 */
#include "mat4.h"

namespace Krust {

bool operator==(const Mat4InMemory &l, const Mat4InMemory &r)
{
  if(l.rows[0] != r.rows[0]) return false;
  if(l.rows[1] != r.rows[1]) return false;
  if(l.rows[2] != r.rows[2]) return false;
  if(l.rows[3] != r.rows[3]) return false;
  return true;;
}

void make_identity_mat4(float matmem[4][4]) {
  Mat4 mat{make_identity_mat4()};
  store(mat, matmem);
}

void make_identity_mat4(Mat4InMemory &matmem) {
  Mat4 mat{make_identity_mat4()};
  store(mat, matmem);
}

void append_translation(Mat4InMemory &mm, const float x, const float y, const float z)
{
    mm.rows[0].v[3] += x;
    mm.rows[1].v[3] += y;
    mm.rows[2].v[3] += z;
}

} // namespace Krust