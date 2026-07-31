#version 460

// Debug line fragment shader: pass through vertex color.
layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor, 1.0);
}
