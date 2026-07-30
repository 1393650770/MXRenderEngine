#version 460

// 2D sprite fragment shader: samples texture atlas, modulated by vertex color.
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstants {
    layout(offset = 0) vec4 color;
} pc;

void main()
{
    outColor = texture(texSampler, inUV) * pc.color;
}
