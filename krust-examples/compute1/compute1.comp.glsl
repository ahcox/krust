// GLSL Compute shader.

#version 450 core

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 0) writeonly lowp uniform image2D framebuffer;

void main()
{
    float wg_shade = (gl_WorkGroupID.x*1.0)/gl_NumWorkGroups.x;
    vec4 pixel = vec4(
        (gl_GlobalInvocationID.x % 32u)/31.0,
        (gl_GlobalInvocationID.y % 32u)/31.0,
        wg_shade,
        1.0);
    imageStore(framebuffer, ivec2(gl_GlobalInvocationID.xy), pixel);
}
