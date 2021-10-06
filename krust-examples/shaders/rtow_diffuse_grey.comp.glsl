// GLSL Compute shader.
// Draws spheres sitting on a checkerboard plane with a checkerboard ceiling above and gradient sky behind.
#version 450 core
#extension GL_GOOGLE_include_directive : require
#include "header.inc.glsl"
#include "intersections.inc.glsl"

// Are spheres solids or just infinitely thin surface shells?
#ifndef SPHERES_ARE_SOLID
#define SPHERES_ARE_SOLID 0
#endif

// Should we shoot rays from intersection points off into the surface hemisphere
// or sample an offset unit sphere's surface?
#ifndef USE_HEMISPHERE
#define USE_HEMISPHERE 1
#endif

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


// Our scene (a sphere is {x,y,x,radius}):
const vec4 spheres[] = {
    vec4(0,0,-1, 0.5),
    vec4(0,-100.5,-1, 100),
#if 1
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88, 0)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(1,  88, 1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88, 0)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-1, 88, 1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0,  88,-1)) * 100, 0.5f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0,  88, 1)) * 100, 0.5f),

    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.5,  88, 0.5)) * 100, 0.25f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.25,  88, 0.25)) * 100, 0.125f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.125,  88, 0.125)) * 100, 0.0625f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.5,  88, 0.5)) * 100, 0.25f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.25,  88, 0.25)) * 100, 0.125f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.125,  88, 0.125)) * 100, 0.0625f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.5,  88, -0.5)) * 100, 0.25f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.25,  88, -0.25)) * 100, 0.125f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(0.125,  88, -0.125)) * 100, 0.0625f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.5,  88, -0.5)) * 100, 0.25f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.25,  88, -0.25)) * 100, 0.125f),
    vec4(vec3(0,-100.5,-1) + normalize(vec3(-0.125,  88, -0.125)) * 100, 0.0625f),
#endif
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
    bool found_hit = false;
    for(uint i = 0; i < spheres.length(); ++i)
    {
        //float t;
        //if(sphere_hit(ray_origin, ray_dir_unit, spheres[i], t)){
        float t1, t2;
        if(sphere_hits(ray_origin, ray_dir_unit, spheres[i], t1, t2)){
            if(t1 >= t_min && t1 < hit.t){
                hit.t = t1;
                hit.prim = uint16_t(i);
                found_hit = true;
                hit.front_face = true;
            }
            if(t2 >= t_min && t2 < hit.t){
                hit.t = t2;
                hit.prim = uint16_t(i);
                found_hit = true;
                hit.front_face = false;
            }

        }
    }
    return found_hit;
}

bool closest_solid_sphere_hit(in vec3 ray_origin, in vec3 ray_dir_unit, in float t_min, inout HitRecord hit)
{
    hit.front_face = true; // Our sphere intersect fails on backfaces currently.
    bool found_hit = false;
    for(uint i = 0; i < spheres.length(); ++i)
    {
        float t1, t2;
        if(sphere_hits(ray_origin, ray_dir_unit, spheres[i], t1, t2)){
            if(t1 < t_min && t2 > t_min){
                hit.t = t_min;
                hit.prim = uint16_t(i);
                found_hit = true;
                hit.front_face = false;
                break;
            }
            if(t1 >= t_min && t1 < hit.t){
                hit.t = t1;
                hit.prim = uint16_t(i);
                found_hit = true;
            }
        }
    }
    return found_hit;
}

const uint MAX_BOUNCE = 9u;
/// Equivalent to `ray_color()`in section 8.2 of Ray Tracing in One Week
/// but with recursion replaced by a loop.
vec3 shoot_ray(inout highp uint32_t seed, in vec3 ray_origin, in vec3 ray_dir_unit)
{
    float attenuation = 1.0f;
    for(uint ray_segment = 0; ray_segment < MAX_BOUNCE; ++ray_segment)
    {
        HitRecord hit;
        hit.t = MISS;

#if SPHERES_ARE_SOLID
        if(closest_solid_sphere_hit(ray_origin, ray_dir_unit, 0.001f, hit)){
#else
        if(closest_sphere_hit(ray_origin, ray_dir_unit, 0.001f, hit)){
#endif
            // Bounce a ray off in a random direction:
            vec4 sphere = spheres[uint(hit.prim)];
            const vec3 new_ray_origin = ray_origin + ray_dir_unit * hit.t;
            const vec3 surface_norm = (new_ray_origin - sphere.xyz) * (1.0f / sphere.w) * (hit.front_face ? 1.0f : -1.0f);
#if USE_HEMISPHERE
            ray_dir_unit = rand_point_on_unit_hemisphere(seed, surface_norm);
#else
            ray_dir_unit = normalize((new_ray_origin + surface_norm +
                //rand_point_in_unit_sphere(seed))
                rand_vector(seed))
                - new_ray_origin);
#endif
            ray_origin = new_ray_origin;
            attenuation *= 0.5f;
        } else {
            break;
        }
    }

    // Default to background colour:
    // To match the CPU version we'd branch on having hit MAX_BOUNCE and shade black
    // instead of sky colour but attenuation would be 1/(2^MAX_BOUNCE) in that case anyway.
    const float wg_shade = (ray_dir_unit.y + 1.0f) * 0.5f;
    vec3 pixel = mix(vec3(1,1,1), vec3(0.5f,0.7f,1.0f), wg_shade);
    pixel *= attenuation;

    return pixel;
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

    vec3 pixel = shoot_ray(random_seed, fp.ray_origin, ray_dir_unit);
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

            pixel += shoot_ray(random_seed, fp.ray_origin, ray_dir_unit);
        }
    }
    pixel *= INV_NUM_SAMPLES;
#endif
    // We need to do gamma correction manually as automatic Linear->sRGB conversion
    // is not supported for image stores like it is when graphics pipelines write to
    // sRGB surfaces after blending:
    pixel = linear_to_gamma(pixel); // Rough version using a 2.2 exponent.
    // pixel = linear_to_gamma_precise(pixel); // A version with branches based on IEC 61966-2-1 spec.
    // Cheaper Gamma of 2:
    // pixel = linear_to_gamma_2_0(pixel);
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), vec4(pixel, 1.0f));
#endif
}
