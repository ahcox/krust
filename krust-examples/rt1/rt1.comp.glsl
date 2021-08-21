// GLSL Compute shader.
// Draws a single sphere sitting on a checkerboard plane with a checkerboard ceiling above and gradient sky behind.
// > If the ray hits the ground only, trace a few more sub-pixel rays against just the ground to blur out the aliasing.
//   > Implement FXAA first on this aliased mess.
#version 450 core
#extension GL_GOOGLE_include_directive : require
#include "intersections.inc.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 0) uniform image2D framebuffer;
layout(push_constant) uniform frame_params_t
{
    float fb_width;
    float fb_height;
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

const vec4 sphere1 = vec4(0,405,-900,405);

void main()
{

    // Bottom-left relative integer screen cordinates:
    const float screen_coord_x = gl_GlobalInvocationID.x;
    const float screen_coord_y = fp.fb_height - gl_GlobalInvocationID.y;

    // Generate the per-fragment ray:
    const vec3 ray_dir_raw = (fp.ray_target_origin + fp.ray_target_right * screen_coord_x + fp.ray_target_up * screen_coord_y) - fp.ray_origin;
    const vec3 ray_dir_unit = normalize(ray_dir_raw);

    // Init pixel to background colour:
    const float wg_shade = (ray_dir_unit.y + 1.0f) * 0.5f;
    vec3 pixel = mix(vec3(1,1,1), vec3(0.5f,0.7f,1.0f), wg_shade);
    pixel = mix(vec3(1,1,1), vec3(0.1f,0.2f,1.0f), wg_shade);

    // Intersect the ray with the ground plane:
    float t = MISS;
    const float ground_t = -fp.ray_origin.y / ray_dir_unit.y;
    if((ground_t > 0.5f) && (ground_t < 2000000.0f))
    {
        vec2 ground_hit = fp.ray_origin.xz + ray_dir_unit.xz * ground_t;
        t = ground_t;
        // Todo: perturb the hit point for a wobbly checkerboard
        ivec2 checker = ivec2(floor(ground_hit / 128));
        vec3 floor_col = check_b;
        if((checker.x & 1) == (checker.y & 1)) {
            floor_col = check_a;
        }
        // Blur the checks into an average colour in the distance:
        pixel = mix(floor_col, check_blur, clamp(distance(fp.ray_origin.xz, ground_hit) / 8192, 0.0, 1.0));
    }
    // Intersect with a roof plane if the floor was missed:
    else {
        const float roof_t = (1536.0f-fp.ray_origin.y) / ray_dir_unit.y;
        if((roof_t > 0.5f) && (roof_t < 2000000.0f))
        {
            vec2 hit_xy = fp.ray_origin.xz + ray_dir_unit.xz * roof_t;
            t = roof_t;
            ivec2 checker = ivec2(floor(hit_xy / 128));
            vec3 roof_col = check_d;
            if((checker.x & 1) == (checker.y & 1)) {
                roof_col = check_c;
            }
            // Blur the checks into an average colour in the distance:
            pixel += mix(roof_col, roof_check_blur, clamp(distance(fp.ray_origin.xz, hit_xy) / 8192, 0.0, 1.0));
            pixel *= 0.5f;
        }
    }

    // Shoot some rays at some spheres:
    const float t_sphere = isect_sphere(fp.ray_origin, ray_dir_unit, 0.5f, sphere1.xyz, sphere1.w);
    if(t_sphere > 0.5f && t_sphere < t){
        t = t_sphere;
        // Shade it based on normal direction:
        const vec3 norm = ((fp.ray_origin + ray_dir_unit * t_sphere) - sphere1.xyz) * (1.0f / sphere1.w);
        pixel = norm;
    }

    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), vec4(pixel, 1.0f));
}
