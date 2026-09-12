#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"

// Stage 1 pixel shader: material resolved by index, not by binding.
// The material id rides along from the vertex stage; materials[] is a plain
// storage-buffer array, so "switching material" is an array read.

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in flat uint v_material_id;

layout(location = 0) out vec4 out_color;

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
}
