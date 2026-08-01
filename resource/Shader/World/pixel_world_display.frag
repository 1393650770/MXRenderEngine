#version 460

// Pixel world display: fullscreen fragment reads the packed-cell SSBO and
// looks up the material palette. Nearest-neighbor texel fetch only - no
// linear filtering, no texture formats (avoids R8-family translation gaps).
// World size comes from the world_params SSBO (bound at init); push
// constants are NOT used here (they would need per-frame SetPushConstants).
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) readonly buffer PackedCells {
    uint cells[];
} world_cells;

layout(set = 0, binding = 1) readonly buffer Palette {
    vec4 colors[];
} world_palette;

layout(set = 0, binding = 2) readonly buffer WorldParams {
    uint world_w;
    uint world_h;
} world_params;

void main()
{
    // inUV: (0,0) is the top-left of the screen in Vulkan clip space (NDC
    // y goes down). World cell (0,0) is the bottom row, so flip Y:
    // screen top (inUV.y=0) -> world cell y = h-1 (top row).
    uint w = world_params.world_w;
    uint h = world_params.world_h;
    uint x = uint(inUV.x * float(w));
    uint y = h - 1u - uint(inUV.y * float(h));
    if (x >= w) x = w - 1u;
    if (y >= h) y = 0u;

    uint idx = y * w + x;
    uint mat = world_cells.cells[idx] & 0xFFu;
    outColor = world_palette.colors[mat];
}
