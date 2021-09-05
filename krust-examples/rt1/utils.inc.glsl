#ifndef KRUST_UTILS_INC_INCLUDED
#define KRUST_UTILS_INC_INCLUDED
/// @file Utilities used widely in shaders.

float length_squared(const vec3 v)
{
    return dot(v, v);
}

highp uint32_t interleave12bits(in highp uint32_t a, in highp uint32_t b)
{
    highp uint32_t re = 0u;
    for(uint i = 0; i < 12u; ++i)
    {
        re |= ((a >> i) & 1u) << i * 2;
        re |= ((b >> i) & 1u) << i * 2 + 1u;
    }

    return re;
}

highp uint32_t interleave16bits(in highp uint32_t a, in highp uint32_t b)
{
    highp uint32_t re = 0u;
    for(uint i = 0; i < 16u; ++i)
    {
        re |= ((a >> i) & 1u) << i * 2;
        re |= ((b >> i) & 1u) << i * 2 + 1u;
    }

    return re;
}

highp uint32_t interleave8and24bits(in highp uint32_t a, in highp uint32_t b)
{
    highp uint32_t re = 0u;
    for(uint i = 0; i < 8u; ++i)
    {
        re |= ((a >> i) & 1u) << i * 2;
        re |= ((b >> i) & 1u) << i * 2 + 1u;
    }
    re += (b & 0xffffff00u) << 8u;

    return re;
}

/// Takes low 12 bits from two numbers and returns a number with the low 24 bits
/// formed by placing them side by side.
highp uint32_t concat12bits(in highp uint32_t x, in highp uint32_t y)
{
    // highp uint32_t sidebyside = (y & 4095u) * 4096u + (x & 4095u);
    // Assume numbers are less than 4096:
    highp uint32_t sidebyside = y * 4096u + x;
    return sidebyside;
}

///
/// @name RandomNumbers Pseudorandom number generation.
/// The first seed passed-in should differ for every invocation, e.g. by
/// deriving it from the X,Y[,Z] coordinates of its gl_GlobalInvocationID or
/// screen position. Each shader invocation should iterate its own version of
/// this seed to provide bits for its random numbers of all kinds: integers,
/// floats in 0-1 range, floats in -1 to +1 range, etc.
///@{


/// The constants for this standard LCG are derived from Listing 37-4 of GPU
/// Gems 3 which got them from the "Quick and Dirty" LCG of Numerical Recipes in
/// C, Second Edition, Press et al. 1992.
void pump_seed(inout highp uint32_t seed)
{
  seed = seed * 1664525U + 1013904223U;
}

void pump_seed_n(inout highp uint32_t seed, in uint iters)
{
  for(uint i = 0U; i < iters; ++i){
    pump_seed(seed);
  }
}


/// Random floating point number in the -1 to 1 range.
/// Assumes a 1:8:23 bit representation of floats.
/// The conversion of bits to float is based on C code here:
/// <https://www.iquilezles.org/www/articles/sfrand/sfrand.htm>
/// but a different LCG PRNG is used as the one he presents has a period that
/// depends on the seed used and ranges from 1 in the worst case (for a zero
/// seed) up to around 500k, far short of the ~4 billion we want from a 32 bit
/// seed.
float randf_one_to_one(inout highp uint32_t seed)
{
  pump_seed(seed);
  // Combine the sign bit and exponent for a number in the 2 to 4 range with our
  // random 23 bits of mantissa:
  const highp uint32_t ires = (seed >> 9U) | uint32_t(0x40000000U);
  // Translate our random number between 2 and 4 into the -1 to +1 range:
  return uintBitsToFloat(ires) - 3.0f;
}

/// Random floating point number in the 0 to 1 range.
/// Put 23 random integer bits in the mantissa and set the sign and exponent
/// Based on C code here: https://www.iquilezles.org/www/articles/sfrand/sfrand.htm
float randf_zero_to_one(inout highp uint32_t seed)
{
  pump_seed(seed);
  const highp uint32_t ires = (seed >> 9U) | uint32_t(0x3f800000U);
  return uintBitsToFloat(ires) - 1.0f;
}

/// @return Point in -1 to 1 cube.
vec3 rand_point(inout highp uint32_t seed)
{
  const float x = randf_one_to_one(seed);
  const float y = randf_one_to_one(seed);
  const float z = randf_one_to_one(seed);
  return vec3(x, y, z);
}

/// @return Point inside or on surface of sphere of radius one.
vec3 rand_point_in_unit_sphere(inout highp uint32_t seed)
{
  vec3 candidate;
  while(true){ // Prone to infinite looping with original LCG but should be fine now
  // for(int i = 0; i < 100; ++i){ // Fixed max iters to avoid GPU hangs / device lost while using very rough LCG PRNG.
    candidate = rand_point(seed);
    if(length_squared(candidate) <= 1.0f){
      break;
    }
  }
  return candidate;
}

/// @}

#endif // KRUST_UTILS_INC_INCLUDED
