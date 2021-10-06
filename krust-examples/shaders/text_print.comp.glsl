// GLSL Compute shader.
// Print a single line of text in an 8x16 font at any location on a char grid.
// We assume that we are executed on a domain that exactly covers the characters
// of the string and no more. CPU setup code ensures this is true.
#version 450 core
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#include "header.inc.glsl"

const uint CHAR_WIDTH  = 8u;
const uint CHAR_HEIGHT = 16u;

layout(local_size_x = 8, local_size_y = 16, local_size_z = 1) in;
layout(rgba8, set = 0, binding = 0) uniform restrict image2D framebuffer;

layout(push_constant) uniform frame_params_t
{
    uint8_t fb_char_x;
    uint8_t fb_char_y;
    uint8_t fg_bg_colours; // 3 bit palleted foreground and background colour stuffed into 2 nibbles.
    uint8_t str[125];
} fp;

// Fixed set of eight colours for the text:
const vec3 colours[] = {
    {0,0,0}, // 0: black
    {1,1,1}, // 1: white
    {1,0,0}, // 2: red
    {0,1,0}, // 3: green
    {0,0,1}, // 4: blue
    {1,1,0}, // 5: yellow
    {0,1,1}, // 6: cyan
    {1,0,1}  // 7: magenta
};

const uint8_t g_font[256][16] = {
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(129), uint8_t(129), uint8_t(165), uint8_t(165), uint8_t(129), uint8_t(129), uint8_t(129), uint8_t(189), uint8_t(153), uint8_t(129), uint8_t(129), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(255), uint8_t(255), uint8_t(219), uint8_t(219), uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(195), uint8_t(231), uint8_t(255), uint8_t(255), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(34), uint8_t(34), uint8_t(119), uint8_t(127), uint8_t(127), uint8_t(127), uint8_t(127), uint8_t(62), uint8_t(62), uint8_t(28), uint8_t(28), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(28), uint8_t(28), uint8_t(62), uint8_t(62), uint8_t(127), uint8_t(127), uint8_t(62), uint8_t(62), uint8_t(28), uint8_t(28), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(28), uint8_t(28), uint8_t(28), uint8_t(8), uint8_t(107), uint8_t(127), uint8_t(127), uint8_t(107), uint8_t(8), uint8_t(28), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(62), uint8_t(127), uint8_t(127), uint8_t(127), uint8_t(127), uint8_t(42), uint8_t(8), uint8_t(28), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(24), uint8_t(60), uint8_t(60), uint8_t(126), uint8_t(126), uint8_t(126), uint8_t(126), uint8_t(126), uint8_t(60), uint8_t(60), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(255), uint8_t(255), uint8_t(231), uint8_t(195), uint8_t(195), uint8_t(129), uint8_t(129), uint8_t(129), uint8_t(129), uint8_t(129), uint8_t(195), uint8_t(195), uint8_t(231), uint8_t(255), uint8_t(255), uint8_t(255)},
    {uint8_t(0), uint8_t(24), uint8_t(36), uint8_t(36), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(36), uint8_t(36), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(255), uint8_t(255), uint8_t(231), uint8_t(219), uint8_t(219), uint8_t(189), uint8_t(189), uint8_t(189), uint8_t(189), uint8_t(189), uint8_t(219), uint8_t(219), uint8_t(231), uint8_t(255), uint8_t(255), uint8_t(255)},
    {uint8_t(120), uint8_t(96), uint8_t(80), uint8_t(80), uint8_t(8), uint8_t(28), uint8_t(34), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(28), uint8_t(34), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(28), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(24), uint8_t(40), uint8_t(40), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(8), uint8_t(8), uint8_t(14), uint8_t(15), uint8_t(15), uint8_t(6), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(252), uint8_t(132), uint8_t(252), uint8_t(132), uint8_t(132), uint8_t(132), uint8_t(132), uint8_t(132), uint8_t(132), uint8_t(196), uint8_t(230), uint8_t(231), uint8_t(71), uint8_t(2), uint8_t(0), uint8_t(0)},
    {uint8_t(146), uint8_t(146), uint8_t(84), uint8_t(108), uint8_t(40), uint8_t(68), uint8_t(199), uint8_t(68), uint8_t(40), uint8_t(108), uint8_t(84), uint8_t(146), uint8_t(146), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(2), uint8_t(6), uint8_t(14), uint8_t(30), uint8_t(62), uint8_t(126), uint8_t(62), uint8_t(30), uint8_t(14), uint8_t(6), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(64), uint8_t(96), uint8_t(112), uint8_t(120), uint8_t(124), uint8_t(126), uint8_t(124), uint8_t(120), uint8_t(112), uint8_t(96), uint8_t(64), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(28), uint8_t(42), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(42), uint8_t(28), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(0), uint8_t(0), uint8_t(34), uint8_t(34), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(78), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(2), uint8_t(4), uint8_t(26), uint8_t(34), uint8_t(68), uint8_t(88), uint8_t(32), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(254), uint8_t(254), uint8_t(254), uint8_t(254), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(28), uint8_t(42), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(42), uint8_t(28), uint8_t(8), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(28), uint8_t(42), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(42), uint8_t(28), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(16), uint8_t(32), uint8_t(127), uint8_t(32), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(4), uint8_t(2), uint8_t(127), uint8_t(2), uint8_t(4), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(36), uint8_t(66), uint8_t(255), uint8_t(66), uint8_t(36), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(28), uint8_t(62), uint8_t(62), uint8_t(127), uint8_t(127), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(127), uint8_t(127), uint8_t(62), uint8_t(62), uint8_t(28), uint8_t(28), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(126), uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(126), uint8_t(36), uint8_t(36), uint8_t(36), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(60), uint8_t(106), uint8_t(10), uint8_t(10), uint8_t(10), uint8_t(28), uint8_t(40), uint8_t(40), uint8_t(40), uint8_t(43), uint8_t(30), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(76), uint8_t(74), uint8_t(42), uint8_t(38), uint8_t(16), uint8_t(16), uint8_t(8), uint8_t(8), uint8_t(100), uint8_t(84), uint8_t(82), uint8_t(50), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(14), uint8_t(17), uint8_t(17), uint8_t(17), uint8_t(10), uint8_t(12), uint8_t(18), uint8_t(81), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(94), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(16), uint8_t(8), uint8_t(8), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(8), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(16), uint8_t(16), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(16), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(8), uint8_t(73), uint8_t(42), uint8_t(28), uint8_t(28), uint8_t(127), uint8_t(28), uint8_t(28), uint8_t(42), uint8_t(73), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(16), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(128), uint8_t(128), uint8_t(64), uint8_t(32), uint8_t(32), uint8_t(16), uint8_t(16), uint8_t(8), uint8_t(8), uint8_t(4), uint8_t(2), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(36), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(36), uint8_t(36), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(12), uint8_t(10), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(64), uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(4), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(56), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(48), uint8_t(48), uint8_t(40), uint8_t(40), uint8_t(36), uint8_t(36), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(126), uint8_t(32), uint8_t(32), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(62), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(62), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(124), uint8_t(64), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(48), uint8_t(48), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(48), uint8_t(48), uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(64), uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(2), uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(32), uint8_t(64), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(2), uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(32), uint8_t(64), uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(64), uint8_t(32), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(16), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(34), uint8_t(65), uint8_t(73), uint8_t(85), uint8_t(85), uint8_t(85), uint8_t(85), uint8_t(85), uint8_t(117), uint8_t(105), uint8_t(2), uint8_t(124), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(62), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(60), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(56), uint8_t(68), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(68), uint8_t(56), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(30), uint8_t(36), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(68), uint8_t(36), uint8_t(30), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(62), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(62), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(56), uint8_t(68), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(114), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(68), uint8_t(56), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(28), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(112), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(10), uint8_t(10), uint8_t(22), uint8_t(18), uint8_t(34), uint8_t(34), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(99), uint8_t(99), uint8_t(85), uint8_t(85), uint8_t(73), uint8_t(73), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(70), uint8_t(70), uint8_t(74), uint8_t(74), uint8_t(82), uint8_t(82), uint8_t(98), uint8_t(98), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(30), uint8_t(34), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(34), uint8_t(30), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(74), uint8_t(82), uint8_t(34), uint8_t(34), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(62), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(62), uint8_t(18), uint8_t(18), uint8_t(34), uint8_t(34), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(60), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(127), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(20), uint8_t(28), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(85), uint8_t(85), uint8_t(85), uint8_t(34), uint8_t(34), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(54), uint8_t(20), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(20), uint8_t(54), uint8_t(34), uint8_t(65), uint8_t(65), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(34), uint8_t(20), uint8_t(20), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(126), uint8_t(64), uint8_t(64), uint8_t(32), uint8_t(32), uint8_t(16), uint8_t(24), uint8_t(8), uint8_t(4), uint8_t(4), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(28), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(2), uint8_t(2), uint8_t(4), uint8_t(8), uint8_t(8), uint8_t(16), uint8_t(16), uint8_t(32), uint8_t(32), uint8_t(64), uint8_t(128), uint8_t(128), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(28), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(20), uint8_t(34), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(58), uint8_t(70), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(70), uint8_t(58), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(92), uint8_t(98), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(48), uint8_t(72), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(92), uint8_t(98), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(64), uint8_t(64), uint8_t(60), uint8_t(0)},
    {uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(58), uint8_t(70), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(12), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(32), uint8_t(0), uint8_t(0), uint8_t(48), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(32), uint8_t(36), uint8_t(24), uint8_t(0)},
    {uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(34), uint8_t(18), uint8_t(10), uint8_t(6), uint8_t(10), uint8_t(18), uint8_t(34), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(12), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(37), uint8_t(91), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(58), uint8_t(70), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(58), uint8_t(70), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(70), uint8_t(58), uint8_t(2), uint8_t(2), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(92), uint8_t(98), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(64), uint8_t(64), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(26), uint8_t(38), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(60), uint8_t(64), uint8_t(64), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(72), uint8_t(48), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(34), uint8_t(20), uint8_t(20), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(54), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(102), uint8_t(36), uint8_t(24), uint8_t(36), uint8_t(102), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(64), uint8_t(64), uint8_t(60), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(126), uint8_t(64), uint8_t(32), uint8_t(32), uint8_t(24), uint8_t(4), uint8_t(4), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(16), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(4), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(32), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(76), uint8_t(50), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(16), uint8_t(40), uint8_t(68), uint8_t(130), uint8_t(130), uint8_t(130), uint8_t(254), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(73), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(73), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(4), uint8_t(4), uint8_t(4), uint8_t(31), uint8_t(4), uint8_t(4), uint8_t(8), uint8_t(14), uint8_t(73), uint8_t(73), uint8_t(54), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(65), uint8_t(65), uint8_t(34), uint8_t(34), uint8_t(20), uint8_t(127), uint8_t(8), uint8_t(8), uint8_t(127), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(2), uint8_t(4), uint8_t(26), uint8_t(34), uint8_t(68), uint8_t(88), uint8_t(32), uint8_t(64), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(56), uint8_t(36), uint8_t(36), uint8_t(88), uint8_t(0), uint8_t(124), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(72), uint8_t(36), uint8_t(18), uint8_t(36), uint8_t(72), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(126), uint8_t(64), uint8_t(64), uint8_t(64), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(48), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(48), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(62), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(62), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(32), uint8_t(24), uint8_t(4), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(33), uint8_t(35), uint8_t(93), uint8_t(1), uint8_t(1), uint8_t(0)},
    {uint8_t(126), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(73), uint8_t(78), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(72), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(48), uint8_t(72), uint8_t(72), uint8_t(48), uint8_t(0), uint8_t(120), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(18), uint8_t(36), uint8_t(72), uint8_t(36), uint8_t(18), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(6), uint8_t(4), uint8_t(4), uint8_t(68), uint8_t(46), uint8_t(16), uint8_t(8), uint8_t(36), uint8_t(50), uint8_t(41), uint8_t(124), uint8_t(32), uint8_t(32), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(6), uint8_t(4), uint8_t(4), uint8_t(68), uint8_t(46), uint8_t(16), uint8_t(8), uint8_t(52), uint8_t(74), uint8_t(33), uint8_t(16), uint8_t(8), uint8_t(120), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(8), uint8_t(0), uint8_t(0), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(4), uint8_t(2), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(124), uint8_t(10), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(127), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(9), uint8_t(121), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(56), uint8_t(68), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(68), uint8_t(56), uint8_t(16), uint8_t(28), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(16), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(62), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(126), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(76), uint8_t(50), uint8_t(0), uint8_t(66), uint8_t(70), uint8_t(70), uint8_t(74), uint8_t(74), uint8_t(82), uint8_t(82), uint8_t(98), uint8_t(98), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(34), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(50), uint8_t(2), uint8_t(2), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(20), uint8_t(34), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(34), uint8_t(34), uint8_t(0), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(24), uint8_t(0), uint8_t(28), uint8_t(34), uint8_t(32), uint8_t(60), uint8_t(34), uint8_t(34), uint8_t(34), uint8_t(18), uint8_t(108), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(54), uint8_t(73), uint8_t(72), uint8_t(78), uint8_t(121), uint8_t(9), uint8_t(9), uint8_t(73), uint8_t(54), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(16), uint8_t(28), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(2), uint8_t(2), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(24), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(56), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(16), uint8_t(8), uint8_t(4), uint8_t(0), uint8_t(12), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(8), uint8_t(20), uint8_t(34), uint8_t(0), uint8_t(12), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(8), uint8_t(28), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(16), uint8_t(56), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(76), uint8_t(50), uint8_t(0), uint8_t(0), uint8_t(58), uint8_t(70), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(60), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(60), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(24), uint8_t(24), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(4), uint8_t(8), uint8_t(16), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(32), uint8_t(16), uint8_t(8), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(24), uint8_t(36), uint8_t(66), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(0), uint8_t(126), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(126), uint8_t(0), uint8_t(0), uint8_t(0), uint8_t(0)},
    {uint8_t(66), uint8_t(66), uint8_t(0), uint8_t(0), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(66), uint8_t(98), uint8_t(92), uint8_t(64), uint8_t(64), uint8_t(60), uint8_t(0)}
};

void main()
{
    // Top-left relative integer screen coordinates:
    // The shader is executed over an 8 * k * 16 domain that encompasses the string, not the whole framebuffer.
    const float screen_coord_x = CHAR_WIDTH  * fp.fb_char_x + gl_GlobalInvocationID.x ;
    const float screen_coord_y = CHAR_HEIGHT * fp.fb_char_y + gl_GlobalInvocationID.y ;
    const ivec2 screen_coord = ivec2(screen_coord_x, screen_coord_y);

    const uint str_char    = gl_GlobalInvocationID.x / CHAR_WIDTH;
    const uint char_column = gl_GlobalInvocationID.x % CHAR_WIDTH;
    const uint char_row    = gl_GlobalInvocationID.y % CHAR_HEIGHT;

    const uint fg = (g_font[uint(fp.str[str_char])][char_row] >> char_column) & 1u;

    switch(uint(fp.fg_bg_colours & uint8_t(3)))
    {
      // No background, not transparent:
      case 0u:
      {
        if (fg == 1u){
          vec4 pixel = vec4(colours[uint(fp.fg_bg_colours) >> 5u], 1.0f);
          imageStore(framebuffer, screen_coord, pixel);
        }
        break;
      }
      // Background, not transparent:
      case 1u:
      {
        if (fg == 1u){
          vec4 pixel = vec4(colours[uint(fp.fg_bg_colours >> 5u)], 1.0f);
          imageStore(framebuffer, screen_coord, pixel);
        } else {
          vec4 pixel = vec4(colours[uint((fp.fg_bg_colours >> 2u) & uint8_t(7))], 1.0f);
          imageStore(framebuffer, screen_coord, pixel);
        }
        break;
      }
      // No background, semi-transparent:
      case 2u:
      {
        if (fg == 1u){
          // Init pixel to background colour:
          vec4 pixel = imageLoad(framebuffer, screen_coord);
          pixel.rgb = (pixel.rgb + colours[uint(fp.fg_bg_colours) >> 5u] * 3.0f) * 0.25f;
          pixel.a = 1.0f;
          imageStore(framebuffer, screen_coord, pixel);
        }
        break;
      }
      // Background, semi-transparent:
      case 3u:
      {
        vec4 pixel = imageLoad(framebuffer, screen_coord);
        if (fg == 1u){
          pixel.rgb = (pixel.rgb + colours[uint(fp.fg_bg_colours >> 5u)] * 3.0f) * 0.25f;
        } else {
          pixel.rgb = (pixel.rgb + colours[uint((fp.fg_bg_colours >> 2u) & uint8_t(7))]) * 0.5f;
        }
        pixel.a = 1.0f;
        imageStore(framebuffer, screen_coord, pixel);
        break;
      }
    }
}

