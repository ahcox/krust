// GLSL Compute shader.
// Draws spheres sitting on a checkerboard plane with a checkerboard ceiling above and gradient sky behind.
#version 460 core
#extension GL_EXT_ray_query : require
#extension GL_EXT_ray_flags_primitive_culling : require // May not actually require this: usually we want to hit everything.
#extension GL_GOOGLE_include_directive : require
#include "header.inc.glsl"
#include "intersections.inc.glsl"

#ifndef GL_EXT_ray_query
#error "This shader requires the GL_EXT_ray_query extension."
#endif

// How far to shoot rays before giving up on finding a hit.
const float c_t_max = 16777216.0f; // Arbitrary number, but happens to be max float in integer precision range: 2^24.

// Disable all culling of instances when passed to rayQueryInitializeEXT().
const uint CULL_MASK_ALL_INTERSECTED = 0xFF;

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

// Should we turn on defocus blur (depth of field)?
#ifndef DOF
#define DOF 1
/// Define the radius of the lens. Rays are started from random poisitions on a
/// disk of this radius, distributed around the reference ray origin.
const float lens_radius = 0.01f;
#endif

#ifndef MAX_BOUNCE
const uint MAX_BOUNCE = 9u;
#endif

#ifndef USE_FINAL_SCENE
#define USE_FINAL_SCENE 1
#endif

const float inv_num_samples = float(double(1.0) / (AA*AA));
const float halfpixel_dim = 0.5f;
const float subpixel_dim = float(double(1.0) / AA);
const float half_subpixel_dim = subpixel_dim * 0.5f;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 0) uniform restrict writeonly image2D framebuffer;
layout(set = 0, binding = 1) uniform restrict readonly ub_t
{
    vec4 spheres[68];
} ub;
layout(set = 0, binding = 2) uniform accelerationStructureEXT sphere_tlas;

layout(push_constant) uniform frame_params_t
{
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t frame_no;
    vec3 ray_origin;
    // The ray target is the grid we are shooting prmary rays at.
    vec3 ray_target_origin;
    vec3 ray_target_right;
    vec3 ray_target_up;
} fp;

// Material Types.
const uint16_t MT_LAMBERTIAN = 0us;
const uint16_t MT_MIRROR     = 1us;
const uint16_t MT_METAL      = 2us;
const uint16_t MT_DIELECTRIC  = 3us;

// A reference to a material type and to a set of material parameters.
// E.g., Type: Lambertian Diffuse, Parameters: Albedo RGB.
struct MaterialRef {
    uint16_t material;
    uint16_t params;
};

/// A way to track the chosen intersection while traversing a number of primitives.
struct HitRecord {
    float t; /// @note Could be fixed point between tmin and tmax for more precision or smaller size (if 16 bit 0.16).
    uint16_t prim;
    bool front_face;
};

// Regenerate this using the following easily built code:
// https://github.com/ahcox/raytracing.github.io/blob/ahc-2021-10-01-dump_scene/src/InOneWeekend/main.cc
#include "rtow_final_scene.inc.glsl"

/// Traverse our scene, which is just a single BLAS of AABBs around spheres in worldspace,
/// and determine the distance to the first hit point.
/// @return true if there was a hit, else false.
bool closest_sphere_hit(in vec3 ray_origin, in vec3 ray_dir_unit, in float t_min, inout HitRecord hit)
{
    bool found_hit = false;
    rayQueryEXT query;
    rayQueryInitializeEXT(query, sphere_tlas, gl_RayFlagsNoneEXT, CULL_MASK_ALL_INTERSECTED, ray_origin, t_min, ray_dir_unit, c_t_max);
    bool traversing = true;
    while (traversing)
    {
        traversing = rayQueryProceedEXT(query);
        uint candidate_type = rayQueryGetIntersectionTypeEXT(query, false /* false = candidate, true = committed*/);
        if(candidate_type == gl_RayQueryCandidateIntersectionAABBEXT) // The only other candidate type would be a triangle.
        {
            int sphere_index = rayQueryGetIntersectionPrimitiveIndexEXT(query, false);
            float t1, t2;
            if(sphere_hits(ray_origin, ray_dir_unit, ub.spheres[sphere_index], t1, t2)){

                if(t1 >= t_min && t1 < hit.t){
                    hit.t = t1;
                    hit.prim = uint16_t(sphere_index);
                    found_hit = true;
                    hit.front_face = true; /// @note setting front and back faces like this breaks the negative radius sphere in sphere hack of book image 16. That requires generating the front/back status by dotting the ray and surface normal. Rather than copy that, I'll skip image 16 and later have better transparency handling that will allow an air sphere inside a glass one.
                }
                if(t2 >= t_min && t2 < hit.t){
                    hit.t = t2;
                    hit.prim = uint16_t(sphere_index);
                    found_hit = true;
                    hit.front_face = false;
                }
                rayQueryGenerateIntersectionEXT(query, hit.t);
            }
        }
    }

    return found_hit;
}

/// @todo Use ray queries like the non-solid version
bool closest_solid_sphere_hit(in vec3 ray_origin, in vec3 ray_dir_unit, in float t_min, inout HitRecord hit)
{
    hit.front_face = true; // Our sphere intersect fails on backfaces currently.
    bool found_hit = false;
    for(uint i = 0; i < ub.spheres.length(); ++i)
    {
        float t1, t2;
        if(sphere_hits(ray_origin, ray_dir_unit, ub.spheres[i], t1, t2)){
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

bool scatter_lambertian(
    inout uint32_t seed,
    in vec3 surface_norm,
    in uint param_index,
    out vec3 scattered_ray_dir,
    out vec3 attenuation)
{
    //scattered_ray_dir = rand_point_on_unit_hemisphere(seed, surface_norm);
    //scattered_ray_dir = normalize(surface_norm + rand_vector(seed) * 0.5f); // Concentrates shadows, big diff from hemisphere.

    // 1: shadows denser and more concentrated than hemisphere.
    //do{
    //    scattered_ray_dir = surface_norm + rand_vector(seed);
    //} while(near_zero(scattered_ray_dir));
    //scattered_ray_dir = normalize(scattered_ray_dir);

    // 2 (book):
    // Same as 1.
    scattered_ray_dir = surface_norm + rand_vector(seed);
    if(near_zero(scattered_ray_dir)){
        scattered_ray_dir = surface_norm;
        // scattered_ray_dir = vec3(0,-1,0); // Same as above for default scene and position.
    }
    scattered_ray_dir = normalize(scattered_ray_dir);

    attenuation = lambertian_params[param_index];
    return true;
}

bool scatter_mirror(
    in vec3 ray_dir_unit,
    in vec3 surface_norm,
    in uint param_index,
    out vec3 scattered_ray_dir,
    out vec3 attenuation)
{
    scattered_ray_dir = reflect(ray_dir_unit, surface_norm);
    attenuation = mirror_params[param_index];
    // This is fine for a perfect mirror (diffed image with this and with dot test to confirm):
    return true ;
}

bool scatter_metal(
    inout uint32_t seed,
    in vec3 ray_dir_unit,
    in vec3 surface_norm,
    in uint param_index,
    out vec3 scattered_ray_dir,
    out vec3 attenuation)
{
    const vec4 params = metal_params[param_index];
    const float roughness = params.a;
    scattered_ray_dir = normalize(reflect(ray_dir_unit, surface_norm) + rand_point_in_unit_sphere(seed) * roughness);
    attenuation = params.rgb;

    // The book does this but it makes horrible dark silhoutte artifacts visible in
    // figure 12 and even more so when flying around.
    // If we jitter the shot direction for fuzzy metal reflections this stops us shooting into the surface:
    // return dot(surface_norm, scattered_ray_dir) > 0.0f;

    // If the ray dips below horizon after roughness jitter, reflect it once more as
    // though it immediately hit the surface and bounce without losing energy:
    // Looks good.
    if(dot(surface_norm, scattered_ray_dir) < 0.0f){
        scattered_ray_dir = reflect(scattered_ray_dir, surface_norm);
        // Attenuate a bit more as though we have a double-hit:
        // (Makes a more modest silhouette than terminating the ray in blackness as the book does but looks nice)
        // attenuation *= attenuation;
    }
    return true;
}

bool scatter_dielectric(
    inout uint32_t seed,
    in vec3 ray_dir_unit,
    in vec3 surface_norm,
    in uint param_index,
    in bool front_face,
    out vec3 scattered_ray_dir,
    out vec3 attenuation)
{
    const vec4 params = dielectric_params[param_index];
    const float ir = front_face ? 1.0f / params.a : params.a;
    scattered_ray_dir = refract(ray_dir_unit, surface_norm, ir);
    attenuation = params.rgb;

    // Check whether we are coming in at a grazing angle and therefore need to reflect instead:
    const float cos_norm_ray_angle = min(1.0f, dot(-ray_dir_unit, surface_norm));
    const float sin_norm_ray_angle = sqrt(1.0f - cos_norm_ray_angle*cos_norm_ray_angle);

    if((ir * sin_norm_ray_angle > 1.0f)
       || (reflectance(cos_norm_ray_angle, ir) > randf_zero_to_one(seed))
    ){
        scattered_ray_dir = reflect(ray_dir_unit, surface_norm);
        //attenuation = vec3(1, 0, 0); // Rare. Line up three spheres and translate along their axis to see this branch reached.
    }

    return true;
}

// Dispatch to the correct scatter function for the material referenced in the hit record passed-in.
bool scatter(
    inout uint32_t seed,
    in vec3 ray_dir_unit,
    in vec3 hit_point,
    in vec3 surface_norm,
    in uint prim_index,
    in bool front_face,
    out vec3 scattered_ray_dir,
    out vec3 attenuation)
{
    switch(uint(sphere_materials[prim_index].material)){
        case uint(MT_LAMBERTIAN):
            return scatter_lambertian(seed, surface_norm, uint(sphere_materials[prim_index].params), scattered_ray_dir, attenuation);
        case uint(MT_MIRROR):
            return scatter_mirror(ray_dir_unit, surface_norm, uint(sphere_materials[prim_index].params), scattered_ray_dir, attenuation);
        case uint(MT_METAL):
            return scatter_metal(seed, ray_dir_unit, surface_norm, uint(sphere_materials[prim_index].params), scattered_ray_dir, attenuation);
        case uint(MT_DIELECTRIC):
            return scatter_dielectric(seed, ray_dir_unit, surface_norm, uint(sphere_materials[prim_index].params), front_face, scattered_ray_dir, attenuation);
    }
    return false;
}

/// Equivalent to `ray_color()`in section 8.2 of Ray Tracing in One Week
/// but with recursion replaced by a loop.
vec3 shoot_ray(inout highp uint32_t seed, in vec3 ray_origin, in vec3 ray_dir_unit)
{
    vec3 attenuation = vec3(1.0f);
    for(uint ray_segment = 0; ray_segment < MAX_BOUNCE; ++ray_segment)
    {
        HitRecord hit;
        hit.t = MISS;

#if SPHERES_ARE_SOLID
        #error "SPHERES_ARE_SOLID is not accelerated with ray queries yet."
        if(closest_solid_sphere_hit(ray_origin, ray_dir_unit, 0.001f, hit)){
#else
        if(closest_sphere_hit(ray_origin, ray_dir_unit, 0.001f, hit)){
#endif
            // Scatter a new ray from the intersection point according to the material
            // properties of the object we have hit:
            vec4 sphere = ub.spheres[uint(hit.prim)];
            const vec3 new_ray_origin = ray_origin + ray_dir_unit * hit.t;
            const vec3 surface_norm = (new_ray_origin - sphere.xyz) * ((1.0f / sphere.w) * (hit.front_face ? 1.0f : -1.0f));
            vec3 scatter_attenuation;
            if(!scatter(seed, ray_dir_unit, new_ray_origin, surface_norm, uint(hit.prim), hit.front_face, ray_dir_unit, scatter_attenuation)) {
                attenuation = vec3(0.0f);
                break;
            }
            ray_origin = new_ray_origin;
            attenuation *= scatter_attenuation;

            //if(ray_segment == MAX_BOUNCE - 1u) attenuation = vec3(0,0,0);
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
            vec3 ray_origin = fp.ray_origin;
#if DOF > 0
            const vec2 offset = rand_in_unit_disk(random_seed) * lens_radius;
            ray_origin += fp.ray_target_right * offset.x;
            ray_origin += fp.ray_target_up * offset.y;
#endif
            const vec3 ray_dir_raw = (fp.ray_target_origin + fp.ray_target_right * sample_x + fp.ray_target_up * sample_y) - ray_origin;
            const vec3 ray_dir_unit = vec3(normalize(ray_dir_raw));

            pixel += shoot_ray(random_seed, ray_origin, ray_dir_unit);
        }
    }
    pixel *= inv_num_samples;
#endif
    // We need to do gamma correction manually as automatic Linear->sRGB conversion
    // is not supported for image stores like it is when graphics pipelines write to
    // sRGB surfaces after blending:
    pixel = linear_to_gamma(pixel); // Rough version using a 2.2 exponent.
    // pixel = linear_to_gamma_precise(pixel); // A version with branches based on IEC 61966-2-1 spec.
    // Cheaper Gamma of 2:
    // pixel = linear_to_gamma_2_0(pixel);
    // Show that increasing Y coordinates are visually going down the image:
    // pixel.r *= gl_GlobalInvocationID.y / float(fp.fb_height); pixel.g *= 0.1; pixel.b *= 0.1;
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), vec4(pixel, 1.0f));
#endif
}
