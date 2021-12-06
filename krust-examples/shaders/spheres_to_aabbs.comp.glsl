// GLSL Compute shader.
// Convert a buffer full of spheres into AABBoxes and write them out to a second buffer.
#version 450 core

struct AABB {
    float min_x;
    float min_y;
    float min_z;
    float max_x;
    float max_y;
    float max_z;
};

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in; // Todo: do I care or is this just getting in the way of warp scheduling?

/// Input buffer of spheres packed into vec4s:
layout(set = 0, binding = 0) restrict readonly buffer sphereBuffer {
    vec4 spheres[];
};

/// Output buffer of AABBoxes:
layout(set = 0, binding = 1) restrict writeonly buffer aabbBuffer {
    AABB aabbs[];
};

void main()
{
    const highp uint i = gl_GlobalInvocationID.x;
    vec4 sphere = spheres[i];
    vec3 centre = sphere.xyz;
    float radius = sphere.w;
    vec3 box_min = centre - radius;
    vec3 box_max = centre + radius;
    aabbs[i].min_x = box_min.x;
    aabbs[i].min_y = box_min.y;
    aabbs[i].min_z = box_min.z;
    aabbs[i].max_x = box_max.x;
    aabbs[i].max_y = box_max.y;
    aabbs[i].max_z = box_max.z;
}
