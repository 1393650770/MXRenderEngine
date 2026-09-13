#version 460

// Depth-only prepass fragment stage: writes gl_FragCoord.z into an R32F target.
//
// gl_FragCoord.z is already in the engine's depth convention (0..1, near = 0)
// because the projection matrix bakes GLM_FORCE_DEPTH_ZERO_TO_ONE, so it needs
// no remapping here and matches what ends up in the depth attachment.

layout(location = 0) out vec4 out_depth;

void main()
{
	out_depth = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
