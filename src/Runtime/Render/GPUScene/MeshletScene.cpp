#include "Render/GPUScene/MeshletScene.h"
#include "Render/GPUScene/MeshPool.h"
#include "Render/GPUScene/GPUScene.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderBuffer.h"
#include "Tool/BufferUtils.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

Bool MeshletScene::Build(const MeshPool& pool, const GPUSceneManager& scene,
	UInt32 max_vertices, UInt32 max_triangles)
{
	Shutdown();
	build_result = MeshletBuilder::Result{};

	const Vector<GPUInstanceData>& instances = scene.GetInstances();
	if (instances.empty()) return false;

	const Vector<Tool::MeshVertex>& vertices = pool.GetVertices();
	const Vector<UInt32>& indices = pool.GetIndices();
	const Vector<GPUMeshMeta>& metas = pool.GetMeshMetas();
	const Vector<GPUObjectData>& objects = scene.GetObjects();
	if (vertices.empty() || indices.empty() || metas.empty()) return false;

	// ---- 1. cluster every drawable unit -------------------------------------
	// mesh_metas is walked in full (not just group heads): a multi-level LOD
	// group contributes one cluster set per level, which is what lets LOD and
	// cluster culling compose.
	Vector<UInt32> cluster_start(metas.size(), kInvalidIndex);
	Vector<UInt32> cluster_count(metas.size(), 0u);

	for (UInt32 m = 0; m < metas.size(); ++m)
	{
		const GPUMeshMeta& meta = metas[m];
		if (meta.index_count == 0u) continue;

		const UInt32 first = meta.first_index;
		const UInt32 count = meta.index_count;
		if (first >= indices.size() || first + count > indices.size()) continue;

		// The builder takes a slice of the pooled index list. Its values are
		// already pooled-global indices (MeshPool rebased them), which is exactly
		// what it indexes `vertices` with.
		Vector<UInt32> slice(indices.begin() + first, indices.begin() + first + count);

		const UInt32 before = static_cast<UInt32>(build_result.clusters.size());
		MeshletBuilder::Build(vertices, slice, m, 0u, build_result, max_vertices, max_triangles);

		cluster_start[m] = before;
		cluster_count[m] = static_cast<UInt32>(build_result.clusters.size()) - before;
	}

	if (build_result.clusters.empty()) return false;

	// ---- 2. expand clusters into (cluster, object) pairs ---------------------
	// One pair per visible-or-not candidate: the same cluster is tested once per
	// object that instances its mesh, because each instance has its own model
	// matrix and therefore its own bounds in world space.
	cluster_instances.clear();
	for (const GPUInstanceData& inst : instances)
	{
		if (inst.object_id >= objects.size()) continue;
		const UInt32 mesh_id = objects[inst.object_id].mesh_id;
		if (mesh_id >= metas.size()) continue;

		const UInt32 start = cluster_start[mesh_id];
		if (start == kInvalidIndex) continue;

		for (UInt32 k = 0; k < cluster_count[mesh_id]; ++k)
		{
			GPUClusterInstance ci{};
			ci.cluster_id = start + k;
			ci.object_id = inst.object_id;
			cluster_instances.push_back(ci);
		}
	}

	if (cluster_instances.empty()) return false;

	// ---- 3. one indirect command per pair ------------------------------------
	// Geometry fields are fixed here; the cluster culler writes indexCount and
	// instanceCount each frame (zero for rejected pairs). The draw count is
	// therefore the PAIR count, not the visible count — the same degradation the
	// object path uses, for the same reason: there is no indirect-count draw.
	cluster_commands.assign(cluster_instances.size(), DrawIndexedIndirectArgs{});
	for (UInt32 i = 0; i < cluster_instances.size(); ++i)
	{
		const GPUCluster& c = build_result.clusters[cluster_instances[i].cluster_id];
		DrawIndexedIndirectArgs& cmd = cluster_commands[i];
		cmd.index_count = c.triangle_count * 3u;   // culler rewrites this
		cmd.instance_count = 1u;                   // culler rewrites this
		cmd.first_index = c.triangle_offset;
		cmd.vertex_offset = 0;
		// The vertex shader reads this to find the cluster's vertex list. Needs
		// shaderDrawParameters, like the object path's batch slice.
		cmd.first_instance = cluster_instances[i].cluster_id;
	}

	// ---- 4. buffers ----------------------------------------------------------
	cluster_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(build_result.clusters.size() * sizeof(GPUCluster)),
		sizeof(GPUCluster), ENUM_BUFFER_TYPE::Dynamic);
	cluster_draw_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(build_result.cluster_draws.size() * sizeof(GPUClusterDraw)),
		sizeof(GPUClusterDraw), ENUM_BUFFER_TYPE::Dynamic);
	vertex_ref_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(build_result.vertex_refs.size() * sizeof(UInt32)),
		sizeof(UInt32), ENUM_BUFFER_TYPE::Dynamic);
	triangle_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(build_result.triangles.size() * sizeof(UInt32)),
		sizeof(UInt32), ENUM_BUFFER_TYPE::Dynamic);
	cluster_instance_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(cluster_instances.size() * sizeof(GPUClusterInstance)),
		sizeof(GPUClusterInstance), ENUM_BUFFER_TYPE::Dynamic);
	command_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(cluster_commands.size() * sizeof(DrawIndexedIndirectArgs)),
		sizeof(DrawIndexedIndirectArgs),
		ENUM_BUFFER_TYPE::Indirect | ENUM_BUFFER_TYPE::Dynamic);
	// Always allocated, not just when a debug view is active: the cull shader
	// writes it unconditionally, which keeps the decision free of any mode
	// branch and makes the views cost nothing but the bandwidth.
	cluster_debug_buffer = Tool::BufferUtils::CreateStorageBuffer(
		static_cast<UInt32>(cluster_instances.size() * sizeof(UInt32)),
		sizeof(UInt32), ENUM_BUFFER_TYPE::Dynamic);

	// Allocation failure is a recoverable condition, not a programming error, so
	// this returns false rather than asserting — the same convention MeshLoader
	// uses for a missing asset. The half-built state is torn down first: leaving
	// CPU mirrors populated with no buffers behind them would make a later query
	// report clusters that cannot be drawn.
	if (!cluster_buffer || !cluster_draw_buffer || !vertex_ref_buffer
		|| !triangle_buffer || !cluster_instance_buffer || !command_buffer
		|| !cluster_debug_buffer)
	{
		std::cerr << "[MeshletScene] buffer allocation failed (clusters="
			<< build_result.clusters.size() << " pairs=" << cluster_instances.size() << ")" << std::endl;
		Shutdown();
		return false;
	}

	UploadAll();
	is_built = true;
	return true;
}

void MeshletScene::Shutdown()
{
	ReleaseBuffers();
	build_result = MeshletBuilder::Result{};
	cluster_instances.clear();
	cluster_commands.clear();
	is_built = false;
}

void MeshletScene::ReleaseBuffers()
{
	delete cluster_buffer;          cluster_buffer = nullptr;
	delete cluster_draw_buffer;     cluster_draw_buffer = nullptr;
	delete vertex_ref_buffer;       vertex_ref_buffer = nullptr;
	delete triangle_buffer;         triangle_buffer = nullptr;
	delete cluster_instance_buffer; cluster_instance_buffer = nullptr;
	delete command_buffer;          command_buffer = nullptr;
	delete cluster_debug_buffer;    cluster_debug_buffer = nullptr;
}

void MeshletScene::UploadAll()
{
	if (cluster_buffer && !build_result.clusters.empty())
		Tool::BufferUtils::Upload(cluster_buffer, build_result.clusters.data(),
			static_cast<UInt32>(build_result.clusters.size() * sizeof(GPUCluster)));

	if (cluster_draw_buffer && !build_result.cluster_draws.empty())
		Tool::BufferUtils::Upload(cluster_draw_buffer, build_result.cluster_draws.data(),
			static_cast<UInt32>(build_result.cluster_draws.size() * sizeof(GPUClusterDraw)));

	if (vertex_ref_buffer && !build_result.vertex_refs.empty())
		Tool::BufferUtils::Upload(vertex_ref_buffer, build_result.vertex_refs.data(),
			static_cast<UInt32>(build_result.vertex_refs.size() * sizeof(UInt32)));

	if (triangle_buffer && !build_result.triangles.empty())
		Tool::BufferUtils::Upload(triangle_buffer, build_result.triangles.data(),
			static_cast<UInt32>(build_result.triangles.size() * sizeof(UInt32)));

	if (cluster_instance_buffer && !cluster_instances.empty())
		Tool::BufferUtils::Upload(cluster_instance_buffer, cluster_instances.data(),
			static_cast<UInt32>(cluster_instances.size() * sizeof(GPUClusterInstance)));

	if (command_buffer && !cluster_commands.empty())
		Tool::BufferUtils::Upload(command_buffer, cluster_commands.data(),
			static_cast<UInt32>(cluster_commands.size() * sizeof(DrawIndexedIndirectArgs)));

	if (cluster_debug_buffer && !cluster_instances.empty())
	{
		const Vector<UInt32> zeros(cluster_instances.size(), 0u);
		Tool::BufferUtils::Upload(cluster_debug_buffer, zeros.data(),
			static_cast<UInt32>(zeros.size() * sizeof(UInt32)));
	}
}

void MeshletScene::ResetClusterCommands()
{
	// The culler sets both fields every frame; clearing them first means a pair
	// the culler never touches (e.g. because the dispatch was sized too small)
	// draws nothing rather than drawing stale geometry.
	for (DrawIndexedIndirectArgs& cmd : cluster_commands)
	{
		cmd.index_count = 0u;
		cmd.instance_count = 0u;
	}

	if (command_buffer && !cluster_commands.empty())
		Tool::BufferUtils::Upload(command_buffer, cluster_commands.data(),
			static_cast<UInt32>(cluster_commands.size() * sizeof(DrawIndexedIndirectArgs)));
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
