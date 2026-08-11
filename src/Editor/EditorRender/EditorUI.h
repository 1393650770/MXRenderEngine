#pragma once
#ifndef _EDITORUI_
#define _EDITORUI_

#include <imgui.h>

#include "Core/ConstDefine.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderGraph.h"
#include "UI/UIBase.h"



MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class BasePanel;
class RenderGraphPanel;
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(Application)
class Window;

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(EditorUI,public UI::UIBase)


#pragma region METHOD
public:
	VIRTUAL ~EditorUI() MYDEFAULT;
	EditorUI() MYDEFAULT;

	void METHOD(Init)(PlatformWindow* in_window, RHI::Viewport* in_viewport);
	//  Logic thread: ImGui NewFrame + widgets + Render → returns ImDrawData
	ImDrawData* METHOD(DrawFrame_Logic)();
	//  Render thread: record ImGui GPU commands
	void METHOD(DrawFrame_Render)(ImDrawData* draw_data, RHI::CommandList* cmd);
	void METHOD(Release)();

	// RenderGraph bridge
	void METHOD(SetRenderGraph)(Render::RenderGraph* g) { graph_ptr = g; }
	Render::RenderGraph* METHOD(GetRenderGraph)() { return graph_ptr; }

	// Panel access (for OutlinePanel etc.)
	UI::RenderGraphPanel* METHOD(GetRenderGraphPanel)();

	/// ImGui texture id for an RHI texture (lazily creates a Vulkan descriptor
	/// set via ImGui_ImplVulkan_AddTexture; cached, freed in Release()).
	/// Lives in EditorUI.cpp — the sanctioned Vulkan-include exception file.
	static ImTextureID METHOD(GetPreviewTextureId)(RHI::Texture* texture);

protected:
	void AddPanelUI(CONST String& name);
	void AddPanelUI(UI::BasePanel* in_panel);
	void OpenRanelUI(CONST String& name);
private:


#pragma endregion

#pragma region MEMBER
public:
	Bool show_editor = true; // -- 
	PlatformWindow* m_window = nullptr;
	RHI::Viewport* m_viewport = nullptr;
protected:
	Vector<UI::BasePanel*> panels;
	Render::RenderGraph* graph_ptr = nullptr;
	UI::RenderGraphPanel* rg_panel = nullptr; // cached reference
private:
	static Map<RHI::Texture*, ImTextureID> preview_texture_cache;
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif // !_EDITORUI_
