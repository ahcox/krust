#ifndef KRUST_INTERSECTIONS_INC_INCLUDED
#define KRUST_INTERSECTIONS_INC_INCLUDED
// ToDos:
// > Sphere intersect which doesn't handle origin inside sphere case.
// > Sphere intersect which yields a bool only.
#include "utils.inc.glsl"

const float MISS = 16777216.0f * 4294967296.0f;
const float EPSILON = 1.0 / 1048576.0;

/// Return first hit of ray with sphere surface.
float isect_sphere1(const vec3 ray_origin, const vec3 ray_dir_unit, const float t_min, const vec3 sphere_centre, const float sphere_radius)
{
    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    float t = MISS;
    const float a = dot(ray_dir_unit, ray_dir_unit);
    const float b = 2.0f * dot(sphere_to_ray, ray_dir_unit);
    const float c = dot(sphere_to_ray, sphere_to_ray) - sphere_radius * sphere_radius;
    const float discriminator = b * b - 4.0f * a * c;
    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float denominator = 2.0f * a;
        const float root1 = (-b-e) / denominator;
        if(root1 > t_min){
            t = root1;
        } else {
            // If the ray origin is inside the sphere, our first intersection with the
            // surface is when it punches out the backside:
            // (Note this may shade surprisingly)
            const float root2 = (-b+e) / denominator;
            if(root2 > t_min){
                t = root2;
            }
        }
    }
    return t;
}

/// Return first hit of ray with sphere surface.
float isect_sphere2(const vec3 ray_origin, const vec3 ray_dir_unit, const float t_min, const vec3 sphere_centre, const float sphere_radius)
{
    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    float t = MISS;
    const float a = dot(ray_dir_unit, ray_dir_unit);
    const float b = 2.0f * dot(sphere_to_ray, ray_dir_unit);
    const float c = dot(sphere_to_ray, sphere_to_ray) -
                    sphere_radius * sphere_radius;
    const float discriminator = b * b - 4.0f * a * c;
    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float inv_denominator = 1.0f / (2.0f * a);
        const float root1 = (-b-e) * inv_denominator;
        const float root2 = (-b+e) * inv_denominator;
        if(root1 > t_min){
            t = root1;
        } else {
            // If the ray origin is inside the sphere, our first intersection with the
            // surface is when it punches out the backside:
            // (Note this may shade surprisingly)
            if(root2 > t_min){
                t = root2;
            }
        }
    }
    return t;
}

/// Return first hit of ray with sphere surface if outside the sphere.
///
/// @note Misses back faces when the ray starts inside the sphere:
/// Followed Listing 13 in RTIOW, Section 6.2 to generate this:
float isect_sphere(const vec3 ray_origin, const vec3 ray_dir, const float t_min, const vec3 sphere_centre, const float sphere_radius)
{
    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    float t = MISS;
    const float a = length_squared(ray_dir); /// @todo hoist 1/a here.
    const float half_b = dot(sphere_to_ray, ray_dir);
    const float c = length_squared(sphere_to_ray) - sphere_radius * sphere_radius;
    const float discriminator = half_b * half_b - a * c;

    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float inv_denominator = 1.0f / a;       /// @todo Try and hoist this out of branch to get it going ASAP. The basic ALU maths might execute in its shadow. Test in a microbenchmark.
        const float root1 = (-half_b-e) * inv_denominator;
        t = root1;
    }
    return t;
}

/// Return first hit of ray with sphere surface if outside the sphere.
bool sphere_hit_outside(
    in const vec3 ray_origin,
    in const vec3 ray_dir_unit, // We rely on this being unit length to save work, including a divide, per sphere.
    in const vec4 sphere,
    out float t)
{
    const vec3 sphere_centre = sphere.xyz;
    const float sphere_radius = sphere.w;

    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    const float half_b = dot(sphere_to_ray, ray_dir_unit);
    const float c = length_squared(sphere_to_ray) - sphere_radius * sphere_radius;
    const float discriminator = half_b * half_b - c;

    bool hit = false;
    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float root1 = (-half_b-e);
        t = root1;
        hit = true;
    }
    return hit;
}

/// Return first hit with sphere surface.
bool sphere_hit(
    in const vec3 ray_origin,
    in const vec3 ray_dir_unit, // We rely on this being unit length to save work, including a divide, per sphere.
    in const vec4 sphere,
    out float t)
{
    const vec3 sphere_centre = sphere.xyz;
    const float sphere_radius = sphere.w;

    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    const float half_b = dot(sphere_to_ray, ray_dir_unit);
    const float c = length_squared(sphere_to_ray) - sphere_radius * sphere_radius;
    const float discriminator = half_b * half_b - c;

    bool hit = false;
    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float root1 = (-half_b-e);
        const float root2 = (-half_b+e);
        t = root1;
        if(root1 <= 0.0f){
            t = root2;
        }
        hit = true;
    }
    return hit;
}

/// Output both hits of a sphere:
bool sphere_hits(
    in const vec3 ray_origin,
    in const vec3 ray_dir_unit, // We rely on this being unit length to save work, including a divide, per sphere.
    in const vec4 sphere,
    out float t1,
    out float t2)
{
    const vec3 sphere_centre = sphere.xyz;
    const float sphere_radius = sphere.w;

    const vec3 sphere_to_ray = ray_origin - sphere_centre;
    const float half_b = dot(sphere_to_ray, ray_dir_unit);
    const float c = length_squared(sphere_to_ray) - sphere_radius * sphere_radius;
    const float discriminator = half_b * half_b - c;

    bool hit = false;
    if(discriminator >= 0.0f){
        const float e = sqrt(discriminator);
        const float root1 = (-half_b-e);
        const float root2 = (-half_b+e);
        t1 = root1;
        t2 = root2;
        hit = true;
    }
    return hit;
}

#endif // KRUST_INTERSECTIONS_INC_INCLUDED