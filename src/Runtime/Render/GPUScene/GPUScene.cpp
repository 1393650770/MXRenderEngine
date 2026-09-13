#include "Render/GPUScene/GPUScene.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderBuffer.h"
#include "Tool/BufferUtils.h"
#include <algorithm>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

Bool GPUSceneManager::Initialize(const Config& cfg, const MeshPool& pool)
{
	config = cfg;
	ReleaseBuffers();

	objects_cpu.clear();
	materials_cpu.clear();
	visible_ids_cpu.clear();
	instances_cpu.clear();
	draw_commands_cpu.clear();
	batches.clear();
	slot_generation.clear();
	free_slots.clear();
	dirty_slots.clear();
	objects_cpu.reserve(cfg.max_objects);
	materials_cpu.reserve(cfg.max_materials);

	// Every buffer below carries the Dynamic bit: host-visible persistently
	// mapped memory. It is the only reliable upload path in this engine — a
	// plain Storage buffer's Map goes through a staging + TRANSFER-queue copy
	// with no cross-queue sync (CLAUDE.md RHI Gotchas).
	object_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_objects * sizeof(GPUObjectData), sizeof(GPUObjectData), ENUM_BUFFER_TYPE::Dynamic);
	material_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_materials * sizeof(GPUMaterialData), sizeof(GPUMaterialData), ENUM_BUFFER_TYPE::Dynamic);
	mesh_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_meshes * sizeof(GPUMeshMeta), sizeof(GPUMeshMeta), ENUM_BUFFER_TYPE::Dynamic);
	visible_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_objects * sizeof(UInt32), sizeof(UInt32), ENUM_BUFFER_TYPE::Dynamic);
	instance_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_objects * sizeof(GPUInstanceData), sizeof(GPUInstanceData), ENUM_BUFFER_TYPE::Dynamic);
	batch_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_batches * sizeof(GPUBatch), sizeof(GPUBatch), ENUM_BUFFER_TYPE::Dynamic);

	// Storage|Indirect|Dynamic: the CPU fills it in stage 1, and from stage 2 on
	// the culling compute shader writes instanceCount into it directly.
	command_buffer = Tool::BufferUtils::CreateStorageBuffer(
		cfg.max_batches * sizeof(DrawIndexedIndirectArgs), sizeof(DrawIndexedIndirectArgs),
		ENUM_BUFFER_TYPE::Indirect | ENUM_BUFFER_TYPE::Dynamic);

	uniform_buffer = Tool::BufferUtils::CreateStorageBuffer(
		sizeof(GPUSceneUniformsData), sizeof(GPUSceneUniformsData), ENUM_BUFFER_TYPE::Dynamic);

	ENSURE(object_buffer != nullptr, "GPUScene: failed to allocate object buffer");
	ENSURE(material_buffer != nullptr, "GPUScene: failed to allocate material buffer");
	ENSURE(mesh_buffer != nullptr, "GPUScene: failed to allocate mesh buffer");
	ENSURE(visible_buffer != nullptr, "GPUScene: failed to allocate visible-id buffer");
	ENSURE(command_buffer != nullptr, "GPUScene: failed to allocate draw-command buffer");
	ENSURE(instance_buffer != nullptr, "GPUScene: failed to allocate instance buffer");
	ENSURE(batch_buffer != nullptr, "GPUScene: failed to allocate batch buffer");

	UploadMeshes(pool);
	is_initialized = object_buffer && material_buffer && mesh_buffer
		&& visible_buffer && command_buffer && uniform_buffer
		&& instance_buffer && batch_buffer;
	return is_initialized;
}

void GPUSceneManager::Shutdown()
{
	ReleaseBuffers();
	objects_cpu.clear();
	materials_cpu.clear();
	meshes_cpu.clear();
	visible_ids_cpu.clear();
	instances_cpu.clear();
	draw_commands_cpu.clear();
	batches.clear();
	slot_generation.clear();
	free_slots.clear();
	dirty_slots.clear();
	is_initialized = false;
}

void GPUSceneManager::ReleaseBuffers()
{
	delete object_buffer;   object_buffer = nullptr;
	delete material_buffer; material_buffer = nullptr;
	delete mesh_buffer;     mesh_buffer = nullptr;
	delete visible_buffer;  visible_buffer = nullptr;
	delete command_buffer;  command_buffer = nullptr;
	delete uniform_buffer;  uniform_buffer = nullptr;
	delete instance_buffer; instance_buffer = nullptr;
	delete batch_buffer;    batch_buffer = nullptr;
}

UInt32 GPUSceneManager::AddMaterial(const GPUMaterialData& material)
{
	ENSURE(materials_cpu.size() < config.max_materials, "GPUScene: material capacity exhausted");
	materials_cpu.push_back(material);
	return static_cast<UInt32>(materials_cpu.size()) - 1u;
}

GPUObjectHandle GPUSceneManager::AddObject(const GPUObjectData& object)
{
	ENSURE(object.mesh_id != kInvalidIndex, "GPUScene: object registered without a mesh id");

	UInt32 slot = kInvalidIndex;
	if (!free_slots.empty())
	{
		// Recycle. The generation was bumped when the slot was released, so it
		// stays as-is and any handle from the previous tenant is already stale.
		slot = free_slots.back();
		free_slots.pop_back();
	}
	else
	{
		ENSURE(objects_cpu.size() < config.max_objects, "GPUScene: object capacity exhausted");
		if (objects_cpu.size() >= config.max_objects)
			return GPUObjectHandle{};
		slot = static_cast<UInt32>(objects_cpu.size());
		objects_cpu.push_back(GPUObjectData{});
		slot_generation.push_back(1u);
	}

	objects_cpu[slot] = object;
	objects_cpu[slot].flags &= ~kObjectFlagDeleted;
	dirty_slots.push_back(slot);

	return GPUObjectHandle{ slot, slot_generation[slot] };
}

Bool GPUSceneManager::RemoveObject(GPUObjectHandle handle)
{
	if (!IsObjectAlive(handle)) return false;

	const UInt32 slot = handle.index;
	objects_cpu[slot].flags |= kObjectFlagDeleted;

	// Bump the generation so every other handle to this slot goes stale, then
	// return the slot to the free list. The object keeps its slot until the next
	// AddObject takes it — BuildBatches is what actually stops drawing it.
	UInt32 gen = (slot_generation[slot] + 1u) & kObjectGenerationMask;
	if (gen == 0u) gen = 1u;      // 0 is reserved, so a fresh slot is never gen 0
	slot_generation[slot] = gen;

	free_slots.push_back(slot);
	return true;
}

Bool GPUSceneManager::IsObjectAlive(GPUObjectHandle handle) const
{
	if (!handle.IsValid()) return false;
	if (handle.index >= objects_cpu.size()) return false;
	if (slot_generation[handle.index] != handle.generation) return false;   // stale handle
	return (objects_cpu[handle.index].flags & kObjectFlagDeleted) == 0u;
}

const GPUObjectData* GPUSceneManager::GetObjectByHandle(GPUObjectHandle handle) const
{
	return IsObjectAlive(handle) ? &objects_cpu[handle.index] : nullptr;
}

void GPUSceneManager::MarkObjectDirty(UInt32 slot)
{
	if (slot < objects_cpu.size())
		dirty_slots.push_back(slot);
}

void GPUSceneManager::SetObjectModel(UInt32 slot, const glm::mat4& model)
{
	ENSURE(slot < objects_cpu.size(), "GPUScene: SetObjectModel out of range");
	if (slot >= objects_cpu.size()) return;
	objects_cpu[slot].model = model;
	MarkObjectDirty(slot);
}

void GPUSceneManager::SetObjectBounds(UInt32 slot, const glm::vec4& sphere_bounds)
{
	ENSURE(slot < objects_cpu.size(), "GPUScene: SetObjectBounds out of range");
	if (slot >= objects_cpu.size()) return;
	objects_cpu[slot].sphere_bounds = sphere_bounds;
	MarkObjectDirty(slot);
}

void GPUSceneManager::UploadObjects()
{
	if (!object_buffer || objects_cpu.empty()) return;

	// Fast path: nothing tracked as changed but the caller asked for an upload
	// (e.g. the very first one after registration) — fall back to a full sweep.
	if (dirty_slots.empty())
	{
		const UInt32 bytes = static_cast<UInt32>(objects_cpu.size() * sizeof(GPUObjectData));
		Tool::BufferUtils::Upload(object_buffer, objects_cpu.data(), bytes);
		return;
	}

	// Otherwise upload only the slots that moved. If most of the scene is dirty
	// the scattered writes cost more than one contiguous copy, so switch back.
	const UInt32 slot_size = static_cast<UInt32>(sizeof(GPUObjectData));
	if (dirty_slots.size() * 4u >= objects_cpu.size())
	{
		const UInt32 bytes = static_cast<UInt32>(objects_cpu.size() * sizeof(GPUObjectData));
		Tool::BufferUtils::Upload(object_buffer, objects_cpu.data(), bytes);
		dirty_slots.clear();
		return;
	}

	// One map for all the scattered ranges: mapping per slot would pay the
	// vkMapMemory/vkUnmapMemory round trip once per object.
	void* mapped = g_render_rhi->MapBuffer(object_buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	if (mapped)
	{
		for (UInt32 slot : dirty_slots)
		{
			if (slot >= objects_cpu.size()) continue;
			std::memcpy(static_cast<UInt8*>(mapped) + slot * slot_size,
				&objects_cpu[slot], slot_size);
		}
		g_render_rhi->UnmapBuffer(object_buffer);
	}
	dirty_slots.clear();
}

void GPUSceneManager::UploadMaterials()
{
	if (!material_buffer || materials_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(materials_cpu.size() * sizeof(GPUMaterialData));
	Tool::BufferUtils::Upload(material_buffer, materials_cpu.data(), bytes);
}

void GPUSceneManager::UploadMeshes(const MeshPool& pool)
{
	if (!mesh_buffer) return;
	meshes_cpu = pool.GetMeshMetas();
	if (meshes_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(meshes_cpu.size() * sizeof(GPUMeshMeta));
	Tool::BufferUtils::Upload(mesh_buffer, meshes_cpu.data(), bytes);
}

void GPUSceneManager::UploadVisibleIDs()
{
	if (!visible_buffer || visible_ids_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(visible_ids_cpu.size() * sizeof(UInt32));
	Tool::BufferUtils::Upload(visible_buffer, visible_ids_cpu.data(), bytes);
}

void GPUSceneManager::UploadDrawCommands()
{
	if (!command_buffer || draw_commands_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(draw_commands_cpu.size() * sizeof(DrawIndexedIndirectArgs));
	Tool::BufferUtils::Upload(command_buffer, draw_commands_cpu.data(), bytes);
}

void GPUSceneManager::UploadInstances()
{
	if (!instance_buffer || instances_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(instances_cpu.size() * sizeof(GPUInstanceData));
	Tool::BufferUtils::Upload(instance_buffer, instances_cpu.data(), bytes);
}

void GPUSceneManager::UploadBatches()
{
	if (!batch_buffer || batches.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(batches.size() * sizeof(GPUBatch));
	Tool::BufferUtils::Upload(batch_buffer, batches.data(), bytes);
}

void GPUSceneManager::ResetDrawCommands()
{
	// Only instanceCount needs clearing; the geometry fields are rebuilt by
	// BuildDrawCommands and must survive.
	for (DrawIndexedIndirectArgs& args : draw_commands_cpu)
		args.instance_count = 0;
	UploadDrawCommands();
}

void GPUSceneManager::UploadUniforms()
{
	if (!uniform_buffer) return;
	Tool::BufferUtils::Upload(uniform_buffer, &uniforms_cpu, sizeof(GPUSceneUniformsData));
}

void GPUSceneManager::BuildBatches(const MeshPool& pool)
{
	batches.clear();
	visible_ids_cpu.clear();
	draw_commands_cpu.clear();

	const UInt32 mesh_count = pool.GetMeshCount();
	if (mesh_count == 0 || objects_cpu.empty()) return;

	// Collect the object ids of each drawable unit.
	Vector<Vector<UInt32>> per_mesh(mesh_count);
	for (UInt32 i = 0; i < objects_cpu.size(); ++i)
	{
		if (objects_cpu[i].flags & kObjectFlagDeleted) continue;   // soft-deleted
		const UInt32 m = objects_cpu[i].mesh_id;
		ENSURE(m < mesh_count, "GPUScene: object references an unknown mesh id");
		if (m < mesh_count)
			per_mesh[m].push_back(i);
	}

	// Reserve one contiguous slice of visibleIDs[] per batch, and remember which
	// batch index each mesh ended up in — the culling shader needs it to know
	// where an instance's output belongs.
	Vector<UInt32> base_of(mesh_count, 0u);
	Vector<UInt32> batch_index_of(mesh_count, kInvalidIndex);
	UInt32 cursor = 0u;
	for (UInt32 m = 0; m < mesh_count; ++m)
	{
		if (per_mesh[m].empty()) continue;

		// Stage 3 — parameter merging. Sorting instances by material makes same
		// material runs contiguous inside visibleIDs[], which is what gives the
		// bindless texture fetches their locality. Sorting by object id as a
		// tiebreaker keeps the result deterministic for the self-test.
		// (shader_id would join this key once more than one shader family exists.)
		std::stable_sort(per_mesh[m].begin(), per_mesh[m].end(),
			[this](UInt32 a, UInt32 b)
			{
				const UInt32 ma = objects_cpu[a].material_id;
				const UInt32 mb = objects_cpu[b].material_id;
				if (ma != mb) return ma < mb;
				return a < b;
			});

		base_of[m] = cursor;
		batch_index_of[m] = static_cast<UInt32>(batches.size());
		GPUBatch batch{};
		batch.mesh_id            = m;
		batch.base_instance      = cursor;
		batch.instance_count     = static_cast<UInt32>(per_mesh[m].size());
		batch.max_instance_count = batch.instance_count;
		batches.push_back(batch);
		cursor += static_cast<UInt32>(per_mesh[m].size());
	}

	visible_ids_cpu.assign(cursor, kInvalidIndex);

	// Scatter each batch's objects into its slice, in the sorted order above.
	for (UInt32 m = 0; m < mesh_count; ++m)
	{
		if (per_mesh[m].empty()) continue;
		UInt32 slot = 0u;
		for (UInt32 object_id : per_mesh[m])
			visible_ids_cpu[base_of[m] + slot++] = object_id;
	}

	// Instance stream: indexed by object id, so the culling shader can map any
	// object straight to its batch.
	instances_cpu.clear();
	instances_cpu.reserve(objects_cpu.size());
	for (UInt32 i = 0; i < objects_cpu.size(); ++i)
	{
		if (objects_cpu[i].flags & kObjectFlagDeleted) continue;   // soft-deleted

		const UInt32 m = objects_cpu[i].mesh_id;
		GPUInstanceData inst{};
		inst.object_id = i;
		inst.batch_id = (m < mesh_count) ? batch_index_of[m] : kInvalidIndex;
		instances_cpu.push_back(inst);
	}
}

void GPUSceneManager::BuildDrawCommands(const MeshPool& pool)
{
	draw_commands_cpu.clear();
	if (batches.empty()) return;

	const Vector<GPUMeshMeta>& metas = pool.GetMeshMetas();
	draw_commands_cpu.reserve(batches.size());

	for (const GPUBatch& batch : batches)
	{
		ENSURE(batch.mesh_id < metas.size(), "GPUScene: batch references an unknown mesh id");
		const GPUMeshMeta& meta = metas[batch.mesh_id];

		DrawIndexedIndirectArgs args;
		args.index_count    = meta.index_count;
		args.instance_count = batch.instance_count;
		args.first_index    = meta.first_index;
		args.vertex_offset  = meta.vertex_offset;
		// When shaderDrawParameters is available this is what lets all batches go
		// out as a single indirect command: gl_InstanceIndex starts here instead
		// of at 0. Without the feature it is ignored, and the caller falls back
		// to one command per batch plus a push constant.
		args.first_instance = use_first_instance ? batch.base_instance : 0u;
		draw_commands_cpu.push_back(args);
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
