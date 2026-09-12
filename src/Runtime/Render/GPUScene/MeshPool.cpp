#include "Render/GPUScene/MeshPool.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderBuffer.h"
#include "Tool/BufferUtils.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

Vector<UInt32> MeshPool::AddMesh(const Tool::MeshDataPayload& payload)
{
	Vector<UInt32> ids;
	if (payload.vertices.empty() || payload.indices.empty())
		return ids;

	const UInt32 base_vertex = static_cast<UInt32>(vertices.size());
	const UInt32 base_index  = static_cast<UInt32>(indices.size());

	vertices.insert(vertices.end(), payload.vertices.begin(), payload.vertices.end());

	// Rebase into pooled space. Doing it here (instead of using the indirect
	// command's vertexOffset) keeps every batch's vertexOffset at 0 and lets one
	// vertex buffer bind serve the whole scene.
	indices.reserve(indices.size() + payload.indices.size());
	for (UInt32 idx : payload.indices)
		indices.push_back(idx + base_vertex);

	// Model-space bounds sphere. Stored untransformed: the culling shader
	// transforms it by obj.model, which keeps per-frame CPU work at zero when an
	// object moves (only the 64-byte model matrix needs rewriting).
	const glm::vec3 bmin(payload.bounds_min[0], payload.bounds_min[1], payload.bounds_min[2]);
	const glm::vec3 bmax(payload.bounds_max[0], payload.bounds_max[1], payload.bounds_max[2]);
	const glm::vec3 center = (bmin + bmax) * 0.5f;
	const Float32   radius = glm::length(bmax - bmin) * 0.5f;

	const UInt32 submesh_count = payload.sub_meshes.empty()
		? 1u
		: static_cast<UInt32>(payload.sub_meshes.size());

	for (UInt32 s = 0; s < submesh_count; ++s)
	{
		GPUMeshMeta meta{};
		if (payload.sub_meshes.empty())
		{
			meta.index_count = static_cast<UInt32>(payload.indices.size());
			meta.first_index = base_index;
		}
		else
		{
			meta.index_count = payload.sub_meshes[s].index_count;
			meta.first_index = base_index + payload.sub_meshes[s].index_offset;
		}
		meta.vertex_offset = 0;   // indices already rebased
		meta.lod_count     = 1;
		meta.bounds        = glm::vec4(center, radius);
		meta.lod_offsets[0] = static_cast<UInt32>(mesh_metas.size());

		ids.push_back(static_cast<UInt32>(mesh_metas.size()));
		mesh_metas.push_back(meta);
	}
	return ids;
}

Bool MeshPool::Upload()
{
	ENSURE(!vertices.empty(), "MeshPool: Upload() called with no vertices");
	ENSURE(!indices.empty(), "MeshPool: Upload() called with no indices");
	if (vertices.empty() || indices.empty())
		return false;

	Release();

	const UInt32 stride = GetVertexStride();

	// Vertex|Dynamic / Index|Dynamic -> host-visible persistently mapped memory.
	// A plain Vertex buffer is device-local and its Map silently takes the
	// unreliable staging+TRANSFER path (see CLAUDE.md RHI Gotchas).
	RHI::BufferDesc vb_desc;
	vb_desc.size   = static_cast<UInt32>(vertices.size() * stride);
	vb_desc.stride = stride;
	vb_desc.type   = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
	vertex_buffer  = g_render_rhi->CreateBuffer(vb_desc);
	ENSURE(vertex_buffer != nullptr, "MeshPool: failed to create merged vertex buffer");
	if (vertex_buffer)
		Tool::BufferUtils::Upload(vertex_buffer, vertices.data(), vb_desc.size);

	RHI::BufferDesc ib_desc;
	ib_desc.size   = static_cast<UInt32>(indices.size() * sizeof(UInt32));
	ib_desc.stride = sizeof(UInt32);
	ib_desc.type   = ENUM_BUFFER_TYPE::Index | ENUM_BUFFER_TYPE::Dynamic;
	index_buffer   = g_render_rhi->CreateBuffer(ib_desc);
	ENSURE(index_buffer != nullptr, "MeshPool: failed to create merged index buffer");
	if (index_buffer)
		Tool::BufferUtils::Upload(index_buffer, indices.data(), ib_desc.size);

	return vertex_buffer != nullptr && index_buffer != nullptr;
}

void MeshPool::Release()
{
	delete vertex_buffer;
	vertex_buffer = nullptr;
	delete index_buffer;
	index_buffer = nullptr;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
