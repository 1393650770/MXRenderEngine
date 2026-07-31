#version 460

// Debug line vertex shader: world-space line vertices, MVP uniform.
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 outColor;

layout(set = 0, binding = 0) uniform Params {
    mat4 mvp;
} params;

void main()
{
    outColor = inColor;
    gl_Position = params.mvp * vec4(inPosition, 0.0, 1.0);
}
