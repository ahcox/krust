// GLSL Compute shader.
// Draws spheres sitting on a checkerboard plane with a checkerboard ceiling above and gradient sky behind.
#version 450 core
#extension GL_GOOGLE_include_directive : require
#include "header.inc.glsl"
#include "intersections.inc.glsl"

// Define the number of samples of each pixel in X and Y by passing a value for this to the
// GLSL compiler (AA = 4 -> 16 samples, AA = 10 -> 100 samples):
#ifndef AA
#define AA 4
#endif
const float INV_NUM_SAMPLES = float(double(1.0) / (AA*AA));
const float halfpixel_dim = 0.5f;
const float subpixel_dim = float(double(1.0) / AA);
const float half_subpixel_dim = subpixel_dim * 0.5f;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 0) uniform image2D framebuffer;
/// @todo Pack into minimal number of vec4s.
layout(push_constant) uniform frame_params_t
{
    /// @todo pack width and height into one 32 bit uint.
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t frame_no;
    vec3 ray_origin;
    vec3 ray_target_origin;
    vec3 ray_target_right;
    vec3 ray_target_up;
} fp;

// Floor checkerboard:
const vec3 check_a = vec3(0.9, 0.1, 0.0);
const vec3 check_b = vec3(0.7, 0.9, 0.0);
const vec3 check_blur = (check_a + check_b) * 0.5f;
// Roof checkerboard:
const vec3 check_c = vec3(0.0, 0.1, 9.0);
const vec3 check_d = vec3(0.0, 0.9, 0.7);
const vec3 roof_check_blur = (check_c + check_d) * 0.5f;

// Our scene (a sphere is {x,y,x,radius}):
const vec4 spheres[] = {
    vec4(0,0,-1, 0.5),
    vec4(0,-100.5,-1, 100),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88, 0)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88, 1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88, 0)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88, 1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0,  88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0,  88, 1)) * 100, 0.5f),
};

/// A way to track the chosen intersection while traversing a number of primitives.
struct HitRecord {
    float t; /// @note Could be fixed point between tmin and tmax for more precision or smaller size (if 16 bit 0.16).
    uint16_t prim;
    bool front_face;
};

/// Traverse our scene, which is just a single list of spheres in worldspace,
/// and determine the distance to the first hit point.
/// @return true if there was a hit, else false.
bool closest_sphere_hit(in vec3 ray_origin, in vec3 ray_dir_unit, in float t_min, inout HitRecord hit)
{
    hit.front_face = true; // Our sphere intersect fails on backfaces currently.
    bool found_hit = false;
    for(uint i = 0; i < spheres.length(); ++i)
    {
        float t;
        if(sphere_hit(ray_origin, ray_dir_unit, spheres[i], t)){
            if(t >= t_min && t < hit.t){
                hit.t = t;
                hit.prim = uint16_t(i);
                found_hit = true;
            }
        }
    }
    return found_hit;
}

const uint MAX_REC = 20u;
/// Equivalent to `ray_color()`in section 8.2 of Ray Tracing in One Week
/// but with recursion replaced by a loop.
vec3 shoot_ray_recursive(inout highp uint32_t seed, in vec3 ray_origin, in vec3 ray_dir_unit)
{
    float attenuation = 1.0f;
    for(uint ray_segment = 0; ray_segment < MAX_REC; ++ray_segment)
    {
        HitRecord hit;
        hit.t = MISS;
        if(closest_sphere_hit(ray_origin, ray_dir_unit, 0.001f, hit)){

            // Bounce a ray off in a random direction:
            vec4 sphere = spheres[uint(hit.prim)];
            const vec3 new_ray_origin = ray_origin + ray_dir_unit * hit.t;
            const vec3 surface_norm = (new_ray_origin - sphere.xyz) * (1.0f / sphere.w);
            ray_dir_unit = normalize((new_ray_origin + surface_norm + rand_point_in_unit_sphere(seed)) - new_ray_origin);
            ray_origin = new_ray_origin;
            attenuation *= 0.5f;
        } else {
            break;
        }
    }

    // Default to background colour:
    // To match the CPU version we'd branch on having hit MAX_REC and shade black
    // instead of sky colour but attenuation would be 1/(2^MAC_REC) in that case anyway.
    const float wg_shade = (ray_dir_unit.y + 1.0f) * 0.5f;
    vec3 pixel = mix(vec3(1,1,1), vec3(0.5f,0.7f,1.0f), wg_shade);
    pixel *= attenuation;

    return pixel;
}

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

void main()
{
    // Bottom-left relative integer screen cordinates:
    const highp uint32_t screen_coord_x = gl_GlobalInvocationID.x;
    const highp uint32_t screen_coord_y = fp.fb_height - gl_GlobalInvocationID.y;

    // Generate a seed for the PRNG that differs between neighbouring pixels:
    //highp uint32_t random_seed = screen_coord_y * fp.fb_height + screen_coord_x;
    //highp uint32_t random_seed = screen_coord_x * fp.fb_width + screen_coord_y;
    highp uint32_t random_seed = make_seed(screen_coord_x, screen_coord_y, fp.frame_no);

    // Pump the RNG in an attempt to make random sequences have less visible
    /// coherence across regions of the image (fewer artifacts):
    uint num_pumps = 1u;
    // Pump different amounts for neighbouring pixels:
    //num_pumps +=  (screen_coord_x & 0x7) + (screen_coord_y & 0x7);
    num_pumps += ((screen_coord_x & 0x3) + (screen_coord_y & 0x7)) * 17; // Markedly different numbers of pumps
    // Multiresolution tiles of differring numbers of pumps:
#if 0
    num_pumps += (screen_coord_x >> 1U) & 0x1;
    num_pumps += (screen_coord_y >> 1U) & 0x1;
    num_pumps += (screen_coord_x >> 3U) & 0x1;
    num_pumps += (screen_coord_y >> 3U) & 0x1;
    num_pumps += (screen_coord_x >> 5U) & 0x1;
    num_pumps += (screen_coord_y >> 5U) & 0x1;
    num_pumps += (screen_coord_x >> 7U) & 0x1;
    num_pumps += (screen_coord_y >> 7U) & 0x1;
#endif
    //num_pumps += fp.frame_no;// & 0xf;
    pump_seed_n(random_seed, num_pumps);

    // Visualize the randomness we have:
    // It is worth doing this with pump_seed_n(random_seed, fp.frame_no); above to see patterns appear.
#if 0
    const vec4 pixel = mix(vec4(0,0,0,1), vec4(1.0f,1.0f,1.0f,1.0f), randf_zero_to_one(random_seed)); // draw random numbers as gradient to check seed is uncorrelated between neighbours.
    //const vec4 pixel = vec4((random_seed & 255u)/255.0f, ((random_seed >> 8u) & 255u)/255.0f, ((random_seed >> 16u) & 255u)/255.0f, ((random_seed >> 24u) & 255u)/255.0f);
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), pixel);
#else
#if AA < 2
    // Generate the single per-fragment ray:
    const vec3 ray_dir_raw = (fp.ray_target_origin + fp.ray_target_right * (screen_coord_x + 0.5f) + fp.ray_target_up * (screen_coord_y + 0.5f)) - fp.ray_origin;
    const vec3 ray_dir_unit = normalize(ray_dir_raw);

    vec3 pixel = shoot_ray_recursive(random_seed, fp.ray_origin, ray_dir_unit);
#else
    // Shoot a grid of rays across the pixel:
    vec3 pixel = {0, 0, 0};
    for(int j = 0; j < AA; ++j){
        const float y_offset = j * subpixel_dim + half_subpixel_dim;
        const float sample_y = screen_coord_y + y_offset;

        for(int i = 0; i < AA; ++i){
            const float x_offset = i * subpixel_dim + half_subpixel_dim;
            const float sample_x = screen_coord_x + x_offset;

            const vec3 ray_dir_raw = (fp.ray_target_origin + fp.ray_target_right * sample_x + fp.ray_target_up * sample_y) - fp.ray_origin;
            const vec3 ray_dir_unit = vec3(normalize(ray_dir_raw));

            pixel += shoot_ray_recursive(random_seed, fp.ray_origin, ray_dir_unit);
        }
    }
    pixel *= INV_NUM_SAMPLES;
#endif
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), vec4(pixel, 1.0f));
#endif
}
