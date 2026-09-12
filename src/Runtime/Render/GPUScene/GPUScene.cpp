#include "Render/GPUScene/GPUScene.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderBuffer.h"
#include "Tool/BufferUtils.h"

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
	draw_commands_cpu.clear();
	batches.clear();
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
	ENSURE(uniform_buffer != nullptr, "GPUScene: failed to allocate uniform buffer");

	UploadMeshes(pool);
	is_initialized = object_buffer && material_buffer && mesh_buffer
		&& visible_buffer && command_buffer && uniform_buffer;
	return is_initialized;
}

void GPUSceneManager::Shutdown()
{
	ReleaseBuffers();
	objects_cpu.clear();
	materials_cpu.clear();
	meshes_cpu.clear();
	visible_ids_cpu.clear();
	draw_commands_cpu.clear();
	batches.clear();
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
}

UInt32 GPUSceneManager::AddMaterial(const GPUMaterialData& material)
{
	ENSURE(materials_cpu.size() < config.max_materials, "GPUScene: material capacity exhausted");
	materials_cpu.push_back(material);
	return static_cast<UInt32>(materials_cpu.size()) - 1u;
}

UInt32 GPUSceneManager::AddObject(const GPUObjectData& object)
{
	ENSURE(objects_cpu.size() < config.max_objects, "GPUScene: object capacity exhausted");
	ENSURE(object.mesh_id != kInvalidIndex, "GPUScene: object registered without a mesh id");
	objects_cpu.push_back(object);
	return static_cast<UInt32>(objects_cpu.size()) - 1u;
}

void GPUSceneManager::SetObjectModel(UInt32 object_id, const glm::mat4& model)
{
	ENSURE(object_id < objects_cpu.size(), "GPUScene: SetObjectModel out of range");
	objects_cpu[object_id].model = model;
}

void GPUSceneManager::SetObjectBounds(UInt32 object_id, const glm::vec4& sphere_bounds)
{
	ENSURE(object_id < objects_cpu.size(), "GPUScene: SetObjectBounds out of range");
	objects_cpu[object_id].sphere_bounds = sphere_bounds;
}

void GPUSceneManager::UploadObjects()
{
	if (!object_buffer || objects_cpu.empty()) return;
	const UInt32 bytes = static_cast<UInt32>(objects_cpu.size() * sizeof(GPUObjectData));
	Tool::BufferUtils::Upload(object_buffer, objects_cpu.data(), bytes);
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

	// Count instances per drawable unit. Iterating mesh ids (not a hash map)
	// keeps batch ordering deterministic, which the self-test relies on.
	Vector<UInt32> counts(mesh_count, 0u);
	for (const GPUObjectData& obj : objects_cpu)
	{
		ENSURE(obj.mesh_id < mesh_count, "GPUScene: object references an unknown mesh id");
		if (obj.mesh_id < mesh_count)
			counts[obj.mesh_id]++;
	}

	// Reserve one contiguous slice of visibleIDs[] per batch.
	Vector<UInt32> base_of(mesh_count, 0u);
	UInt32 cursor = 0u;
	for (UInt32 m = 0; m < mesh_count; ++m)
	{
		base_of[m] = cursor;
		if (counts[m] > 0)
		{
			GPUBatch batch{};
			batch.mesh_id            = m;
			batch.base_instance      = cursor;
			batch.instance_count     = counts[m];   // stage 1: everything visible
			batch.max_instance_count = counts[m];
			batches.push_back(batch);
		}
		cursor += counts[m];
	}

	visible_ids_cpu.assign(cursor, kInvalidIndex);

	// Scatter object ids into their batch's slice.
	Vector<UInt32> write_cursor = base_of;
	for (UInt32 i = 0; i < objects_cpu.size(); ++i)
	{
		const UInt32 mesh_id = objects_cpu[i].mesh_id;
		if (mesh_id >= mesh_count) continue;
		visible_ids_cpu[write_cursor[mesh_id]++] = i;
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
		// firstInstance stays 0: the engine does not enable `shaderDrawParameters`,
		// so it would be ignored. The batch's slice offset travels in a push
		// constant instead (see gpuscene_object.vert).
		args.first_instance = 0;
		draw_commands_cpu.push_back(args);
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
