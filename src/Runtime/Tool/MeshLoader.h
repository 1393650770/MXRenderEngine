#pragma once
#ifndef _MESHLOADER_
#define _MESHLOADER_
#include "Core/ConstDefine.h"
#include "RHI/RenderEnum.h"
#include "RHI/RenderResource.h"
#include <atomic>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Tool)

// Interleaved vertex format produced by MeshLoader: pos + normal + uv, 32 bytes.
// No tangent/color yet - add fields (and a layout entry) when normal mapping
// arrives; octahedral-packed normals were deliberately not adopted.
MYRENDERER_BEGIN_STRUCT(MeshVertex)
public:
	Float32 position[3] = { 0.0f, 0.0f, 0.0f };
	Float32 normal[3] = { 0.0f, 0.0f, 0.0f };
	Float32 uv[2] = { 0.0f, 0.0f };
MYRENDERER_END_STRUCT

// CPU-side mesh blob (loader output, MeshAsset input) - the mesh counterpart
// of RHI::TextureDataPayload, but owned by the Tool layer because the RHI
// never consumes it directly (buffer creation only needs size/stride).
MYRENDERER_BEGIN_STRUCT(MeshDataPayload)
public:
	MYRENDERER_BEGIN_STRUCT(SubMesh)
	public:
		UInt32 index_offset = 0;
		UInt32 index_count = 0;
	MYRENDERER_END_STRUCT

	Vector<MeshVertex> vertices;
	Vector<UInt32> indices;               // always 32-bit (SetIndexBuffer index32=true)
	Vector<SubMesh> sub_meshes;           // one entry per material slot; single-material models have exactly one
	Float32 bounds_min[3] = { 0.0f, 0.0f, 0.0f };
	Float32 bounds_max[3] = { 0.0f, 0.0f, 0.0f };

	// PSO vertex input layout matching MeshVertex:
	// binding 0, PerVertex: loc0 pos RGB32F off0 / loc1 normal RGB32F off12 / loc2 uv RG32F off24
	static Vector<RHI::VertexInputLayout> METHOD(GetVertexInputLayout)();
	static UInt32 METHOD(GetVertexStride)();
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS(MeshLoader)
#pragma region METHOD
public:
	// Load a model file (obj/gltf/fbx/... - anything assimp reads) into the
	// payload. Node hierarchies are pre-transformed (baked) into one flat
	// vertex/index pair. Returns false on failure (missing asset is a
	// recoverable error - no CHECK/abort). Keep paths ASCII.
	static Bool METHOD(LoadMeshData)(CONST String& in_filename, MeshDataPayload& out_payload);
	// Async variant (same shape as TextureLoader::LoadTextureData).
	static void METHOD(LoadMeshData)(CONST String& in_filename, MeshDataPayload* out_payload, std::atomic_bool& out_result);
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:

private:

#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif
