#pragma once
#ifndef _WGPU_BUFFER_
#define _WGPU_BUFFER_

#if PLATFORM_WGPU

#include "RHI/RenderBuffer.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_Buffer, public Buffer)
#pragma region METHOD
public:
	WGPU_Buffer(CONST BufferDesc& desc);
	VIRTUAL ~WGPU_Buffer() OVERRIDE;

	VIRTUAL void* METHOD(Map)(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag) OVERRIDE FINAL;
	VIRTUAL void METHOD(Unmap)() OVERRIDE FINAL;

	void METHOD(SetBuffer)(WGPUBuffer buffer, void* mapped_ptr = nullptr);
	WGPUBuffer METHOD(GetBuffer)() CONST { return m_buffer; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUBuffer m_buffer = nullptr;
	void* m_mapped_ptr = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_BUFFER_
