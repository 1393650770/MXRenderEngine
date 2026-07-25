#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_PipelineState.h"
#include "RHI/WGPU/WGPU_ShaderResourceBinding.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_PipelineState::WGPU_PipelineState(CONST RenderGraphiPipelineStateDesc& in_desc)
	: RenderPipelineState(in_desc)
{
}

WGPU_PipelineState::~WGPU_PipelineState()
{
	if (m_pipeline) { wgpuRenderPipelineRelease(m_pipeline); m_pipeline = nullptr; }
}

void WGPU_PipelineState::CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource)
{
	// HelloTriangle: empty bind group (no resources).
	// Future: create WGPUBindGroup from the pipeline layout's bind group layouts.
	out_srb = new WGPU_ShaderResourceBinding();
}

void WGPU_PipelineState::SetPipeline(WGPURenderPipeline pipeline)
{
	if (m_pipeline) wgpuRenderPipelineRelease(m_pipeline);
	m_pipeline = pipeline;
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
