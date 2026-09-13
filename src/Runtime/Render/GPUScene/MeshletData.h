#pragma once
#ifndef _MESHLET_DATA_
#define _MESHLET_DATA_

// Meshlet (cluster) data contract — the fine-grained alternative to per-object
// culling.
//
// Why clusters: culling at object granularity means one large mesh is
// all-or-nothing. Splitting geometry into ~64-vertex clusters lets the cull
// reject the parts of a mesh that are off-screen or hidden, which is where the
// real win is for objects that are usually partially visible.
//
// This is the COMPUTE path, not mesh shaders. The engine has no
// Shader_Mesh / Shader_Task stage (see ENUM_SHADER_STAGE), so clusters are
// drawn by pulling their vertices from a storage buffer inside a normal vertex
// shader, with gl_InstanceIndex carrying the cluster id.
//
// Layout mirrors resource/Shader/Global/Meshlet.glsl — keep both in sync.

#include "Render/GPUScene/GPUSceneData.h"
#include "Tool/MeshLoader.h"   // Tool::MeshVertex, needed by MeshletBuilder

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

// ---- Debug views ------------------------------------------------------------
// GPU-driven pipelines are near-impossible to debug from the outside: the CPU
// never learns what was rejected, so a missing cluster is indistinguishable from
// a broken transform. These modes make the decision visible.
//
// The key trick: when a debug mode is active the culler emits geometry for EVERY
// cluster and records what it WOULD have done. Without that you can only inspect
// the survivors — exactly the half that needs no explanation.
//
// Selected through `lodParams.z` in the uniform block (the slot was free, so no
// struct had to grow).
enum class ENUM_CLUSTER_DEBUG : UInt32
{
	Off = 0,
	ClusterId = 1,       // hash per cluster — shows how the mesh was partitioned
	CullState = 2,       // why each cluster survived or was rejected
	LodLevel = 3,        // which level of detail was chosen
	MaterialId = 4,      // hash per material — shows batching
	TriangleDensity = 5, // heat map of triangles per cluster
};

// Per-pair culling decision, written by the cull shader and read back by the
// debug pixel shader. Bitwise, so a view can show several at once.
constexpr UInt32 kClusterCullFrustum = 1u << 0;   // passed the frustum test
constexpr UInt32 kClusterCullDistance = 1u << 1;  // inside the draw distance
constexpr UInt32 kClusterCullOcclusion = 1u << 2; // NOT occluded
constexpr UInt32 kClusterCullDrawn = 1u << 3;     // actually submitted

// Cluster size limits. 64 vertices is the classic value: it keeps the cluster's
// vertex list inside a single cache line group and lets the local vertex index
// be expressed in 8 bits if a future packing wants that.
constexpr UInt32 kMeshletMaxVertices = 64;
constexpr UInt32 kMeshletMaxTriangles = 124;   // 3*124 = 372 < 64*8, the usual 8-bit index bound

// ---- One cluster (32 bytes) -------------------------------------------------
// Mirrors `struct GPUCluster` in Meshlet.glsl.
struct GPUCluster
{
	glm::vec4 bounds{ 0.0f, 0.0f, 0.0f, 1.0f };   // 16  xyz = center (model space), w = radius
	UInt32 vertex_ref_offset = 0;                 // 4   into cluster_vertex_refs[]
	UInt32 triangle_offset = 0;                   // 4   into cluster_triangles[], in index units
	UInt32 vertex_count = 0;                      // 4
	UInt32 triangle_count = 0;                    // 4
};

// ---- Per-cluster draw (32 bytes) --------------------------------------------
// Mirrors `struct GPUClusterData` in Meshlet.glsl. `mesh_id` and `lod_level` are
// kept beside the bounds so the culling shader can resolve an object's model
// matrix without a second lookup chain.
struct GPUClusterDraw
{
	UInt32 mesh_id = kInvalidIndex;      // 4   which LOD-group head this cluster belongs to
	UInt32 lod_level = 0;                // 4   which level of that group
	UInt32 vertex_count = 0;             // 4   replicated from GPUCluster for the shader
	UInt32 triangle_count = 0;           // 4
};

static_assert(sizeof(GPUCluster) == 32, "GPUCluster must be 32 bytes (std430)");
static_assert(sizeof(GPUClusterDraw) == 16, "GPUClusterDraw must be 16 bytes (std430)");

// ---- CPU-side cluster build -------------------------------------------------
// Splits a mesh's index list into clusters. This is a straightforward sequential
// partition, not a spatial optimiser: it walks triangles in order and closes a
// cluster when adding the next triangle would exceed either limit.
//
// A quality builder would cluster by spatial proximity (and add a normal cone
// for backface culling). Sequential partitioning already produces clusters whose
// bounds are far tighter than the whole mesh, which is what the culler needs —
// so it is a real win, just not the best possible one.
class MeshletBuilder
{
public:
	struct Result
	{
		Vector<GPUCluster> clusters;
		Vector<GPUClusterDraw> cluster_draws;
		Vector<UInt32> vertex_refs;    // global vertex index, per cluster vertex
		Vector<UInt32> triangles;      // cluster-local vertex index, 3 per triangle
	};

	// in_indices are indices into in_vertices (already rebased by MeshPool).
	static void Build(
		CONST Vector<Tool::MeshVertex>& in_vertices,
		CONST Vector<UInt32>& in_indices,
		UInt32 in_mesh_id,
		UInt32 in_lod_level,
		Result& out_result,
		UInt32 in_max_vertices = kMeshletMaxVertices,
		UInt32 in_max_triangles = kMeshletMaxTriangles);
};

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _MESHLET_DATA_
