#pragma once
#ifndef _GPU_SCENE_DATA_
#define _GPU_SCENE_DATA_

// GPU Scene data contract shared by C++ and GLSL.
//
// Every struct below has a byte-for-byte mirror in the shaders. The static_asserts
// at the bottom are the guard: if a field is added/reordered on either side the
// build breaks instead of silently rendering garbage.
//
// Layout rules: std430 storage buffers, NOT std140 uniform blocks.
//   - std140 forces array stride to 16 bytes (a vec3 becomes a vec4), std430 does not.
//   - the engine's Storage buffers lack UNIFORM usage anyway (see CLAUDE.md RHI Gotchas),
//     so `readonly buffer` is the only option for shader parameters.
//
// Include order: Render/View/SceneView.h comes first because it establishes the
// engine-wide GLM conventions (0..1 depth range + Y-flip baked into projection).

#include "Render/View/SceneView.h"
#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

// ---- Capacity limits -------------------------------------------------------
// Fixed at Initialize(); all GPU Scene buffers are retained (never rebound), so
// they must be allocated once with headroom. Growing later means a full rebind.
constexpr UInt32 kInvalidIndex = 0xFFFFFFFFu;
constexpr UInt32 kMaxObjects   = 1u << 16;   // 65536 objects  = 6 MB
constexpr UInt32 kMaxMaterials = 1u << 12;   // 4096 materials = 192 KB
constexpr UInt32 kMaxMeshes    = 1u << 12;   // 4096 drawable units
constexpr UInt32 kMaxBatches   = 1u << 14;   // 16384 indirect commands

// ---- Per-object data (96 bytes) --------------------------------------------
// Mirrors `struct GPUObjectData` in GPUScene.glsl.
// This is the layer that REPLACES per-object descriptor binding: an object's
// parameters are read out of this array by index instead of being bound.
struct GPUObjectData
{
	glm::mat4 model{ 1.0f };                                // 64
	glm::vec4 sphere_bounds{ 0.0f, 0.0f, 0.0f, 1.0f };      // 16  xyz = center (model space), w = radius
	UInt32 material_id = kInvalidIndex;                     // 4   index into materials[]
	UInt32 mesh_id = kInvalidIndex;                         // 4   index into meshes[]
	UInt32 flags = 0;                                       // 4
	Float32 lod_bias = 0.0f;                                // 4
};

// ---- Per-material data (48 bytes) ------------------------------------------
// Mirrors `struct GPUMaterialData` in GPUScene.glsl.
// Texture references are bindless indices (set 2), never descriptors: switching
// material costs one integer compare, not a vkCmdBindDescriptorSets.
struct GPUMaterialData
{
	UInt32 base_color_idx = kInvalidIndex;                  // 4
	UInt32 normal_idx = kInvalidIndex;                      // 4
	UInt32 aorm_idx = kInvalidIndex;                        // 4
	UInt32 flags = 0;                                       // 4
	glm::vec4 base_color_factor{ 1.0f, 1.0f, 1.0f, 1.0f };  // 16
	Float32 metallic = 0.0f;                                // 4
	Float32 roughness = 0.5f;                               // 4
	Float32 alpha_cutoff = 0.0f;                             // 4
	UInt32 shader_id = 0;                                   // 4   batch grouping key
};

// ---- Per-drawable-unit data (48 bytes) -------------------------------------
// Mirrors `struct GPUMeshMeta` in GPUScene.glsl.
// One entry per submesh: the geometry extent of a single indirect command.
struct GPUMeshMeta
{
	UInt32 index_count = 0;                                 // 4
	UInt32 first_index = 0;                                 // 4   pre-rebased into the merged pool
	Int    vertex_offset = 0;                               // 4   always 0 (indices are rebased)
	UInt32 lod_count = 1;                                   // 4
	glm::vec4 bounds{ 0.0f, 0.0f, 0.0f, 1.0f };             // 16  model-space bounds sphere
	UInt32 lod_offsets[4] = { 0, 0, 0, 0 };                 // 16  reserved for LOD (stage 5)
};

// ---- Object flags -----------------------------------------------------------
// Removal is a soft delete: the slot keeps its id (so external references stay
// valid) and is simply skipped when batches are rebuilt. Actually reclaiming
// the slot needs a free list plus generation handles — not worth it until
// scenes churn hard enough to hit kMaxObjects.
constexpr UInt32 kObjectFlagDeleted = 1u << 0;

// ---- The culling shader's input stream (8 bytes) ---------------------------
// Mirrors `struct GPUInstanceData` in GPUScene.glsl.
// One entry per object, built at registration time; only changes when objects
// are added or removed. Lets the culling shader know both which object it is
// looking at and which batch's slice it belongs to.
struct GPUInstanceData
{
	UInt32 object_id = kInvalidIndex;   // 4
	UInt32 batch_id = kInvalidIndex;    // 4
};

// ---- Per-scene + per-camera uniforms (288 bytes) ---------------------------
// Mirrors `struct GPUSceneUniformsData` in GPUScene.glsl.
// PerScene and PerCamera live in one buffer: both update once per view, and the
// engine has no per-frame descriptor pool yet (stage 4), so fewer buffers is safer.
struct GPUSceneUniformsData
{
	glm::mat4 view_proj{ 1.0f };                            // 64   @0
	glm::mat4 view{ 1.0f };                                 // 64   @64
	glm::vec4 camera_position{ 0.0f, 0.0f, 0.0f, 1.0f };    // 16   @128
	glm::vec4 light_dir{ 0.4f, 0.8f, 0.45f, 0.0f };         // 16   @144
	glm::vec4 frustum_planes[6]{};                          // 96   @160
	glm::vec4 hiz_and_depth{ 0.0f, 0.0f, 0.1f, 100.0f };    // 16   @256  xy = hiz size, z = znear, w = zfar
	glm::uvec4 counts{ 0u, 0u, 0u, 0u };                    // 16   @272  x = object count, y = frame index, z = culling, w = occlusion
};

// ---- CPU-side batch descriptor ---------------------------------------------
// A batch is one indirect command: a group of instances sharing the same geometry.
// Material may vary per instance (objects[].material_id) — only geometry must match.
struct GPUBatch
{
	UInt32 mesh_id = kInvalidIndex;      // which drawable unit
	UInt32 base_instance = 0;            // start of this batch's slice in visibleIDs[]
	UInt32 instance_count = 0;           // instances currently in the slice
	UInt32 max_instance_count = 0;       // reserved capacity of the slice
};

// ---- Layout contract -------------------------------------------------------
// sizeof is what matters: it becomes the array stride on the GLSL side, so a
// mismatch silently shifts every subsequent element.
//
// alignof is deliberately NOT asserted. glm only guarantees 16-byte alignment
// for mat4 when GLM_FORCE_ALIGNED is set, and the engine does not set it.
// That is fine — array elements are packed at sizeof() stride either way, and
// std430's alignment rules are applied by the GLSL compiler, not by us.
static_assert(sizeof(GPUObjectData) == 96, "GPUObjectData must be 96 bytes (std430)");
static_assert(sizeof(GPUMaterialData) == 48, "GPUMaterialData must be 48 bytes (std430)");
static_assert(sizeof(GPUMeshMeta) == 48, "GPUMeshMeta must be 48 bytes (std430)");
static_assert(sizeof(GPUSceneUniformsData) == 288, "GPUSceneUniformsData must be 288 bytes (std430)");
static_assert(sizeof(GPUInstanceData) == 8, "GPUInstanceData must be 8 bytes (std430)");
static_assert(sizeof(GPUBatch) == 16, "GPUBatch must be 16 bytes (std430)");

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _GPU_SCENE_DATA_
