#include "BufferUtils.h"
#include <cstring>
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderBuffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Tool)

RHI::Buffer* BufferUtils::CreateStorageBuffer(UInt32 in_size_bytes, UInt32 in_stride, ENUM_BUFFER_TYPE in_extra_flags)
{
	RHI::BufferDesc desc;
	desc.size = in_size_bytes;
	desc.stride = in_stride;
	desc.type = ENUM_BUFFER_TYPE::Storage | in_extra_flags;
	return g_render_rhi->CreateBuffer(desc);
}

RHI::Buffer* BufferUtils::CreateDynamicParamBuffer(UInt32 in_size_bytes)
{
	RHI::BufferDesc desc;
	desc.size = in_size_bytes;
	desc.stride = in_size_bytes;
	desc.type = ENUM_BUFFER_TYPE::Storage | ENUM_BUFFER_TYPE::Dynamic;
	return g_render_rhi->CreateBuffer(desc);
}

void BufferUtils::Upload(RHI::Buffer* in_buffer, CONST void* in_data, UInt32 in_size_bytes, UInt32 in_dst_offset)
{
	// Per-frame uploads on pre-bound buffers use Map(None) to keep the same
	// sub-allocation offset (UE PersistentMapping pattern). The descriptor was
	// written once at init — changing the offset would make it stale.
	// In record mode, MapBuffer returns shadow memory (offset unchanged anyway).
	void* mapped = g_render_rhi->MapBuffer(in_buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memcpy((UInt8*)mapped + in_dst_offset, in_data, in_size_bytes);
	g_render_rhi->UnmapBuffer(in_buffer);
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
