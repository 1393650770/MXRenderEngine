// Standalone unit tests for the GPU Scene.
//
// These run WITHOUT a device. The scene's CPU side — batching, slot recycling,
// cluster partitioning, the data contract sizes — is where the indexing bugs
// live, and all of it is testable with no GPU at all. That matters here because
// this machine cannot build the renderer (xmake cannot fetch dependencies), so
// these tests are the only executable verification available.
//
// It deliberately does NOT link the whole Runtime: that would pull in
// BufferUtils' real implementation, which reaches straight for the device.
// Instead the handful of device-touching symbols are stubbed below and only the
// four GPUScene translation units are linked. See the GPUScene-SelfTest target
// in xmake.lua.

#include "Render/GPUScene/MeshPool.h"
#include "Render/GPUScene/GPUScene.h"
#include "Render/GPUScene/MeshletData.h"
#include "Render/GPUScene/MeshletScene.h"
#include "Tool/BufferUtils.h"   // the stubs below need to see the class

#include <cmath>
#include <cstdio>
#include <limits>

// windows.h comes in through the RHI headers and defines min/max as macros,
// which breaks std::numeric_limits<...>::max() and glm::min/max.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// ---- stubs ------------------------------------------------------------------
namespace MXRender
{
namespace RHI
{
	class RenderRHI;
}
}

// g_render_rhi is a GLOBAL — it is not a member of any namespace (see CLAUDE.md).
// Defining it inside MXRender::RHI produces a different symbol and the linker
// will still report it missing. Nothing here dereferences it: every path that
// would touch the device is one the tests deliberately avoid.
MXRender::RHI::RenderRHI* g_render_rhi = nullptr;

namespace MXRender
{
namespace Tool
{
	// Declared in MeshLoader.h but defined in MeshLoader.cpp, which would drag in
	// the loader's own dependencies. The tests only need a plausible stride.
	UInt32 MeshDataPayload::GetVertexStride() { return 32; }

	RHI::Buffer* BufferUtils::CreateStorageBuffer(UInt32, UInt32, ENUM_BUFFER_TYPE) { return nullptr; }
	void BufferUtils::Upload(RHI::Buffer*, const void*, UInt32, UInt32) {}
}
}

using namespace MXRender;
using namespace MXRender::Render::GPUScene;

namespace
{
	int g_failures = 0;
	int g_checks = 0;
	const char* g_group = "";

	void BeginGroup(const char* name)
	{
		g_group = name;
		std::printf("\n--- %s ---\n", name);
	}

	void Check(bool condition, const char* what)
	{
		++g_checks;
		if (!condition) ++g_failures;
		std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
	}

	Tool::MeshDataPayload MakeTriangle()
	{
		Tool::MeshDataPayload p;
		p.vertices.resize(3);
		for (UInt32 i = 0; i < 3; ++i)
		{
			p.vertices[i].position[0] = static_cast<Float32>(i);
			p.vertices[i].normal[1] = 1.0f;
		}
		p.indices = { 0u, 1u, 2u };
		Tool::MeshDataPayload::SubMesh sub{};
		sub.index_count = 3;
		p.sub_meshes.push_back(sub);
		p.bounds_max[0] = 2.0f;
		return p;
	}

	GPUObjectData MakeObject(UInt32 mesh_id, UInt32 material_id)
	{
		GPUObjectData o{};
		o.mesh_id = mesh_id;
		o.material_id = material_id;
		return o;
	}
}

// ---- 1. data contract --------------------------------------------------------
// These sizes ARE the contract with the shaders. If one drifts, the GPU reads
// garbage and nothing warns — so they are asserted here.
static void TestDataContract()
{
	BeginGroup("data contract (std430 sizes mirrored in GLSL)");

	Check(sizeof(GPUObjectData) == 96, "GPUObjectData is 96 bytes");
	Check(sizeof(GPUMaterialData) == 48, "GPUMaterialData is 48 bytes");
	Check(sizeof(GPUMeshMeta) == 48, "GPUMeshMeta is 48 bytes");
	Check(sizeof(GPUSceneUniformsData) == 304, "GPUSceneUniformsData is 304 bytes");
	Check(sizeof(GPUInstanceData) == 8, "GPUInstanceData is 8 bytes");
	Check(sizeof(GPUBatch) == 16, "GPUBatch is 16 bytes");
	Check(sizeof(GPUCluster) == 32, "GPUCluster is 32 bytes");
	Check(sizeof(GPUClusterDraw) == 16, "GPUClusterDraw is 16 bytes");
	Check(sizeof(GPUClusterInstance) == 8, "GPUClusterInstance is 8 bytes");

	Check(kMeshletMaxVertices == 64u, "meshlet vertex cap is 64");
	Check(kMeshletMaxTriangles == 124u, "meshlet triangle cap is 124");

	Check(kClusterCullFrustum == 1u && kClusterCullDistance == 2u
		&& kClusterCullOcclusion == 4u && kClusterCullDrawn == 8u,
		"cull flags are four distinct bits");
	Check((kClusterCullFrustum | kClusterCullDistance | kClusterCullOcclusion | kClusterCullDrawn) == 15u,
		"cull flags pack into four bits");

	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::Off) == 0u, "debug mode Off is 0");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::ClusterId) == 1u, "ClusterId is 1");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::CullState) == 2u, "CullState is 2");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::LodLevel) == 3u, "LodLevel is 3");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::MaterialId) == 4u, "MaterialId is 4");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::TriangleDensity) == 5u, "TriangleDensity is 5");
	Check(static_cast<UInt32>(ENUM_CLUSTER_DEBUG::HizLevel) == 6u, "HizLevel is 6");
}

// ---- 2. mesh pool ------------------------------------------------------------
static void TestMeshPool()
{
	BeginGroup("MeshPool");

	MeshPool pool;
	pool.AddMesh(MakeTriangle());
	pool.AddMesh(MakeTriangle());

	Check(pool.GetVertexCount() == 6, "vertices merged into one pool");
	Check(pool.GetIndexCount() == 6, "indices merged into one pool");
	// Rebasing by baseVertex is what lets a single indirect command address any
	// drawable unit, and keeps every command's vertexOffset at 0.
	Check(pool.GetIndices()[3] == 3u, "second mesh's indices rebased by baseVertex");
	Check(pool.GetMeshMetas()[1].first_index == 3u, "second mesh's first_index is offset");
	Check(pool.GetMeshMetas()[1].vertex_offset == 0, "vertexOffset stays 0 after rebasing");
	Check(pool.GetLODGroupHeads().size() == 2, "two single-level groups");

	// LOD groups: several levels, one head.
	MeshPool lod_pool;
	Vector<Tool::MeshDataPayload> levels;
	levels.push_back(MakeTriangle());
	levels.push_back(MakeTriangle());
	const Vector<UInt32> ids = lod_pool.AddMeshLODs(levels);

	Check(ids.size() == 1, "a LOD group reports one id (its LOD0)");
	Check(lod_pool.GetMeshCount() == 2, "both levels are registered");
	Check(lod_pool.GetLODGroupHeads().size() == 1, "they form ONE group, not two meshes");
	Check(lod_pool.GetMeshMetas()[ids[0]].lod_count == 2, "head knows it has 2 levels");
	// Contiguity is what lets the cull shader reach a level with base + lod.
	Check(lod_pool.GetMeshMetas()[ids[0]].lod_offsets[0] == ids[0]
		&& lod_pool.GetMeshMetas()[ids[0]].lod_offsets[1] == ids[0] + 1,
		"levels are contiguous after the head");
}

// ---- 3. batching -------------------------------------------------------------
static void TestBatching()
{
	BeginGroup("batching and visibleIDs");

	MeshPool pool;
	pool.AddMesh(MakeTriangle());
	pool.AddMesh(MakeTriangle());

	GPUSceneManager scene;
	const UInt32 mesh_of[5] = { 0u, 1u, 0u, 1u, 0u };
	for (UInt32 i = 0; i < 5; ++i)
		scene.AddObject(MakeObject(mesh_of[i], i % 2));

	scene.BuildBatches(pool);
	Check(scene.GetBatchCount() == 2, "one batch per drawable unit");
	Check(scene.GetBatches()[0].base_instance == 0u && scene.GetBatches()[0].instance_count == 3u,
		"batch0: three instances at base 0");
	Check(scene.GetBatches()[1].base_instance == 3u && scene.GetBatches()[1].instance_count == 2u,
		"batch1: two instances at base 3");

	const Vector<UInt32>& vis = scene.GetVisibleIDs();
	Check(vis[0] == 0u && vis[1] == 2u && vis[2] == 4u, "batch0 slice holds objects 0,2,4");
	Check(vis[3] == 1u && vis[4] == 3u, "batch1 slice holds objects 1,3");

	const Vector<GPUInstanceData>& insts = scene.GetInstances();
	Check(insts.size() == 5, "one instance entry per object");
	Check(insts[1].batch_id == 1u && insts[4].batch_id == 0u, "instance -> batch mapping");

	// Material ordering inside a batch, for texture locality.
	MeshPool p2;
	p2.AddMesh(MakeTriangle());
	GPUSceneManager s2;
	const UInt32 materials[3] = { 2u, 0u, 1u };
	for (UInt32 i = 0; i < 3; ++i)
		s2.AddObject(MakeObject(0, materials[i]));
	s2.BuildBatches(p2);
	const Vector<UInt32>& v2 = s2.GetVisibleIDs();
	Check(v2[0] == 1u && v2[1] == 2u && v2[2] == 0u, "instances sorted by material id");

	// One batch per LOD level; every level reserves the whole group.
	MeshPool lp;
	Vector<Tool::MeshDataPayload> lv;
	lv.push_back(MakeTriangle());
	lv.push_back(MakeTriangle());
	const Vector<UInt32> lid = lp.AddMeshLODs(lv);
	GPUSceneManager ls;
	for (UInt32 i = 0; i < 4; ++i)
		ls.AddObject(MakeObject(lid[0], i % 2));
	ls.BuildBatches(lp);
	Check(ls.GetBatchCount() == 2, "one batch per LOD level");
	Check(ls.GetBatches()[1].mesh_id == lid[0] + 1, "batch1 is LOD1 geometry");
	Check(ls.GetBatches()[0].max_instance_count == 4 && ls.GetBatches()[1].max_instance_count == 4,
		"every level reserves the whole group");
	Check(ls.GetVisibleIDs().size() == 8, "visibleIDs reserves a slice per level");
	Check(ls.GetInstances()[3].batch_id == 0, "instances share the group base");

	// Single-level meshes must be unaffected by the LOD machinery.
	MeshPool fp;
	fp.AddMesh(MakeTriangle());
	GPUSceneManager fs;
	for (UInt32 i = 0; i < 3; ++i)
		fs.AddObject(MakeObject(0, i));
	fs.BuildBatches(fp);
	Check(fs.GetBatchCount() == 1, "single-level mesh yields one batch");
	Check(fs.GetVisibleIDs().size() == 3, "single-level visibleIDs is not inflated");
}

// ---- 4. draw commands --------------------------------------------------------
static void TestDrawCommands()
{
	BeginGroup("draw commands");

	MeshPool pool;
	pool.AddMesh(MakeTriangle());
	pool.AddMesh(MakeTriangle());
	GPUSceneManager scene;
	const UInt32 mesh_of[5] = { 0u, 1u, 0u, 1u, 0u };
	for (UInt32 i = 0; i < 5; ++i)
		scene.AddObject(MakeObject(mesh_of[i], i % 2));
	scene.BuildBatches(pool);
	scene.BuildDrawCommands(pool);

	const Vector<DrawIndexedIndirectArgs>& cmds = scene.GetDrawCommands();
	Check(cmds.size() == 2, "one command per batch");
	Check(cmds[0].index_count == 3u && cmds[0].instance_count == 3u && cmds[0].first_index == 0u,
		"command0 matches mesh0");
	Check(cmds[1].index_count == 3u && cmds[1].instance_count == 2u && cmds[1].first_index == 3u,
		"command1 matches mesh1");
	Check(cmds[0].vertex_offset == 0 && cmds[1].vertex_offset == 0, "vertexOffset is always 0");

	// firstInstance carries the batch slice only when the device honours it.
	scene.SetUseFirstInstance(true);
	scene.BuildDrawCommands(pool);
	Check(scene.GetDrawCommands()[1].first_instance == 3u, "firstInstance carries the batch slice");

	scene.SetUseFirstInstance(false);
	scene.BuildDrawCommands(pool);
	Check(scene.GetDrawCommands()[1].first_instance == 0u, "fallback zeroes firstInstance");

	// Reset clears the counter the culler accumulates, and nothing else.
	scene.ResetDrawCommands();
	Check(scene.GetDrawCommands()[0].instance_count == 0u, "reset clears instanceCount");
	Check(scene.GetDrawCommands()[0].index_count == 3u, "reset preserves indexCount");
	Check(scene.GetDrawCommands()[1].first_index == 3u, "reset preserves firstIndex");
}

// ---- 5. slots ----------------------------------------------------------------
static void TestSlots()
{
	BeginGroup("object slots (recycling)");

	MeshPool pool;
	pool.AddMesh(MakeTriangle());
	GPUSceneManager scene;

	Vector<GPUObjectHandle> handles;
	for (UInt32 i = 0; i < 3; ++i)
		handles.push_back(scene.AddObject(MakeObject(0, i)));
	scene.BuildBatches(pool);

	Check(scene.GetActiveObjectCount() == 3, "three active before removal");
	Check(scene.IsObjectAlive(handles[1]), "handle is alive before removal");
	Check(scene.RemoveObject(handles[1]), "RemoveObject reports success");
	Check(!scene.IsObjectAlive(handles[1]), "removed handle is dead");
	Check(!scene.RemoveObject(handles[1]), "removing twice is rejected");
	Check(scene.GetObjectByHandle(handles[1]) == nullptr, "a dead handle yields no object");

	scene.BuildBatches(pool);
	Check(scene.GetActiveObjectCount() == 2, "two active after removal");
	const Vector<UInt32>& vis = scene.GetVisibleIDs();
	bool removed_gone = true;
	for (UInt32 v : vis)
		if (v == handles[1].index) removed_gone = false;
	Check(removed_gone, "removed object is absent from visibleIDs");
	Check(scene.GetFreeSlotCount() == 1, "one slot is on the free list");

	// Recycling: the freed slot must be handed back out, not grown past.
	const UInt32 slots_before = scene.GetSlotCount();
	const GPUObjectHandle reused = scene.AddObject(MakeObject(0, 9));
	Check(scene.GetSlotCount() == slots_before, "recycled a freed slot instead of growing");
	Check(reused.index == handles[1].index, "LIFO free list returns the most recently freed slot");
	Check(reused.generation != handles[1].generation, "generation was bumped on recycle");
	Check(!scene.IsObjectAlive(handles[1]), "the old handle stays dead after the slot is reused");
	Check(scene.IsObjectAlive(reused), "the new handle is alive");
	Check(scene.GetFreeSlotCount() == 0, "free list drained");

	scene.BuildBatches(pool);
	Check(scene.GetActiveObjectCount() == 3, "three active again after refill");

	// Churn must not consume slots — that is the whole point of the free list.
	for (UInt32 cycle = 0; cycle < 100; ++cycle)
	{
		const GPUObjectHandle h = scene.AddObject(MakeObject(0, 1));
		scene.RemoveObject(h);
	}
	Check(scene.GetSlotCount() == slots_before + 1, "100 add/remove cycles consumed no new slots");

	// Dirty tracking: mutations mark a slot, and the queue is what gets uploaded.
	const UInt32 slot = handles[0].index;
	scene.SetObjectModel(slot, glm::mat4(2.0f));
	Check(scene.GetObject(slot).model[0][0] == 2.0f, "SetObjectModel writes through");
}

// ---- 6. meshlets -------------------------------------------------------------
static void TestMeshlets()
{
	BeginGroup("meshlet clusters");

	Vector<Tool::MeshVertex> cube(8);
	for (UInt32 i = 0; i < 8; ++i)
	{
		cube[i].position[0] = (i & 1u) ? 1.0f : 0.0f;
		cube[i].position[1] = (i & 2u) ? 1.0f : 0.0f;
		cube[i].position[2] = (i & 4u) ? 1.0f : 0.0f;
		cube[i].normal[1] = 1.0f;
	}
	const UInt32 cube_index[36] = {
		0,1,3, 0,3,2, 4,6,7, 4,7,5, 0,4,5, 0,5,1,
		2,3,7, 2,7,6, 0,2,6, 0,6,4, 1,5,7, 1,7,3
	};
	const Vector<UInt32> indices(cube_index, cube_index + 36);

	MeshletBuilder::Result r;
	MeshletBuilder::Build(cube, indices, 0u, 0u, r, 4u, 4u);

	Check(!r.clusters.empty(), "the build produced clusters");
	Check(r.clusters.size() >= 3u, "12 triangles under a 4-triangle cap needs at least 3");
	Check(r.cluster_draws.size() == r.clusters.size(), "one draw record per cluster");
	Check(r.cluster_draws[0].mesh_id == 0u, "draw records carry the mesh id");

	UInt32 total_triangles = 0;
	bool sizes_ok = true, refs_ok = true, tris_ok = true, bounds_ok = true;
	for (const GPUCluster& c : r.clusters)
	{
		if (c.vertex_count == 0u || c.vertex_count > 4u) sizes_ok = false;
		if (c.triangle_count == 0u || c.triangle_count > 4u) sizes_ok = false;
		if (c.bounds.w <= 0.0f) bounds_ok = false;
		total_triangles += c.triangle_count;

		for (UInt32 i = 0; i < c.vertex_count; ++i)
			if (r.vertex_refs[c.vertex_ref_offset + i] >= 8u) refs_ok = false;

		// Local indices must be relative to the cluster, not absolute into the
		// shared vertex-ref array — the bug this test was written to catch.
		for (UInt32 i = 0; i < c.triangle_count * 3u; ++i)
			if (r.triangles[c.triangle_offset + i] >= c.vertex_count) tris_ok = false;
	}
	Check(sizes_ok, "every cluster respects the vertex/triangle caps");
	Check(bounds_ok, "every cluster got real bounds");
	Check(refs_ok, "vertex refs point at real vertices");
	Check(tris_ok, "triangle indices stay inside their own cluster");
	Check(total_triangles == 12u, "no triangle was lost while partitioning");
	Check(r.clusters[0].vertex_count < 8u, "a cluster is a proper subset of the mesh");

	// Default caps: a cube fits in one cluster, so nothing should split.
	MeshletBuilder::Result r2;
	MeshletBuilder::Build(cube, indices, 0u, 0u, r2);
	Check(r2.clusters.size() == 1u, "a cube fits in a single default cluster");
	Check(r2.clusters[0].triangle_count == 12u, "that cluster holds all 12 triangles");
	Check(r2.clusters[0].vertex_count == 8u, "and all 8 vertices");

	// MeshletScene: pairs, and a clean failure when buffers cannot be made.
	MeshPool mp;
	mp.AddMesh(MakeTriangle());
	GPUSceneManager ms;
	for (UInt32 i = 0; i < 3; ++i)
		ms.AddObject(MakeObject(0, i));
	ms.BuildBatches(mp);

	MeshletScene msc;
	const bool built = msc.Build(mp, ms, 3u, 1u);   // stubbed device: no buffers
	Check(!built, "Build reports failure when buffer creation fails");
	Check(msc.GetCommandCount() == 0u, "a failed Build leaves nothing half-built");

	msc.Shutdown();
	Check(msc.GetClusterCount() == 0u, "Shutdown clears the build result");
}

int main()
{
	std::printf("=== GPU Scene unit tests (no device required) ===\n");

	TestDataContract();
	TestMeshPool();
	TestBatching();
	TestDrawCommands();
	TestSlots();
	TestMeshlets();

	std::printf("\n===========================================\n");
	if (g_failures == 0)
	{
		std::printf("ALL PASS — %d checks\n", g_checks);
		return 0;
	}
	std::printf("FAILURES: %d of %d checks failed\n", g_failures, g_checks);
	return 1;
}
