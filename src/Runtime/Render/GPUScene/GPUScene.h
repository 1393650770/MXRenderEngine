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

// A stable reference to an object slot.
//
// Index alone is not enough once slots are recycled: the object that used to
// live at index 7 is gone the moment a new object takes slot 7. The generation
// is bumped on every release, so a stale handle fails IsValid() instead of
// silently addressing a different object.
struct GPUObjectHandle
{
	UInt32 index = kInvalidIndex;
	UInt32 generation = 0;
	Bool IsValid() const { return index != kInvalidIndex; }
};

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

	// Allocates a slot, recycling a freed one when available. The scene can
	// therefore be cycled indefinitely as long as the live count stays under
	// max_objects — this is the difference between "the array has capacity" and
	// "the scene can churn".
	GPUObjectHandle AddObject(const GPUObjectData& object);

	// Releases the slot back to the free list and bumps its generation, so any
	// other handle to the same object becomes invalid. Call BuildBatches
	// afterwards to apply it.
	Bool RemoveObject(GPUObjectHandle handle);
	Bool IsObjectAlive(GPUObjectHandle handle) const;

	// ---- CPU-side mutation ----
	// Index-based accessors. The index is the raw slot; use a handle when the
	// slot may be recycled underneath you.
	GPUObjectData&       GetObject(UInt32 slot)       { return objects_cpu[slot]; }
	const GPUObjectData& GetObject(UInt32 slot) const { return objects_cpu[slot]; }
	const GPUObjectData* GetObjectByHandle(GPUObjectHandle handle) const;

	// Marks the slot for upload. Cheap, and UploadObjects only walks what
	// actually changed.
	void MarkObjectDirty(UInt32 slot);
	void SetObjectModel(UInt32 slot, const glm::mat4& model);
	void SetObjectBounds(UInt32 slot, const glm::vec4& sphere_bounds);

	// ---- upload (buffer writes, never descriptor updates) ----
	void UploadObjects();
	void UploadMaterials();
	void UploadMeshes(const MeshPool& pool);
	void UploadVisibleIDs();
	void UploadDrawCommands();
	void UploadUniforms();
	void UploadInstances();
	void UploadBatches();

	// Resets every command's instanceCount to 0. The culling shader then
	// atomically increments it, so this must run before the cull each frame.
	void ResetDrawCommands();

	// ---- batching ----
	// One batch per drawable unit (mesh id). Instances within a batch may use
	// different materials — material is per-instance via objects[].material_id,
	// only the geometry has to match.
	void BuildBatches(const MeshPool& pool);
	void BuildDrawCommands(const MeshPool& pool);

	// Whether DrawIndexedIndirectArgs::firstInstance is honoured by the device
	// (requires the `shaderDrawParameters` feature). When it is, every batch can
	// be issued as ONE indirect command because gl_InstanceIndex then starts at
	// firstInstance, so no push constant is needed to locate a batch's slice.
	//
	// When it is not, the caller must issue one command per batch and pass the
	// slice offset some other way. Both paths produce the same visibleIDs[]
	// layout — only how the shader resolves the instance index differs.
	Bool GetUseFirstInstance() const { return use_first_instance; }
	void SetUseFirstInstance(Bool enable) { use_first_instance = enable; }

	// ---- GPU buffers (bind once) ----
	RHI::Buffer* GetObjectBuffer()   const { return object_buffer; }
	RHI::Buffer* GetMaterialBuffer() const { return material_buffer; }
	RHI::Buffer* GetMeshBuffer()     const { return mesh_buffer; }
	RHI::Buffer* GetVisibleIDBuffer()const { return visible_buffer; }
	RHI::Buffer* GetDrawCommandBuffer() const { return command_buffer; }
	RHI::Buffer* GetUniformBuffer()  const { return uniform_buffer; }
	RHI::Buffer* GetInstanceBuffer() const { return instance_buffer; }
	RHI::Buffer* GetBatchBuffer()    const { return batch_buffer; }

	// ---- CPU-side views (also used by the headless self-test) ----
	GPUSceneUniformsData& GetUniforms() { return uniforms_cpu; }
	const Vector<GPUBatch>& GetBatches() const { return batches; }
	const Vector<UInt32>&   GetVisibleIDs() const { return visible_ids_cpu; }
	const Vector<GPUInstanceData>& GetInstances() const { return instances_cpu; }
	const Vector<DrawIndexedIndirectArgs>& GetDrawCommands() const { return draw_commands_cpu; }
	const Vector<GPUObjectData>& GetObjects() const { return objects_cpu; }
	UInt32 GetBatchCount() const { return static_cast<UInt32>(batches.size()); }
	UInt32 GetObjectCount() const { return static_cast<UInt32>(objects_cpu.size()); }
	// Objects actually submitted for drawing — the number the culling dispatch
	// must cover, since it indexes the instance stream rather than objects[].
	UInt32 GetActiveObjectCount() const { return static_cast<UInt32>(instances_cpu.size()); }
	// Slots ever allocated (live + holes). Slots beyond this are untouched.
	UInt32 GetSlotCount() const { return static_cast<UInt32>(objects_cpu.size()); }
	UInt32 GetFreeSlotCount() const { return static_cast<UInt32>(free_slots.size()); }
	UInt32 GetMaterialCount() const { return static_cast<UInt32>(materials_cpu.size()); }

private:
	void ReleaseBuffers();

	Config config{};

	// CPU mirrors (source of truth for every upload)
	Vector<GPUObjectData>             objects_cpu;
	Vector<GPUMaterialData>           materials_cpu;
	Vector<GPUMeshMeta>               meshes_cpu;
	Vector<UInt32>                    visible_ids_cpu;
	Vector<GPUInstanceData>           instances_cpu;
	Vector<DrawIndexedIndirectArgs>   draw_commands_cpu;
	Vector<GPUBatch>                  batches;
	GPUSceneUniformsData              uniforms_cpu{};

	// Slot bookkeeping. slot_generation[i] is bumped every time slot i is
	// released; free_slots is the recycle stack (LIFO keeps recently freed slots
	// hot in cache, and makes the self-test deterministic).
	Vector<UInt32> slot_generation;
	Vector<UInt32> free_slots;
	Vector<UInt32> dirty_slots;

	// GPU buffers (retained; bound once at init)
	RHI::Buffer* object_buffer   = nullptr;
	RHI::Buffer* material_buffer = nullptr;
	RHI::Buffer* mesh_buffer     = nullptr;
	RHI::Buffer* visible_buffer  = nullptr;
	RHI::Buffer* command_buffer  = nullptr;
	RHI::Buffer* uniform_buffer  = nullptr;
	RHI::Buffer* instance_buffer = nullptr;
	RHI::Buffer* batch_buffer    = nullptr;

	Bool is_initialized = false;
	Bool use_first_instance = false;
};

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _GPU_SCENE_
