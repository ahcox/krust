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

#include "catch.hpp"

/* ----------------------------------------------------------------------- *//**
 * @file Test of the Vec4 class.
 */
#include "krust-gm/public-api/vec4.h"
#include "krust-gm/public-api/vec4_inl.h"
namespace {
namespace kr = Krust;

TEST_CASE("Vec4 Template", "[gm]")
{
  bool destroyed = false;
  REQUIRE(destroyed == false);
}

TEST_CASE("Vec4 dot", "[gm]")
{
    auto v1 = kr::make_vec4(1, 1, 1, 1);
    auto v2 = kr::make_vec4(2, 2, 2, 2);
    REQUIRE(kr::dot(v1, v1) == 4.0f);
    REQUIRE(kr::dot(v1, v2) == 8.0f);
    REQUIRE(kr::dot(v2, v2) == 16.0f);
    auto v3 = kr::make_vec4(2.28383383f, 19.2282f, 0.8292746462f, 0.18388383f);
    REQUIRE(kr::dot(v1, v3) == 2.28383383f + 19.2282f + 0.8292746462f + 0.18388383f);
    REQUIRE(kr::dot(v1, v3) == kr::hadd(v3));
}

TEST_CASE("Vec4 load", "[gm]")
{
    float vmem[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    kr::Vec4 v1 = kr::loadf4(vmem);
    REQUIRE(v1[0] == 1.0f);
    REQUIRE(v1[1] == 2.0f);
    REQUIRE(v1[2] == 3.0f);
    REQUIRE(v1[3] == 4.0f);
}

TEST_CASE("Vec4 store", "[gm]")
{
    float vmem[4] = {-1.0f, -2.0f, -3.0f, -4.0f};
    kr::Vec4 v1 = kr::make_vec4(1, 2, 3, 4);
    kr::store(v1, vmem);
    REQUIRE(vmem[0] == 1.0f);
    REQUIRE(vmem[1] == 2.0f);
    REQUIRE(vmem[2] == 3.0f);
    REQUIRE(vmem[3] == 4.0f);
}

TEST_CASE("Vec4 load struct", "[gm]")
{
    const kr::Vec4InMemory vmem = {1.0f, 2.0f, 3.0f, 4.0f};
    kr::Vec4 v1 = kr::load(vmem);
    REQUIRE(v1[0] == 1.0f);
    REQUIRE(v1[1] == 2.0f);
    REQUIRE(v1[2] == 3.0f);
    REQUIRE(v1[3] == 4.0f);
}

TEST_CASE("Vec4 store struct", "[gm]")
{
    kr::Vec4InMemory vmem = {-1.0f, -2.0f, -3.0f, -4.0f};
    kr::Vec4 v1 = kr::make_vec4(1, 2, 3, 4);
    kr::store(v1, vmem);
    REQUIRE(vmem.v[0] == 1.0f);
    REQUIRE(vmem.v[1] == 2.0f);
    REQUIRE(vmem.v[2] == 3.0f);
    REQUIRE(vmem.v[3] == 4.0f);
}

TEST_CASE("Vec4 compound", "[gm]")
{
    kr::Vec4InMemory vmem1 = { 1.0f,  2.0f,  3.0f,  4.0f};
    kr::Vec4InMemory vmem2 = {-1.0f, -2.0f, -3.0f, -4.0f};
    float vmem3[4]         = { 10.0f,  20.0f,  30.0f,  40.0f};
    float vmem4[4]         = {-10.0f, -20.0f, -30.0f, -40.0f};
    kr::Vec4 v1 = kr::make_vec4(
        kr::dot(kr::load(vmem1), kr::load(vmem2)),
        kr::dot(kr::loadf4(vmem3), kr::loadf4(vmem4)),
        kr::dot(kr::load(vmem1), kr::loadf4(vmem3)),
        kr::dot(kr::load(vmem2), kr::loadf4(vmem4))
    );
    REQUIRE(v1[0] == -1 + -2*2 + -3*3 + -4*4);
    REQUIRE(v1[1] == -10*10 + -20*20 + -30*30 + -40*40);
    REQUIRE(v1[2] == 1 * 10 + 2*20 + 3*30 + 4*40);
    REQUIRE(v1[3] == -1 * -10 + -2*-20 + -3*-30 + -4*-40);
}

}