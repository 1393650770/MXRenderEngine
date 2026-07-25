#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_Buffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_Buffer::WGPU_Buffer(CONST BufferDesc& desc)
	: Buffer(desc)
{
}

WGPU_Buffer::~WGPU_Buffer()
{
	if (m_buffer) { wgpuBufferRelease(m_buffer); m_buffer = nullptr; }
	m_mapped_ptr = nullptr;
}

void* WGPU_Buffer::Map(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag)
{
	// Phase C: wgpuBufferMapAsync + wgpuBufferGetMappedRange
	// For now, return persisted mapped pointer if available (Dynamic buffers)
	return m_mapped_ptr;
}

void WGPU_Buffer::Unmap()
{
	// Phase C: wgpuBufferUnmap
}

void WGPU_Buffer::SetBuffer(WGPUBuffer buffer, void* mapped_ptr)
{
	if (m_buffer) wgpuBufferRelease(m_buffer);
	m_buffer = buffer;
	m_mapped_ptr = mapped_ptr;
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
