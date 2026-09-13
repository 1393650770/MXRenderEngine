#pragma once
#ifndef _MESHLET_SCENE_
#define _MESHLET_SCENE_

#include "Core/ConstDefine.h"
#include "Render/GPUScene/MeshletData.h"
#include "RHI/RenderCommandList.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

class MeshPool;
class GPUSceneManager;

// One (cluster, object) pair — the unit the cluster culler actually tests.
//
// A cluster belongs to a mesh; a mesh is instanced by many objects. Culling at
// cluster granularity therefore has to be done per PAIR, not per cluster: the
// same cluster is visible for one instance and hidden for another, because the
// model matrix differs. This stream is the cluster-level analogue of
// GPUInstanceData.
struct GPUClusterInstance
{
	UInt32 cluster_id = kInvalidIndex;   // 4
	UInt32 object_id = kInvalidIndex;    // 4
};
static_assert(sizeof(GPUClusterInstance) == 8, "GPUClusterInstance must be 8 bytes (std430)");

// Owns the buffers and the bookkeeping for the meshlet (cluster) path.
//
// Deliberately separate from GPUSceneManager: cluster data is a superset of
// scene data, and mixing it in would make every ordinary scene pay for geometry
// it never registered. It reads the scene's instance stream and produces its own
// buffers.
class MeshletScene
{
public:
	MeshletScene() = default;
	~MeshletScene() = default;

	// Builds clusters for every LOD group in the pool, then expands them into
	// (cluster, object) pairs using the scene's instance stream.
	//
	// Call after GPUSceneManager::BuildBatches — the pair stream is derived from
	// the instances, so adding or removing objects means calling this again.
	Bool Build(const MeshPool& pool, const GPUSceneManager& scene,
		UInt32 max_vertices = kMeshletMaxVertices,
		UInt32 max_triangles = kMeshletMaxTriangles);

	void Shutdown();

	// Uploads every CPU mirror. Commands are uploaded with instanceCount cleared,
	// so the culler accumulates from zero.
	void UploadAll();
	void ResetClusterCommands();

	const MeshletBuilder::Result& GetBuildResult() const { return build_result; }
	UInt32 GetClusterCount() const { return static_cast<UInt32>(build_result.clusters.size()); }
	UInt32 GetClusterInstanceCount() const { return static_cast<UInt32>(cluster_instances.size()); }
	UInt32 GetCommandCount() const { return static_cast<UInt32>(cluster_commands.size()); }

	RHI::Buffer* GetClusterBuffer() const { return cluster_buffer; }
	RHI::Buffer* GetClusterDrawBuffer() const { return cluster_draw_buffer; }
	RHI::Buffer* GetVertexRefBuffer() const { return vertex_ref_buffer; }
	RHI::Buffer* GetTriangleBuffer() const { return triangle_buffer; }
	RHI::Buffer* GetClusterInstanceBuffer() const { return cluster_instance_buffer; }
	RHI::Buffer* GetCommandBuffer() const { return command_buffer; }
	// One culling-decision word per pair, written by the cull shader. Only read
	// by the debug views, but always written, so the views need no extra pass.
	RHI::Buffer* GetClusterDebugBuffer() const { return cluster_debug_buffer; }

	const Vector<GPUClusterInstance>& GetClusterInstances() const { return cluster_instances; }
	const Vector<DrawIndexedIndirectArgs>& GetCommands() const { return cluster_commands; }

private:
	void ReleaseBuffers();

	MeshletBuilder::Result build_result;

	Vector<GPUClusterInstance>        cluster_instances;
	Vector<DrawIndexedIndirectArgs>   cluster_commands;

	RHI::Buffer* cluster_buffer = nullptr;
	RHI::Buffer* cluster_draw_buffer = nullptr;
	RHI::Buffer* vertex_ref_buffer = nullptr;
	RHI::Buffer* triangle_buffer = nullptr;
	RHI::Buffer* cluster_instance_buffer = nullptr;
	RHI::Buffer* command_buffer = nullptr;
	RHI::Buffer* cluster_debug_buffer = nullptr;

	Bool is_built = false;
};

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _MESHLET_SCENE_
