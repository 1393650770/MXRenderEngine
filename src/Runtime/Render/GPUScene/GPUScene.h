#pragma once
#ifndef _GPU_SCENE_
#define _GPU_SCENE_

#include "Core/ConstDefine.h"
#include "Render/GPUScene/GPUSceneData.h"
#include "Render/GPUScene/MeshPool.h"
#include "RHI/RenderCommandList.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

// Owns every retained buffer that makes up the GPU Scene, plus the CPU-side
// mirror of each one.
//
// Binding model (the reason this class exists):
//   ALL of these buffers are bound exactly once, at initialization, through a
//   single SRB. From then on they are never rebound — per-frame updates are
//   plain buffer writes, and per-object parameters reach the shader as array
//   reads rather than descriptor bindings.
//
// Buffers are retained on purpose: the RDG resource pool is LIFO, so a transient
// resource's physical identity changes every frame and binding one into a
// persistent descriptor set corrupts the next frame (CLAUDE.md RHI Gotchas).
// Named GPUSceneManager rather than GPUScene: the enclosing namespace is
// already GPUScene, and a type with the same name as its namespace makes every
// unqualified use inside it ambiguous (C2872).
class GPUSceneManager
{
public:
	struct Config
	{
		UInt32 max_objects   = kMaxObjects;
		UInt32 max_materials = kMaxMaterials;
		UInt32 max_meshes    = kMaxMeshes;
		UInt32 max_batches   = kMaxBatches;
	};

	GPUSceneManager() = default;
	~GPUSceneManager() = default;

	Bool Initialize(const Config& cfg, const MeshPool& pool);
	void Shutdown();
	Bool IsInitialized() const { return is_initialized; }

	// ---- registration ----
	UInt32 AddMaterial(const GPUMaterialData& material);
	UInt32 AddObject(const GPUObjectData& object);

	// ---- CPU-side mutation ----
	GPUObjectData&       GetObject(UInt32 object_id)       { return objects_cpu[object_id]; }
	const GPUObjectData& GetObject(UInt32 object_id) const { return objects_cpu[object_id]; }
	void SetObjectModel(UInt32 object_id, const glm::mat4& model);
	void SetObjectBounds(UInt32 object_id, const glm::vec4& sphere_bounds);

	// ---- upload (buffer writes, never descriptor updates) ----
	void UploadObjects();
	void UploadMaterials();
	void UploadMeshes(const MeshPool& pool);
	void UploadVisibleIDs();
	void UploadDrawCommands();
	void UploadUniforms();

	// ---- batching ----
	// One batch per drawable unit (mesh id). Instances within a batch may use
	// different materials — material is per-instance via objects[].material_id,
	// only the geometry has to match.
	void BuildBatches(const MeshPool& pool);
	void BuildDrawCommands(const MeshPool& pool);

	// ---- GPU buffers (bind once) ----
	RHI::Buffer* GetObjectBuffer()   const { return object_buffer; }
	RHI::Buffer* GetMaterialBuffer() const { return material_buffer; }
	RHI::Buffer* GetMeshBuffer()     const { return mesh_buffer; }
	RHI::Buffer* GetVisibleIDBuffer()const { return visible_buffer; }
	RHI::Buffer* GetDrawCommandBuffer() const { return command_buffer; }
	RHI::Buffer* GetUniformBuffer()  const { return uniform_buffer; }

	// ---- CPU-side views (also used by the headless self-test) ----
	GPUSceneUniformsData& GetUniforms() { return uniforms_cpu; }
	const Vector<GPUBatch>& GetBatches() const { return batches; }
	const Vector<UInt32>&   GetVisibleIDs() const { return visible_ids_cpu; }
	const Vector<DrawIndexedIndirectArgs>& GetDrawCommands() const { return draw_commands_cpu; }
	const Vector<GPUObjectData>& GetObjects() const { return objects_cpu; }
	UInt32 GetBatchCount() const { return static_cast<UInt32>(batches.size()); }
	UInt32 GetObjectCount() const { return static_cast<UInt32>(objects_cpu.size()); }
	UInt32 GetMaterialCount() const { return static_cast<UInt32>(materials_cpu.size()); }

private:
	void ReleaseBuffers();

	Config config{};

	// CPU mirrors (source of truth for every upload)
	Vector<GPUObjectData>             objects_cpu;
	Vector<GPUMaterialData>           materials_cpu;
	Vector<GPUMeshMeta>               meshes_cpu;
	Vector<UInt32>                    visible_ids_cpu;
	Vector<DrawIndexedIndirectArgs>   draw_commands_cpu;
	Vector<GPUBatch>                  batches;
	GPUSceneUniformsData              uniforms_cpu{};

	// GPU buffers (retained; bound once at init)
	RHI::Buffer* object_buffer   = nullptr;
	RHI::Buffer* material_buffer = nullptr;
	RHI::Buffer* mesh_buffer     = nullptr;
	RHI::Buffer* visible_buffer  = nullptr;
	RHI::Buffer* command_buffer  = nullptr;
	RHI::Buffer* uniform_buffer  = nullptr;

	Bool is_initialized = false;
};

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _GPU_SCENE_
