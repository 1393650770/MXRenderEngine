#pragma once
#ifndef _WGPU_SHADER_
#define _WGPU_SHADER_

#if PLATFORM_WGPU

#include "RHI/RenderShader.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_Shader, public Shader)
#pragma region METHOD
public:
	WGPU_Shader(CONST ShaderDesc& desc);
	WGPU_Shader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data);
	VIRTUAL ~WGPU_Shader() OVERRIDE;

	void METHOD(SetModule)(WGPUShaderModule module);
	WGPUShaderModule METHOD(GetModule)() CONST { return m_module; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUShaderModule m_module = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_SHADER_
