#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_ShaderResourceBinding.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_ShaderResourceBinding::WGPU_ShaderResourceBinding() MYDEFAULT;

WGPU_ShaderResourceBinding::~WGPU_ShaderResourceBinding()
{
	if (m_bind_group) { wgpuBindGroupRelease(m_bind_group); m_bind_group = nullptr; }
}

void WGPU_ShaderResourceBinding::SetBindGroup(WGPUBindGroup group)
{
	if (m_bind_group) wgpuBindGroupRelease(m_bind_group);
	m_bind_group = group;
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
