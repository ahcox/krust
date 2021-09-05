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
    vec4(0,405,-900,405),
    vec4(0,810 + 200,-900,200),
    vec4(0,810 + 400 + 100,-900, 100),
    vec4(0,810 + 400 + 200 + 50,-900, 50),
    vec4(0,810 + 400 + 200 + 100 + 25,-900, 25),
    vec4(0,810 + 400 + 200 + 100 + 50 + 12.5f,-900, 12.5f),
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

/// @return Colour for a single ray.
vec3 shoot_ray(in vec3 ray_origin, in vec3 ray_dir_unit)
{
    vec3 pixel;
    float t = MISS;
    {
        HitRecord hit;
        hit.t = MISS;
        if(closest_sphere_hit(ray_origin, ray_dir_unit, 0.5f, hit)){
            t = hit.t;
            // Shade it based on normal direction:
            vec4 sphere = spheres[uint(hit.prim)];
            const vec3 norm = ((ray_origin + ray_dir_unit * t) - sphere.xyz) * (1.0f / sphere.w);
            pixel = norm;
        }
    }

    // Intersect the ray with the ground plane:
    const float ground_t = -ray_origin.y / ray_dir_unit.y;
    if((ground_t > 0.5f) && (ground_t < t))
    {
        vec2 ground_hit = ray_origin.xz + ray_dir_unit.xz * ground_t;
        t = ground_t;
        ivec2 checker = ivec2(floor(ground_hit / 128));
        vec3 floor_col = check_b;
        if((checker.x & 1) == (checker.y & 1)) {
            floor_col = check_a;
        }
        // Blur the checks into an average colour in the distance:
        pixel = mix(floor_col, check_blur, clamp(distance(ray_origin.xz, ground_hit) / 8192, 0.0, 1.0));
    }
    // Intersect with a roof plane if the floor was missed:
    else {
        const float roof_t = (1536.0f-ray_origin.y) / ray_dir_unit.y;
        if((roof_t > 0.5f) && (roof_t < t))
        {
            // Init pixel to that original gradient background colour:
            const float wg_shade = (ray_dir_unit.y + 1.0f) * 0.5f;
            // pixel = mix(vec3(1,1,1), vec3(0.5f,0.7f,1.0f), wg_shade); ///< Original pale gradient
            // More intense blue:
            pixel = mix(vec3(1,1,1), vec3(0.1f,0.2f,1.0f), wg_shade);
            {
                vec2 hit_xy = ray_origin.xz + ray_dir_unit.xz * roof_t;
                t = roof_t;
                ivec2 checker = ivec2(floor(hit_xy / 128));
                vec3 roof_col = check_d;
                if((checker.x & 1) == (checker.y & 1)) {
                    roof_col = check_c;
                }
                // Blur the checks into an average colour in the distance and mix with gradient:
                pixel += mix(roof_col, roof_check_blur, clamp(distance(ray_origin.xz, hit_xy) / 8192, 0.0, 1.0)) * 4.0f;
                pixel *= 0.2;
            }
        }
    }
    return pixel;
}

void main()
{
    // Bottom-left relative integer screen cordinates:
    const float screen_coord_x = gl_GlobalInvocationID.x;
    const float screen_coord_y = fp.fb_height - gl_GlobalInvocationID.y;

#if AA < 2
    // Generate the per-fragment ray:
    const vec3 ray_dir_raw = (fp.ray_target_origin + fp.ray_target_right * (screen_coord_x + 0.5f) + fp.ray_target_up * (screen_coord_y + 0.5f)) - fp.ray_origin;
    const vec3 ray_dir_unit = normalize(ray_dir_raw);

    vec3 pixel = shoot_ray(fp.ray_origin, ray_dir_unit);
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

            pixel += shoot_ray(fp.ray_origin, ray_dir_unit);
        }
    }
    pixel *= INV_NUM_SAMPLES;
#endif
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), vec4(pixel, 1.0f));
}
