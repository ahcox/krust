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
 * @file Test of the Vec3 class.
 */
#include "krust-gm/public-api/vec3.h"
#include "krust-gm/public-api/vec3_inl.h"
namespace {
namespace kr = Krust;

TEST_CASE("Template", "[gm]")
{
  bool destroyed = false;
  REQUIRE(destroyed == false);
}

TEST_CASE("dot", "[gm]")
{
    auto v1 = kr::make_vec3(1, 1, 1);
    auto v2 = kr::make_vec3(2, 2, 2);
    REQUIRE(kr::dot(v1, v1) == 3.0f);
    REQUIRE(kr::dot(v1, v2) == 6.0f);
    REQUIRE(kr::dot(v2, v2) == 12.0f);
    auto v3 = kr::make_vec3(2.28383383f, 19.2282f, 0.8292746462f);
    REQUIRE(kr::dot(v1, v3) == 2.28383383f + 19.2282f + 0.8292746462f);
    REQUIRE(kr::dot(v1, v3) == kr::hadd(v3));
}

TEST_CASE("cross", "[gm]")
{
    const auto v1 = kr::make_vec3(1, 0, 0);
    const auto v2 = kr::make_vec3(0, 1, 0);
    const auto v3 = kr::make_vec3(0, 0, 1);
    REQUIRE(all_of(kr::cross(v1, v2) == v3));
    REQUIRE(all_of(kr::cross(v2, v1) == -v3));
    REQUIRE(all_of(kr::cross(v2, v3) == v1));
    REQUIRE(all_of(kr::cross(v3, v2) == -v1));
    REQUIRE(all_of(kr::cross(v3, v1) == v2));
    REQUIRE(all_of(kr::cross(v1, v3) == -v2));
    const auto v4 = kr::make_vec3(3.99f, 10/17.0f, 119.0f);
    REQUIRE(all_of(kr::cross(v1, v4) == -kr::cross(v4, v1)));
    REQUIRE(all_of(kr::cross(v2, v4) == -kr::cross(v4, v2)));
    REQUIRE(all_of(kr::cross(v3, v4) == -kr::cross(v4, v3)));
    const auto v5 = kr::make_vec3(123.0199f, 1024/17.0f, 3.14159265f);
    REQUIRE(all_of(kr::cross(v5, v4) == -kr::cross(v4, v5)));
}

}