#pragma once
#ifndef _RENDERPIPELINESTATE_
#define _RENDERPIPELINESTATE_
#include "RenderEnum.h"
#include "RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class ShaderResourceBinding;
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderPipelineState, public RenderResource)
#pragma region METHOD
public:
	RenderPipelineState() MYDEFAULT;
	RenderPipelineState(CONST RenderGraphiPipelineStateDesc& in_desc);
	VIRTUAL ~RenderPipelineState();
	VIRTUAL void CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource = false) PURE;
protected:
private:

#pragma endregion

#pragma region MEMBER
public:
protected:
	RenderGraphiPipelineStateDesc desc;
private:

#pragma endregion
MYRENDERER_END_CLASS

// --  
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(ComputePipelineState, public RenderResource)
#pragma region METHOD
public:
    ComputePipelineState() MYDEFAULT;
    ComputePipelineState(CONST ComputePipelineStateDesc& in_desc);
    VIRTUAL ~ComputePipelineState();
    VIRTUAL void CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource = false) PURE;
    CONST ComputePipelineStateDesc& GetDesc() CONST { return desc; }
protected:
private:

#pragma endregion

#pragma region MEMBER
public:
protected:
    ComputePipelineStateDesc desc;
private:

#pragma endregion
MYRENDERER_END_CLASS
// --  

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif //_RENDERSTATE_
