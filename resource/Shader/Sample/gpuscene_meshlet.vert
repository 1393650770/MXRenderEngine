#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"
#include "../Global/Meshlet.glsl"

// Cluster rendering by vertex pulling.
//
// There is no mesh shader stage in this engine (see ENUM_SHADER_STAGE), so the
// cluster is unpacked here instead: gl_InstanceIndex names the (cluster, object)
// pair, gl_VertexIndex walks that cluster's triangle list, and the actual vertex
// attributes are read out of a storage buffer. No vertex input layout is
// declared — nothing comes through the input assembler.
//
// Depends on shaderDrawParameters: gl_InstanceIndex only carries firstInstance
// when that device feature is enabled, and firstInstance is what identifies the
// pair. Without it every cluster would resolve to pair 0.

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out flat uint v_material_id;
// Debug views. Flat because they are per-cluster constants, not per-pixel.
layout(location = 3) out flat uint v_cluster_id;
layout(location = 4) out flat uint v_cull_state;
layout(location = 5) out flat uint v_tri_count;
layout(location = 6) out flat uint v_lod_level;

layout(std430, set = 0, binding = 0) readonly buffer SceneUniforms { GPUSceneUniformsData u; } g_scene;

layout(std430, set = 1, binding = 0) readonly buffer Objects { GPUObjectData objects[]; } g_objects;
layout(std430, set = 1, binding = MESHLET_BIND_CLUSTERS) readonly buffer Clusters
{
	GPUCluster clusters[];
} g_clusters;
layout(std430, set = 1, binding = MESHLET_BIND_VERTEX_REFS) readonly buffer VertexRefs
{
	uint refs[];              // cluster-local slot -> pooled global vertex index
} g_cluster_refs;
layout(std430, set = 1, binding = MESHLET_BIND_TRIANGLES) readonly buffer ClusterTriangles
{
	uint tris[];              // cluster-local vertex index, 3 per triangle
} g_cluster_tris;
layout(std430, set = 1, binding = MESHLET_BIND_PAIRS) readonly buffer ClusterPairs
{
	GPUClusterInstance items[];
} g_cluster_instances;
layout(std430, set = 1, binding = MESHLET_BIND_CLUSTER_DRAWS) readonly buffer ClusterDraws
{
	GPUClusterDraw draws[];
} g_cluster_draws;
// The culler's decision for this pair. Always written, so the debug views need
// no extra pass — this stage just forwards it.
layout(std430, set = 1, binding = MESHLET_BIND_DEBUG) readonly buffer ClusterDebug
{
	uint state[];
} g_cluster_debug;
layout(std430, set = 1, binding = MESHLET_BIND_VERTICES) readonly buffer VertexData
{
	// MeshVertex is 32 bytes: position(3) + normal(3) + uv(2). Read as a flat
	// float array so the stride matches the interleaved vertex buffer exactly —
	// a struct with vec3 members would be padded to 40 bytes under std430.
	float v[];
} g_vertex_data;

void main()
{
	const uint pair = gl_InstanceIndex;
	const GPUClusterInstance item = g_cluster_instances.items[pair];
	const GPUCluster cluster = g_clusters.clusters[item.clusterID];
	const GPUObjectData obj = g_objects.objects[item.objectID];

	// gl_VertexIndex already includes the command's firstIndex, i.e. this
	// cluster's triangle offset into the shared triangle list.
	const uint local_vertex = g_cluster_tris.tris[gl_VertexIndex];
	const uint global_vertex = g_cluster_refs.refs[cluster.vertexRefOffset + local_vertex];

	const uint base = global_vertex * 8u;
	const vec3 position = vec3(
		g_vertex_data.v[base + 0u], g_vertex_data.v[base + 1u], g_vertex_data.v[base + 2u]);
	const vec3 normal = vec3(
		g_vertex_data.v[base + 3u], g_vertex_data.v[base + 4u], g_vertex_data.v[base + 5u]);
	const vec2 uv = vec2(
		g_vertex_data.v[base + 6u], g_vertex_data.v[base + 7u]);

	gl_Position = g_scene.u.viewProj * obj.model * vec4(position, 1.0);
	v_normal = mat3(obj.model) * normal;
	v_uv = uv;
	v_material_id = obj.materialID;

	v_cluster_id = item.clusterID;
	v_cull_state = g_cluster_debug.state[pair];
	v_tri_count = cluster.triangleCount;
	v_lod_level = g_cluster_draws.draws[item.clusterID].lodLevel;
}
