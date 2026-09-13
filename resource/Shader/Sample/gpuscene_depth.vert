#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"

// Depth-only prepass vertex stage.
//
// The same index chain as the main pass, but the only thing that leaves this
// stage is clip position — no varyings, no material. Its purpose is to publish
// a depth buffer that the occlusion test can sample. The engine's own depth
// attachment is created with DEPTH_ATTACHMENT usage only (no SAMPLED bit) and
// the RHI cannot bind a per-mip view, so the depth cannot be read back
// directly; writing it to an R32F colour target is the way around that.

layout(location = 0) in vec3 in_pos;

layout(std430, set = 0, binding = 0) readonly buffer SceneUniforms
{
	GPUSceneUniformsData u;
} g_scene;

layout(std430, set = 1, binding = 0) readonly buffer Objects    { GPUObjectData objects[]; } g_objects;
layout(std430, set = 1, binding = 3) readonly buffer VisibleIDs { uint          ids[];     } g_visible;

layout(push_constant) uniform PushConstants
{
	uint batchBaseInstance;
} pc;

void main()
{
	const uint objectID = g_visible.ids[pc.batchBaseInstance + gl_InstanceIndex];
	gl_Position = g_scene.u.viewProj * g_objects.objects[objectID].model * vec4(in_pos, 1.0);
}
