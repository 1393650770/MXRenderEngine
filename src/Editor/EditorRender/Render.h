
#pragma once
#ifndef _EDITORRENDERPIPELINE_
#define _EDITORRENDERPIPELINE_

#include <imgui.h>
#include <mutex>

#include "Core/ConstDefine.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderGraph.h"
#include "EditorUI.h"
#include "Render/Core/RenderGraphDefinition.h"
#include "Render/Core/RenderFrameData.h"


MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)
class Window;


MYRENDERER_BEGIN_CLASS_WITH_DERIVE(EditorRenderPipeline, public MXRender::RenderInterface)


#pragma region METHOD
public:
	EditorRenderPipeline(MXRender::PlatformWindow* in_window);
	VIRTUAL ~EditorRenderPipeline() MYDEFAULT;
	EditorRenderPipeline() MYDEFAULT;

	//  Logic thread lifecycle
	VIRTUAL void METHOD(OnInit_Logic)(PlatformWindow* in_window, RHI::Viewport* in_viewport) OVERRIDE;
	VIRTUAL void METHOD(OnShutdown_Logic)() OVERRIDE;

	//  Render thread lifecycle
	VIRTUAL void METHOD(OnInit_Render)() OVERRIDE;
	VIRTUAL void METHOD(OnShutdown_Render)() OVERRIDE;

	//  Logic thread per-frame
	VIRTUAL void METHOD(OnUpdate)(float dt) OVERRIDE;
	VIRTUAL void METHOD(OnPrepareFrameContext)(Render::FrameContext& ctx) OVERRIDE;

	//  Render thread per-frame
	VIRTUAL void METHOD(OnPreRender)(Render::FrameContext& ctx) OVERRIDE;
	VIRTUAL void METHOD(OnRender)() OVERRIDE;
	VIRTUAL void METHOD(OnPostRender)(Render::FrameContext& ctx) OVERRIDE;

	// --  Rebuild runtime graph from editor definition
	void METHOD(RebuildFromDefinition)(CONST MXRender::Render::RenderGraphDefinition& def);

	/// Adds the UIPreviewPass to the graph (no-op if already present). Called
	/// from BOTH OnInit_Render and RebuildFromDefinition — the rebuild path
	/// replaces the graph's passes.
	void EnsureUIPreviewPass(Render::RenderGraph& graph);

	PlatformWindow* METHOD(GetWindow)();
protected:

private:


#pragma endregion

#pragma region MEMBER
public:

protected:
	PlatformWindow* m_window = nullptr;
	RHI::Viewport* m_viewport = nullptr;
	EditorUI editor_ui;

	Bool has_deferred_rebuild = false;
	Bool need_clear_fb = false;
	Render::RenderGraphDefinition deferred_def;
	//  Mutex for deferred_def (Logic writes, Render reads in 3-thread mode)
	std::mutex rebuild_mutex_;

	// --  Pre-created pipeline states for loaded graph passes
	RHI::Shader* skybox_vs = nullptr, *skybox_ps = nullptr;
	RHI::RenderPipelineState* skybox_pipeline = nullptr;
	RHI::ShaderResourceBinding* skybox_srb = nullptr;
	void InitRenderPasses();
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif // !_EDITORRENDERPIPELINE_
