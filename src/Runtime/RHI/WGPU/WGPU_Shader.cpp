#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_Shader.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_Shader::WGPU_Shader(CONST ShaderDesc& desc)
	: Shader(desc)
{
}

WGPU_Shader::WGPU_Shader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
	: Shader(desc, data)
{
}

WGPU_Shader::~WGPU_Shader()
{
	if (m_module) { wgpuShaderModuleRelease(m_module); m_module = nullptr; }
}

void WGPU_Shader::SetModule(WGPUShaderModule module)
{
	m_module = module;
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
