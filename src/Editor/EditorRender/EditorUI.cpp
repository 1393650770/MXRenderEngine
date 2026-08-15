#include "EditorUI.h"
#include "Application/Window.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderTexture.h"
#include "RHI/Vulkan/VK_CommandBuffer.h"
#define  GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>
#include <iostream>
#include "Render/Core/CommandQueue.h"
#include "UI/BasePanel.h"
#include "UI/RenderGraphEditor/Panels/RenderGraphPanel.h"
#include "UI/RenderGraphEditor/Panels/PropertiesPanel.h"
#include "UI/RenderGraphEditor/Panels/OutlinePanel.h"
#include "UI/UIPreviewPanel/UIPreviewPanel.h"
#include "UI/UIDesigner/UIPalette.h"
#include "UI/UIDesigner/UIDesignerState.h"
#include "UI/UIManager.h"
#include "UI/UIDesigner/UIPropertyPanel.h"
#include "RHI/Vulkan/VK_Texture.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

using namespace Render;
using namespace RHI;
using namespace UI;

Map<RHI::Texture*, ImTextureID> EditorUI::preview_texture_cache;
static std::mutex g_preview_tex_mutex;   // logic-thread reads vs render-thread writes

// Dock layout state: built once on first run (or after Window > Reset Layout).
static bool s_layout_ready = false;
static bool s_reset_layout = false;

/// Commercial-engine style default dock layout (Unity-ish):
///   ┌────────┬───────────────────────┬──────────────┐
///   │ Outline│ TabBar: RenderGraph / │ Properties   │
///   │        │        UI Preview     ├──────────────┤
///   │        │                       │ UI Properties│
///   ├────────┴───────────────────────┼──────────────┤
///   │            (central)           │ UI Palette   │
///   └────────────────────────────────┴──────────────┘
/// All panels start docked; the user can drag them anywhere (ini persists).
static void BuildDefaultDockLayout(ImGuiID dockspace_id, const ImVec2& size)
{
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, size);

	ImGuiID main = dockspace_id;
	ImGuiID left, right;
	ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.22f, &left, &main);
	ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.30f, &right, &main);

	// Right column: inspector (Properties over UI Properties), palette below.
	ImGuiID right_top, palette;
	ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.38f, &palette, &right_top);
	ImGuiID props, ui_props;
	ImGui::DockBuilderSplitNode(right_top, ImGuiDir_Down, 0.5f, &ui_props, &props);

	// Central tabbed workspace: both editors share the tab bar.
	ImGui::DockBuilderDockWindow("RenderGraphPanel", main);
	ImGui::DockBuilderDockWindow("UI Preview", main);

	ImGui::DockBuilderDockWindow("OutlinePanel", left);
	ImGui::DockBuilderDockWindow("PropertiesPanel", props);
	ImGui::DockBuilderDockWindow("UI Properties", ui_props);
	ImGui::DockBuilderDockWindow("UI Palette", palette);

	ImGui::DockBuilderFinish(dockspace_id);
}
void EditorUI::Init(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window;
	m_viewport = in_viewport;   // was never assigned — DrawFrame_Render dereferenced null
	IMGUI_CHECKVERSION();
	ImGuiContext* context = ImGui::CreateContext();
	CHECK_WITH_LOG(context ==nullptr,"Failed to create ImGui context!");
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	// Multi-viewport is DISABLED: UpdatePlatformWindows/RenderPlatformWindowsDefault
		// crash on this editor's GLFW/3-thread setup (platform-window lifecycle vs
		// the logic thread's frame loop). The editor is a single-window tool.
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleViewports;
	io.ConfigFlags |= ImGuiConfigFlags_DpiEnableScaleFonts;
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	io.Fonts->AddFontDefault();
	io.Fonts->Build();
	//ImFont* font1 = io.Fonts->AddFontDefault();

	GLFWwindow* glfw_window = (GLFWwindow*)in_window->GetNativeHandle();

	CHECK_WITH_LOG(ImGui_ImplGlfw_InitForVulkan(glfw_window, true) == false, "Failed to init ImGui for Vulkan!");
	in_viewport->AttachUiLayer(this);

	// Register all panels (DockSpace will auto-arrange them)
	AddPanelUI(RenderGraphPanel::GetTypeName());
	AddPanelUI(PropertiesPanel::GetTypeName());
	AddPanelUI(OutlinePanel::GetTypeName());
	// Host viewport FIRST: in Single mode the panel's init command executes
	// INLINE during AddPanelUI (command queue bypass) and needs the viewport.
	UIPreviewPanel::SetHostViewport(in_viewport);
	AddPanelUI(UIPreviewPanel::GetTypeName());
	// UI designer panels (palette drag-source + whitelist-driven property editor).
	AddPanelUI(UIPalettePanel::GetTypeName());
	AddPanelUI(UIPropertyPanel::GetTypeName());

	// Cache the RenderGraphPanel reference and wire up data sources
	for (auto* p : panels)
	{
		if (auto* rg = dynamic_cast<UI::RenderGraphPanel*>(p))
		{
			rg_panel = rg;
		}
	}
	// Wire OutlinePanel to access RenderGraphPanel's node list
	if (rg_panel)
	{
		for (auto* p : panels)
		{
			if (auto* outline = dynamic_cast<UI::OutlinePanel*>(p))
				outline->SetDataSource(rg_panel);
			if (auto* props = dynamic_cast<UI::PropertiesPanel*>(p))
				props->SetDataSource(rg_panel);
		}
	}
}

//  Logic thread: ImGui NewFrame + widgets + Render → returns ImDrawData
ImDrawData* EditorUI::DrawFrame_Logic()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (show_editor)
	{
		// The editor IS the main tool window — anchor it to the viewport
		// origin. (Offsetting it right of the main viewport puts the whole UI
		// off-screen once the OS window gets repositioned; a stale ini then
		// keeps the panel layout out of reach.)
		ImVec2 editor_pos = ImVec2(0, 0);
		ImVec2 editor_size = ImVec2(1280, 960);

		ImGui::SetNextWindowPos(editor_pos, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(editor_size, ImGuiCond_FirstUseEver);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGuiWindowFlags editor_flags = ImGuiWindowFlags_NoDocking;

		Bool editor_open = true;
		ImGui::Begin("MXRender Editor", &editor_open, editor_flags);
		ImGui::PopStyleVar(2);

		ImGuiIO& io = ImGui::GetIO();

		// ---- Main menu bar (tool-agnostic: window visibility + layout) ----
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
					editor_open = false;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window"))
			{
				for (auto* p : panels)
					ImGui::MenuItem(p->GetName().c_str(), nullptr, &p->is_show);
				ImGui::Separator();
				if (ImGui::MenuItem("Reset Layout"))
					s_reset_layout = true;
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help"))
			{
				ImGui::TextDisabled("MXRender Editor");
				ImGui::TextDisabled("RenderGraph + UI Designer");
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		// ---- Toolbar (quick actions, engine-style) ----
		const float toolbar_h = 34.0f;
		const float status_h = 26.0f;
		const float avail_h = ImGui::GetContentRegionAvail().y;
		ImGui::BeginChild("##Toolbar", ImVec2(0, toolbar_h), false, ImGuiWindowFlags_NoScrollbar);
		{
			if (ImGui::Button("Reload UI"))
				UIManager::Get().ReloadAllDocuments();
			ImGui::SameLine();
			if (ImGui::Button("F8 Debugger"))
				UIManager::Get().ToggleDebugger();
			ImGui::SameLine();
			if (ImGui::Button("Reset Layout"))
				s_reset_layout = true;
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			ImGui::TextDisabled("RenderGraph Editor | UI Designer — drag tabs to reorganize");
		}
		ImGui::EndChild();
		ImGui::Separator();

		// ---- Central workspace (dock host) ----
		ImGui::BeginChild("##DockHost", ImVec2(0, avail_h - toolbar_h - status_h), false, ImGuiWindowFlags_NoScrollbar);
		{
			ImGuiID dockspace_id = ImGui::GetID("EditorDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);

			// ---- Default dock layout (first run / manual reset) ----
			// Commercial-engine style: central tabbed workspace (RenderGraph <->
			// UI Designer), left hierarchy, right inspector, bottom-right tools.
			// Built via DockBuilder ONLY when the dock has no layout yet (fresh
			// ini); otherwise the imgui.ini owns the layout and the user's
			// re-docks persist. Window > Reset Layout forces a rebuild.
			if (!s_layout_ready || s_reset_layout)
			{
				s_layout_ready = true;
				if (s_reset_layout || ImGui::DockBuilderGetNode(dockspace_id) == nullptr
					|| ImGui::DockBuilderGetNode(dockspace_id)->ChildNodes[0] == nullptr)
				{
					s_reset_layout = false;
					BuildDefaultDockLayout(dockspace_id, ImVec2(1280, 960));
				}
				else
				{
					s_reset_layout = false;
				}
			}
		}
		ImGui::EndChild();
		ImGui::Separator();

		// ---- Status bar (bottom, engine-style) ----
		ImGui::BeginChild("##StatusBar", ImVec2(0, status_h), false, ImGuiWindowFlags_NoScrollbar);
		{
			const String sel = UI::UIDesignerState::Get().GetSelection();
			ImGui::Text("FPS: %.1f (%.1f ms)", io.Framerate, 1000.0f / io.Framerate);
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			ImGui::Text("UI Preview: %s", UI::UIPreviewPanel::IsReady() ? "ready" : "initializing");
			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();
			ImGui::Text("Selected: %s", sel.empty() ? "-" : sel.c_str());
		}
		ImGui::EndChild();

		for (auto& panel : panels)
		{
			panel->Update();   // logic-thread per-frame tick (UIManager::Update etc.)
			panel->Draw();
		}

		ImGui::End();

		if (!editor_open) show_editor = false;
	}

	ImGui::Render();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	return ImGui::GetDrawData();
}

//  Render thread: record ImGui GPU commands to CommandList
void EditorUI::DrawFrame_Render(ImDrawData* draw_data, RHI::CommandList* cmd)
{
	if (!draw_data || !cmd) return;

	Vector<RHI::Texture*> rtvs;
	rtvs = { m_viewport->GetCurrentBackBufferRTV() };
	cmd->SetRenderTarget(rtvs, nullptr, {}, false);

	if (cmd->IsBypass()) {
		auto* vk_cb = static_cast<RHI::Vulkan::VK_CommandBuffer*>(cmd);
		ImGui_ImplVulkan_RenderDrawData(draw_data, vk_cb->GetCommandBuffer());
	} else {
		// Safe under MX_FORCE_LOCKSTEP (Editor.cpp): frame N's replay completes
		// before frame N+1's NewFrame reuses ImGui's draw-data buffers.
		cmd->GetRecordedCommands().push_back(std::make_unique<RHICmdRenderImGui>(draw_data, ImGui::GetCurrentContext()));
	}
}

void EditorUI::Release()
{
	{
		std::lock_guard<std::mutex> lock(g_preview_tex_mutex);
		for (auto& [tex, id] : preview_texture_cache)
			ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)id);
		preview_texture_cache.clear();
	}
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

ImTextureID EditorUI::GetPreviewTextureId(RHI::Texture* texture)
{
	if (!texture) return nullptr;
	{
		std::lock_guard<std::mutex> lock(g_preview_tex_mutex);
		auto it = preview_texture_cache.find(texture);
		if (it != preview_texture_cache.end())
			return it->second;
	}

	// Deferred creation on the RENDER thread: ImGui's Vulkan backend is not
	// thread-safe (RenderDrawData runs on the render thread), so creating the
	// descriptor from the logic thread races with rendering. Returns nullptr
	// until the command runs; the panel shows a blank frame meanwhile.
	// This file is the sanctioned exception for Vulkan includes (EditorUI.cpp).
	ENQUEUE_RENDER_COMMAND(UIPreviewTexCreate)([texture]()
	{
		auto* vk_tex = static_cast<RHI::Vulkan::VK_Texture*>(texture);
		ImTextureID id = ImGui_ImplVulkan_AddTexture(
			vk_tex->GetSampler(), vk_tex->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		{
			std::lock_guard<std::mutex> lock(g_preview_tex_mutex);
			preview_texture_cache[texture] = id;
		}
	});
	return nullptr;
}

void EditorUI::AddPanelUI(CONST String& name)
{
	for (auto& panel : panels)
	{
		if (panel->GetName() == name)
		{
			return;
		}
	}
	auto it =UI::BasePanel::CreatePanel(name,name);
	it ->Init();
	panels.push_back(it);
}

void EditorUI::AddPanelUI(UI::BasePanel* in_panel)
{
	in_panel->Init();
	panels.push_back(in_panel);
}

void EditorUI::OpenRanelUI(CONST String& name)
{
	for (auto& panel : panels)
	{
		if (panel->GetName() == name)
		{
			panel->is_show =true;
			return;
		}
	}
}

UI::RenderGraphPanel* EditorUI::GetRenderGraphPanel()
{
	return rg_panel;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
