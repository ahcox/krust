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
#include "krust-gm/public-api/mat4.h"
#include "krust-kernel/public-api/debug.h"
#include "catch.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/random.hpp"
#include <iostream>

/* ----------------------------------------------------------------------- *//**
 * @file Test of the Mat4 class.
 */

namespace {
namespace kr = Krust;

inline float randf()
{
    const float r = glm::linearRand(std::numeric_limits<float>::min(), std::numeric_limits<float>::max());
    return r;
}

void log_matrix(const kr::Mat4& m)
{
    std::cerr << "        " << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << "\n";
    std::cerr << "        " << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << "\n";
    std::cerr << "        " << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << "\n";
    std::cerr << "        " << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3] << "\n";
}

void log_matrix(const glm::mat4& m)
{
    std::cerr << "        " << m[0][0] << " " << m[1][0] << " " << m[2][0] << " " << m[3][0] << "\n";
    std::cerr << "        " << m[0][1] << " " << m[1][1] << " " << m[2][1] << " " << m[3][1] << "\n";
    std::cerr << "        " << m[0][2] << " " << m[1][2] << " " << m[2][2] << " " << m[3][2] << "\n";
    std::cerr << "        " << m[0][3] << " " << m[1][3] << " " << m[2][3] << " " << m[3][3] << "\n";
}

template <typename M, typename N>
void log_matrixes(const M& m1, const N& m2)
{
    std::cerr << "m1:\n";
    log_matrix(m1);
    std::cerr << "m2:\n";
    log_matrix(m2);
}

static void require_identity(const kr::Mat4& m1)
{
    REQUIRE(m1[0][0] == 1.0f);
    REQUIRE(m1[1][1] == 1.0f);
    REQUIRE(m1[2][2] == 1.0f);
    REQUIRE(m1[3][3] == 1.0f);
    REQUIRE(m1[0][1] == 0.0f);
    REQUIRE(m1[0][2] == 0.0f);
    REQUIRE(m1[0][3] == 0.0f);
    REQUIRE(m1[1][0] == 0.0f);
    REQUIRE(m1[1][2] == 0.0f);
    REQUIRE(m1[1][3] == 0.0f);
    REQUIRE(m1[2][0] == 0.0f);
    REQUIRE(m1[2][1] == 0.0f);
    REQUIRE(m1[2][3] == 0.0f);
    REQUIRE(m1[3][0] == 0.0f);
    REQUIRE(m1[3][1] == 0.0f);
    REQUIRE(m1[3][2] == 0.0f);
}

template <typename M, typename N>
void require_equal(const M& m1, const N& m2)
{
    REQUIRE(m1[0][0] == m2[0][0]);
    REQUIRE(m1[0][1] == m2[0][1]);
    REQUIRE(m1[0][2] == m2[0][2]);
    REQUIRE(m1[0][3] == m2[0][3]);
    REQUIRE(m1[1][0] == m2[1][0]);
    REQUIRE(m1[1][1] == m2[1][1]);
    REQUIRE(m1[1][2] == m2[1][2]);
    REQUIRE(m1[1][3] == m2[1][3]);
    REQUIRE(m1[2][0] == m2[2][0]);
    REQUIRE(m1[2][1] == m2[2][1]);
    REQUIRE(m1[2][2] == m2[2][2]);
    REQUIRE(m1[2][3] == m2[2][3]);
    REQUIRE(m1[3][0] == m2[3][0]);
    REQUIRE(m1[3][1] == m2[3][1]);
    REQUIRE(m1[3][2] == m2[3][2]);
    REQUIRE(m1[3][3] == m2[3][3]);
}

template <typename ROW_MAJOR, typename COLUMN_MAJOR>
void require_equal_row_x_column(const ROW_MAJOR& m1, const COLUMN_MAJOR& m2)
{
    REQUIRE(m1[0][0] == m2[0][0]);
    REQUIRE(m1[0][1] == m2[1][0]);
    REQUIRE(m1[0][2] == m2[2][0]);
    REQUIRE(m1[0][3] == m2[3][0]);
    REQUIRE(m1[1][0] == m2[0][1]);
    REQUIRE(m1[1][1] == m2[1][1]);
    REQUIRE(m1[1][2] == m2[2][1]);
    REQUIRE(m1[1][3] == m2[3][1]);
    REQUIRE(m1[2][0] == m2[0][2]);
    REQUIRE(m1[2][1] == m2[1][2]);
    REQUIRE(m1[2][2] == m2[2][2]);
    REQUIRE(m1[2][3] == m2[3][2]);
    REQUIRE(m1[3][0] == m2[0][3]);
    REQUIRE(m1[3][1] == m2[1][3]);
    REQUIRE(m1[3][2] == m2[2][3]);
    REQUIRE(m1[3][3] == m2[3][3]);
}

template <typename V, typename W>
void require_equal_vec4s(const V& v, const W& w)
{
    REQUIRE(v[0] == w[0]);
    REQUIRE(v[1] == w[1]);
    REQUIRE(v[2] == w[2]);
    REQUIRE(v[3] == w[3]);
}

/*
TEST_CASE("Mat4 Template", "[gm]")
{
  bool destroyed = false;
  REQUIRE(destroyed == false);
}
*/
TEST_CASE("Mat4 assign", "[gm]")
{
    kr::Mat4 m1 { kr::make_identity_mat4()};
    auto m2 = m1;

    REQUIRE(m1 == m2);

    float m3[4][4];
    kr::make_identity_mat4(m3);
    REQUIRE(m1 == m3);
}

TEST_CASE("Mat4 identity", "[gm]")
{
    const kr::Mat4 m1 = kr::make_identity_mat4();

    require_identity(m1);
}

TEST_CASE("Mat4 load_mat4", "[gm]")
{
    const float v[4][4] = {
        {0.0f,  1.0f,  2.0f,  3.0f},
        {4.0f,  5.0f,  6.0f,  7.0f},
        {8.0f,  9.0f,  10.0f, 11.0f},
        {12.0f, 13.0f, 14.0f, 15.0f}
    };
    const kr::Mat4 m1 = kr::load_mat4(v);

    REQUIRE(m1[0][0] == 0.0f);
    REQUIRE(m1[0][3] == 3.0f);
    REQUIRE(m1[3][0] == 12.0f);
    REQUIRE(m1[3][3] == 15.0f);
}

TEST_CASE("Mat4 load store load sequence", "[gm]")
{
    const float v[4][4] = {
        {0.0f,  1.0f,  2.0f,  3.0f},
        {4.0f,  5.0f,  6.0f,  7.0f},
        {8.0f,  9.0f,  10.0f, 11.0f},
        {12.0f, 13.0f, 14.0f, 15.0f}
    };
    const kr::Mat4 m1 = kr::load_mat4(v);
    require_equal(v, m1);
    float buffer[4][4];
    kr::store(m1, buffer);
    const kr::Mat4 m2 = kr::load_mat4(buffer);
    require_equal(m1, m2);
    kr::Vec4InMemory buffer2[4];
    kr::store(m2, buffer2);
    const kr::Mat4 m3 = kr::load_mat4(buffer2);
    require_equal(m1, m3);
}

TEST_CASE("Mat4 conctenate identities", "[gm]")
{
    const kr::Mat4 m1 = kr::make_identity_mat4();
    const kr::Mat4 m2 = kr::make_identity_mat4();
    const kr::Mat4 m3 = kr::concatenate(m1, m2);
    require_identity(m3);
    require_equal(m1, m3);
}

TEST_CASE("Mat4 concatenate fixed", "[gm]")
{
    const float matrix1[4][4] = {
        {0.0f,  1.0f,  2.0f,  3.0f},
        {4.0f,  5.0f,  6.0f,  7.0f},
        {8.0f,  9.0f,  10.0f, 11.0f},
        {12.0f, 13.0f, 14.0f, 15.0f}
    };
    const float matrix2[4][4] = {
        {10.0f,  11.0f,  12.0f,  13.0f},
        {-104.0f,  -105.0f,  -106.0f,  -107.0f},
        {1008.0f,  1009.0f,  10010.0f, 10011.0f},
        {-99912.0f, -99913.0f, -99914.0f, -99915.0f}
    };

    const kr::Mat4 m1 = kr::load_mat4(matrix1);
    const kr::Mat4 m2 = kr::load_mat4(matrix2);
    const kr::Mat4 m3 = kr::concatenate(m1, m2);

    // GLSL and thus GLM has column-major matrix constructors so we feed our row-major matrices in that order:
    glm::mat4 glm_m1 = glm::mat4(matrix1[0][0], matrix1[1][0], matrix1[2][0], matrix1[3][0],
                                 matrix1[0][1], matrix1[1][1], matrix1[2][1], matrix1[3][1],
                                 matrix1[0][2], matrix1[1][2], matrix1[2][2], matrix1[3][2],
                                 matrix1[0][3], matrix1[1][3], matrix1[2][3], matrix1[3][3]);

    glm::mat4 glm_m2 = glm::mat4(matrix2[0][0], matrix2[1][0], matrix2[2][0], matrix2[3][0],
                                 matrix2[0][1], matrix2[1][1], matrix2[2][1], matrix2[3][1],
                                 matrix2[0][2], matrix2[1][2], matrix2[2][2], matrix2[3][2],
                                 matrix2[0][3], matrix2[1][3], matrix2[2][3], matrix2[3][3]);

    glm::mat4 glm_m3 = glm_m1 * glm_m2;

    if(false)
    {
        std::cerr << "m3:\n";
        log_matrix(m3);
        std::cerr << "glm_m3:\n";
        log_matrix(glm_m3);
    }
    require_equal_row_x_column(m3, glm_m3);
}

TEST_CASE("Mat4 concatenate randoms", "[gm]")
{
    for(unsigned rep = 0; rep < 100; ++rep)
    {
        const float matrix1[4][4] = {
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()}
        };
        const float matrix2[4][4] = {
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()}
        };

        const kr::Mat4 m1 = kr::load_mat4(matrix1);
        const kr::Mat4 m2 = kr::load_mat4(matrix2);
        const kr::Mat4 m3 = kr::concatenate(m1, m2);

        // GLSL and thus GLM has column-major matrix constructors so we feed our row-major matrices in that order:
        glm::mat4 glm_m1 = glm::mat4(matrix1[0][0], matrix1[1][0], matrix1[2][0], matrix1[3][0],
                                    matrix1[0][1], matrix1[1][1], matrix1[2][1], matrix1[3][1],
                                    matrix1[0][2], matrix1[1][2], matrix1[2][2], matrix1[3][2],
                                    matrix1[0][3], matrix1[1][3], matrix1[2][3], matrix1[3][3]);

        glm::mat4 glm_m2 = glm::mat4(matrix2[0][0], matrix2[1][0], matrix2[2][0], matrix2[3][0],
                                    matrix2[0][1], matrix2[1][1], matrix2[2][1], matrix2[3][1],
                                    matrix2[0][2], matrix2[1][2], matrix2[2][2], matrix2[3][2],
                                    matrix2[0][3], matrix2[1][3], matrix2[2][3], matrix2[3][3]);
        glm::mat4 glm_m3 = glm_m1 * glm_m2;
        KRUST_UNUSED_VAR(glm_m3);
        KRUST_UNUSED_VAR(m3);
        require_equal_row_x_column(m3, glm_m3);
    }
}

TEST_CASE("Mat4 Vec4 transform identities", "[gm]")
{
    const kr::Mat4 m1 = kr::make_identity_mat4();

    for(unsigned i = 0; i < 100; ++i)
    {
        const kr::Vec4 randvec = kr::make_vec4(randf(), randf(), randf(), randf());
        const kr::Vec4 randvec_prime = kr::transform(m1, randvec);
        require_equal_vec4s(randvec, randvec_prime);
    }
}

TEST_CASE("Mat4 Vec4 transform 90s", "[gm]")
{
    // Rotate 90 degrees about the z axis.
    const float matrix1[4][4] = {
        {0.0f,  1.0f,  0.0f,  0.0f},
        {-1.0f, 0.0f,  0.0f,  0.0f},
        {0.0f,  0.0f,  1.0f,  0.0f},
        {0.0f,  0.0f,  0.0f,  1.0f}
    };
    kr::Mat4 m1 = kr::load_mat4(matrix1);
    const auto zero = kr::make_vec4(0.0f, 0.0f, 0.0f, 0.0f);
    const auto zero_prime = kr::transform(m1, zero);
    require_equal_vec4s(zero, zero_prime);
    const auto x = kr::make_vec4(1.0f, 0.0f, 0.0f, 1.0f);
    const auto x_prime = kr::transform(m1, x);
    require_equal_vec4s(x_prime, kr::make_vec4(0.0f, -1.0f, 0.0f, 1.0f));
    require_equal_vec4s(kr::transform(m1, kr::make_vec4(-1.0f, 0.0f, 0.0f, 1.0f)), kr::make_vec4(0.0f, 1.0f, 0.0f, 1.0f));
    require_equal_vec4s(kr::transform(m1, kr::make_vec4(0.0f, 1.0f, 0.0f, 1.0f)),  kr::make_vec4(1.0f, 0.0f, 0.0f, 1.0f));
    require_equal_vec4s(kr::transform(m1, kr::make_vec4(0.0f, -1.0f, 0.0f, 1.0f)), kr::make_vec4(-1.0f, 0.0f, 0.0f, 1.0f));
    require_equal_vec4s(kr::transform(m1, kr::make_vec4(0.0f, 0.0f, 1.0f, 1.0f)),  kr::make_vec4(0.0f, 0.0f, 1.0f, 1.0f));
    require_equal_vec4s(kr::transform(m1, kr::make_vec4(0.0f, 0.0f, -1.0f, 1.0f)),  kr::make_vec4(0.0f, 0.0f, -1.0f, 1.0f));
}

TEST_CASE("Mat4 Vec4 transform randoms", "[gm]")
{
    for(unsigned rep = 0; rep < 100; ++rep)
    {
        const float matrix1[4][4] = {
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()},
            {randf(), randf(), randf(), randf()}
        };
        const kr::Mat4 m1 = kr::load_mat4(matrix1);
        // GLSL and thus GLM has column-major matrix constructors so we feed our row-major matrices in that order:
        glm::mat4 glm_m1 = glm::mat4(matrix1[0][0], matrix1[1][0], matrix1[2][0], matrix1[3][0],
                                    matrix1[0][1], matrix1[1][1], matrix1[2][1], matrix1[3][1],
                                    matrix1[0][2], matrix1[1][2], matrix1[2][2], matrix1[3][2],
                                    matrix1[0][3], matrix1[1][3], matrix1[2][3], matrix1[3][3]);

        const auto v = kr::make_vec4(randf(), randf(), randf(), randf());
        const auto v_prime = kr::transform(m1, v);
        const auto v_prime_glm = glm_m1 * glm::vec4(v[0], v[1], v[2], v[3]);
        require_equal_vec4s(v_prime, v_prime_glm);
    }
}

// Hardcoded maximum epsilon for float comparison.
inline bool fuzzy_equal(const kr::Vec4& l, const kr::Vec4& r)
{
    return abs(reduce(l - r)) < 0.000001f;
}

TEST_CASE("Mat4 rotation about x", "[gm]")
{
    auto m = kr::make_rotation_x_mat4(M_PI_2);
    REQUIRE(m[0][0] == 1.0f);
    kr::Mat4InMemory mm;
    kr::store(m, mm);
    REQUIRE(m[0][0] == 1.0f);
    REQUIRE(true == fuzzy_equal(load(mm.rows[0]), kr::make_vec4(1,0,0,0)));
    REQUIRE(true == fuzzy_equal(load(mm.rows[1]), kr::make_vec4(0,0,-1,0)));
    REQUIRE(true == fuzzy_equal(load(mm.rows[2]), kr::make_vec4(0,1,0,0)));
    REQUIRE(true == fuzzy_equal(load(mm.rows[3]), kr::make_vec4(0,0,0,1)));

}

}