#pragma once
#ifndef _WGPU_SRB_
#define _WGPU_SRB_

#if PLATFORM_WGPU

#include "RHI/RenderShader.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

// Minimal SRB for HelloTriangle (no resources bound).
// Phase C/future: implement bind groups for textures/buffers.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_ShaderResourceBinding, public ShaderResourceBinding)
#pragma region METHOD
public:
	WGPU_ShaderResourceBinding() MYDEFAULT;
	VIRTUAL ~WGPU_ShaderResourceBinding() OVERRIDE;

	VIRTUAL void METHOD(SetResource)(CONST String& name, CONST RenderResource* resource) OVERRIDE FINAL {}
	VIRTUAL void METHOD(FlushDescriptorWrites)() OVERRIDE FINAL {}

	WGPUBindGroup METHOD(GetBindGroup)() CONST { return m_bind_group; }
	void METHOD(SetBindGroup)(WGPUBindGroup group);
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUBindGroup m_bind_group = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_SRB_
