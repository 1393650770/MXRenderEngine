#version 460

// RmlUI vertex shader — per-draw transform via SSBO (MeshSample pattern)

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragTexCoord;

// Per-draw data via push constants (mat4 + vec2 = 72 bytes).
// Push constants are recorded per-draw in the command stream, so every
// geometry gets its OWN transform/translation even under deferred execution.
layout(push_constant) uniform PerDraw {
    mat4 transform;
    vec2 translation;
} pc;

void main()
{
    vec2 pos = inPosition + pc.translation;
    gl_Position = pc.transform * vec4(pos, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
