#version 460

// 2D sprite vertex shader: position+uv per vertex, MVP uniform from Camera2D.
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 0) out vec2 outUV;

layout(set = 0, binding = 0) uniform Params {
    mat4 mvp;
} params;

void main()
{
    outUV = inUV;
    gl_Position = params.mvp * vec4(inPosition, 0.0, 1.0);
}
