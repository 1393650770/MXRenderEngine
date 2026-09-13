#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"
#include "../Global/Meshlet.glsl"

// Meshlet pixel shader: normal shading plus the cluster debug views, and the
// depth output that feeds next frame's Hi-Z pyramid.
//
// The debug branch is a uniform test on lodParams.z, so it costs one compare in
// the normal path — cheap enough that a separate pipeline per view would be
// needless complication.
//
// All four debug fields arrive as FLAT varyings: they are per-cluster constants,
// not per-pixel values, so interpolating them would be meaningless.

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in flat uint v_material_id;
layout(location = 3) in flat uint v_cluster_id;
layout(location = 4) in flat uint v_cull_state;
layout(location = 5) in flat uint v_tri_count;
layout(location = 6) in flat uint v_lod_level;

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
	vec3 color = mat.baseColorFactor.rgb * (vec3(0.25) + ndl);

	// ---- debug views --------------------------------------------------------
	const uint mode = uint(g_scene.u.lodParams.z + 0.5);

	if (mode == GPU_CLUSTER_DEBUG_CLUSTER_ID)
	{
		// Distinct colour per cluster: an unbalanced or fragmented partition is
		// immediately obvious, and a cluster that spans two objects is visible as
		// a colour patch crossing a seam.
		color = ClusterDebugHashColor(v_cluster_id);
	}
	else if (mode == GPU_CLUSTER_DEBUG_CULL_STATE)
	{
		// Green = drawn, red = outside the frustum, orange = past the draw
		// distance, blue = occluded. The culler emits every cluster while any
		// debug view is on, so the rejected ones are actually on screen to look at.
		color = ClusterDebugCullColor(v_cull_state);
	}
	else if (mode == GPU_CLUSTER_DEBUG_LOD_LEVEL)
	{
		color = ClusterDebugLodColor(v_lod_level);
	}
	else if (mode == GPU_CLUSTER_DEBUG_MATERIAL)
	{
		color = ClusterDebugHashColor(v_material_id);
	}
	else if (mode == GPU_CLUSTER_DEBUG_TRI_DENSITY)
	{
		color = ClusterDebugDensityColor(v_tri_count);
	}
	else if (mode != GPU_CLUSTER_DEBUG_OFF)
	{
		// Unknown mode: magenta, deliberately unmissable.
		color = vec3(1.0, 0.0, 1.0);
	}

	out_color = vec4(color, 1.0);

	// Always the real depth, even in debug views — otherwise the pyramid the next
	// frame culls against would be built from debug colours.
	out_depth = vec4(gl_FragCoord.z, 0.0, 0.0, 1.0);
}
