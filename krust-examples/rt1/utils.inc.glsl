#ifndef KRUST_UTILS_INC_INCLUDED
#define KRUST_UTILS_INC_INCLUDED
/// @file Utilities used widely in shaders.

float length_squared(const vec3 v)
{
    return dot(v, v);
}

vec3 pow(in vec3 v, in float p)
{
  return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

mediump float pow_mp(in mediump float v, in mediump float p)
{
  return pow(v, p);
}
mediump vec3 pow_mp(in mediump vec3 v, in mediump float p)
{
  return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

mediump vec3 linear_to_gamma(in mediump vec3 colour)
{
  return pow_mp(colour, float(1.0/2.2));
}

/// Cheaper gamma correction assuming 2.0 gamma
mediump vec3 linear_to_gamma_2_0(in mediump vec3 colour)
{
  return sqrt(colour);
}

/// Nasty version of sRGB gamma correction.
/// See the standard:
/// <https://web.archive.org/web/20200608215908/https://www.sis.se/api/document/preview/562720/>
mediump float linear_to_gamma_precise(in mediump float comp)
{
  if(comp > 0.0031308f){
    return 1.055f * pow_mp(comp, float(1.0/2.4)) - 0.055f;
  }
  // If the value is negative, keep it so after correction:
  if(comp < -0.0031308f){
    return -1.055f * pow_mp(-comp, float(1.0/2.4)) + 0.055f;
  }
  // Linear close to zero:
  return 12.92f * comp;
}

/// Nasty version of sRGB gamma correction
mediump vec3 linear_to_gamma_precise(in mediump vec3 colour)
{
    return vec3(linear_to_gamma_precise(colour.r), linear_to_gamma_precise(colour.g), linear_to_gamma_precise(colour.b));
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
    re += (b & 0xffffff00u) << 16u;

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

// Seed a random number generator based on 2D position and a monotonically
// increasing frame number.
highp uint32_t make_seed(in highp uint32_t x, in highp uint32_t y, in highp uint32_t frame)
{
    //const highp uint32_t pos_part = 0;
    //const highp uint32_t pos_part = (y << 1u) ^ x; // Note this is deliberately making patterns.
    //const highp uint32_t pos_part = concat12bits(x, x);
    const highp uint32_t pos_part = interleave12bits(x, y);
    //const highp uint32_t pos_part = interleave16bits(x, y);

    // Add some bits that change every frame:
    const highp uint32_t with_frame = pos_part;  // Ignore the changing frame number and generate identical sequence of random numbers every frame.
    //const highp uint32_t with_frame = pos_part ^ frame; // Looks okay but doesn't change much each frame
    //const highp uint32_t with_frame = pos_part * 256u + (frame & 255u); // Looks the best of these
    //const highp uint32_t with_frame = pos_part * 16u + (frame & 15u);
    //const highp uint32_t with_frame = (pos_part & 0x00ffffffu) + (frame << 24u); //* 16777216u; // Shadow seems to be flowing horizontally with interleaved bits and 1 SPP
    //const highp uint32_t with_frame = interleave8and24bits(frame, pos_part);
    //const highp uint32_t with_frame = pos_part + frame;

    const highp uint32_t seed = with_frame;

    return seed;
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

/// @return A random unit vector.
vec3 rand_vector(inout highp uint32_t seed)
{
  return normalize(rand_point_in_unit_sphere(seed));
}

/// @return A random unit vector pointing into .
vec3 rand_point_on_unit_hemisphere(inout highp uint32_t seed, in vec3 norm)
{
  vec3 sp = rand_point_in_unit_sphere(seed);
  // If distance projected onto normal is negative, we need to multiply by -1 to flip it,
  // but rather than branch we multiply by the distance, which has an appropriate sign,
  // since the result is normalised to unit length afterwards anyway.
  const float dist = dot(norm, sp);
  return normalize(sp * dist);
}

/// @}

#endif // KRUST_UTILS_INC_INCLUDED
