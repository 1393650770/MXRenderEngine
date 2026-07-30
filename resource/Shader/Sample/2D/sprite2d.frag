#version 460

// 2D sprite fragment shader: minimal solid-color output for the simple-demo path.
// (no texture binding required — just outputs white, modulated via blend or
// per-quad vertex color when added later.)
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
