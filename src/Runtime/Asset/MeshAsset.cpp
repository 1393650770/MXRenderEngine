#include "MeshAsset.h"
#include "Tool/MeshLoader.h"
#include "Tool/BufferUtils.h"
#include "RHI/RenderRHI.h"
#include "RHI/ResourceManager.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderBuffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)

MeshAsset::MeshAsset(CONST String& in_path)
{
	LoadMesh(in_path);
}

MeshAsset::~MeshAsset()
{
	ReleaseBuffers();
}

void MeshAsset::ReleaseBuffers()
{
	if (load_future.valid())
		load_future.wait();
	delete vertex_buffer;
	vertex_buffer = nullptr;
	delete index_buffer;
	index_buffer = nullptr;
	delete payload;
	payload = nullptr;
	vertex_count = 0;
	index_count = 0;
	is_loaded = false;
}

void MeshAsset::LoadMesh(CONST String& in_path)
{
	if (payload == nullptr)
		payload = new Tool::MeshDataPayload();
	is_loaded = false;
	// Keep the future as a member: a discarded std::async future blocks in
	// its destructor, degrading the load to synchronous (TextureAsset bug).
	load_future = std::async(std::launch::async, [this, in_path]()
	{
		Tool::MeshLoader::LoadMeshData(in_path, this->payload, this->is_loaded);
	});
}

Bool MeshAsset::IsLoaded() CONST
{
	return is_loaded;
}

void MeshAsset::WaitUntilLoaded()
{
	if (load_future.valid())
		load_future.wait();
}

void MeshAsset::CreateBuffersIfReady()
{
	if (vertex_buffer != nullptr || !is_loaded || payload == nullptr)
		return;
	if (payload->vertices.empty() || payload->indices.empty())
		return;

	vertex_count = (UInt32)payload->vertices.size();
	index_count = (UInt32)payload->indices.size();

	// Dynamic bit -> host-visible persistent mapping -> reliable direct write
	RHI::BufferDesc vb_desc;
	vb_desc.size = vertex_count * Tool::MeshDataPayload::GetVertexStride();
	vb_desc.stride = Tool::MeshDataPayload::GetVertexStride();
	vb_desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
	vertex_buffer_handle = RHICreateBuffer(vb_desc, "MeshVB"); vertex_buffer = Resolve(vertex_buffer_handle);
	Tool::BufferUtils::Upload(vertex_buffer, payload->vertices.data(), vb_desc.size);

	RHI::BufferDesc ib_desc;
	ib_desc.size = index_count * (UInt32)sizeof(UInt32);
	ib_desc.stride = (UInt32)sizeof(UInt32);
	ib_desc.type = ENUM_BUFFER_TYPE::Index | ENUM_BUFFER_TYPE::Dynamic;
	index_buffer_handle = RHICreateBuffer(ib_desc, "MeshIB"); index_buffer = Resolve(index_buffer_handle);
	Tool::BufferUtils::Upload(index_buffer, payload->indices.data(), ib_desc.size);

	// CPU data no longer needed after the GPU upload
	delete payload;
	payload = nullptr;
}

RHI::Buffer* MeshAsset::GetVertexBuffer()
{
	CreateBuffersIfReady();
	return vertex_buffer;
}

RHI::Buffer* MeshAsset::GetIndexBuffer()
{
	CreateBuffersIfReady();
	return index_buffer;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
