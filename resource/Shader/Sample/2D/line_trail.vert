#version 460

// Line/trail vertex shader: per-vertex position + color, MVP from a readonly
// storage buffer (same pattern as sprite2d.vert).
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 outColor;

layout(std430, set = 0, binding = 0) readonly buffer Params {
    mat4 mvp;
} params;

void main()
{
    outColor = inColor;
    gl_Position = params.mvp * vec4(inPosition, 0.0, 1.0);
}
