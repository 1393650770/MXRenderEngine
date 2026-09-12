#pragma once
#ifndef _GPU_SCENE_MESH_POOL_
#define _GPU_SCENE_MESH_POOL_

#include "Core/ConstDefine.h"
#include "Render/GPUScene/GPUSceneData.h"
#include "Tool/MeshLoader.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

// Merges every loaded mesh into ONE vertex buffer and ONE index buffer, so a
// single indirect command can address any drawable unit via firstIndex/indexCount.
//
// This is the prerequisite GPU Scene cannot skip: while each mesh owns its own
// buffers, per-mesh binding is unavoidable — which is exactly what GPU Scene
// exists to remove.
//
// Indices are rebased into pooled space (index += baseVertex) so every batch
// keeps vertexOffset = 0.
class MeshPool
{
public:
	MeshPool() = default;
	~MeshPool() = default;

	// Append one loaded mesh. Returns one mesh id per submesh
	// (single-material models return exactly one entry).
	Vector<UInt32> AddMesh(const Tool::MeshDataPayload& payload);

	// Allocate + fill the merged buffers. Call once, after the last AddMesh.
	Bool Upload();
	void Release();

	RHI::Buffer* GetVertexBuffer() const { return vertex_buffer; }
	RHI::Buffer* GetIndexBuffer()  const { return index_buffer; }
	const Vector<GPUMeshMeta>& GetMeshMetas() const { return mesh_metas; }
	UInt32 GetMeshCount() const { return static_cast<UInt32>(mesh_metas.size()); }
	UInt32 GetVertexCount() const { return static_cast<UInt32>(vertices.size()); }
	UInt32 GetIndexCount() const { return static_cast<UInt32>(indices.size()); }
	static UInt32 GetVertexStride() { return Tool::MeshDataPayload::GetVertexStride(); }

	// Raw CPU-side data. Exposed so the headless self-test can verify the
	// index rebase math without ever creating a GPU device.
	const Vector<Tool::MeshVertex>& GetVertices() const { return vertices; }
	const Vector<UInt32>& GetIndices() const { return indices; }

private:
	Vector<Tool::MeshVertex> vertices;
	Vector<UInt32>           indices;
	Vector<GPUMeshMeta>      mesh_metas;
	RHI::Buffer*             vertex_buffer = nullptr;
	RHI::Buffer*             index_buffer = nullptr;
};

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _GPU_SCENE_MESH_POOL_
