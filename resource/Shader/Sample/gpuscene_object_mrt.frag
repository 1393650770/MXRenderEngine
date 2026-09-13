#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"

// Main-pass pixel shader with an extra depth output (MRT).
//
// The second target is what removes the depth prepass: instead of rendering the
// whole scene once just to produce depth, the main pass publishes it as it goes.
// That halves the geometry work, at the cost of the occlusion test consuming the
// PREVIOUS frame's pyramid — one frame of lag, which shows up as occasional
// over-draw rather than any visual error, because the MAX reduction is
// conservative in the safe direction.
//
// Same material logic as gpuscene_object.frag; only the depth output is added.

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in flat uint v_material_id;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_depth;   // R32F target: only .x is read

layout(std430, set = 0, binding = 0) readonly buffer SceneUniforms
{
	GPUSceneUniformsData u;
} g_scene;

layout(std430, set = 1, binding = 1) readonly buffer Materials
{
	GPUMaterialData materials[];
} g_materials;

void main()
{
	const GPUMaterialData mat = g_materials.materials[v_material_id];

	const vec3 n = normalize(v_normal);
	const vec3 l = normalize(g_scene.u.lightDir.xyz);
	const float ndl = max(dot(n, l), 0.0);
	const vec3 ambient = vec3(0.25);

	const vec3 color = mat.baseColorFactor.rgb * (ambient + ndl);
	out_color = vec4(color, 1.0);

	// Already in the engine's 0..1 convention, so it matches the depth attachment
	// exactly and needs no remapping.
	out_depth = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
