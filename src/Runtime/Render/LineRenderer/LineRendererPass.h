#pragma once
#ifndef _LINERENDERER_PASS_
#define _LINERENDERER_PASS_

// LineRendererPass: batch-renders every LineRendererComponent trail
// (UIRegisterPass-style free function). Reads the current frame slot from
// LineRendererManager (value-copy buffer, render-thread read-only) -> CPU
// expands segments into quads (Vulkan wide lines are unreliable) -> shared
// dynamic VB single Upload -> firstVertex-offset batched draws.
//
// Register between PlayerPass and the UI pass (RDG timeline = registration
// order; backbuffer overlay order: sim -> display -> player -> line -> UI).

#include "Core/ConstDefine.h"
#include "Render/Core/RenderGraph.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class CommandList;   // forward-declare the ENGINE RHI namespace (MXRender::RHI)
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(Render)
class RenderGraph;

// Free function (UIRegisterPass pattern) - register between PlayerPass and
// the UI pass.
void RegisterLinePass(
	RenderGraph* graph,
	RenderGraphResource<MXRender::RHI::TextureDesc, MXRender::RHI::Texture>* bb_resource,
	MXRender::RHI::CommandList* cmd_list);
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // _LINERENDERER_PASS_
