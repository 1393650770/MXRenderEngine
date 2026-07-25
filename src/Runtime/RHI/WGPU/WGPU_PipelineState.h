#pragma once
#ifndef _WGPU_PIPELINESTATE_
#define _WGPU_PIPELINESTATE_

#if PLATFORM_WGPU

#include "RHI/RenderPipelineState.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_PipelineState, public RenderPipelineState)
#pragma region METHOD
public:
	WGPU_PipelineState() MYDEFAULT;
	WGPU_PipelineState(CONST RenderGraphiPipelineStateDesc& in_desc);
	VIRTUAL ~WGPU_PipelineState() OVERRIDE;

	VIRTUAL void CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource = false) OVERRIDE FINAL;

	void METHOD(SetPipeline)(WGPURenderPipeline pipeline);
	WGPURenderPipeline METHOD(GetPipeline)() CONST { return m_pipeline; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPURenderPipeline m_pipeline = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_PIPELINESTATE_
