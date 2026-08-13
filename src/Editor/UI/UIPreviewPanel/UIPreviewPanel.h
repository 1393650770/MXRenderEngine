#pragma once
#ifndef _UI_PREVIEW_PANEL_
#define _UI_PREVIEW_PANEL_

#include "Core/ConstDefine.h"
#include "UI/BasePanel.h"
#include "UI/UIHandleTypes.h"
#include "UI/UIManager.h"
#include "UIPreviewData.h"
#include "UI/UIDesigner/UIDesignerState.h"

#include <atomic>

struct ImVec2;
struct ImDrawList;

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
class Texture;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)
class PreviewViewport;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)

/// Editor panel hosting a real RmlUISystem rendering into its own offscreen
/// textures, shown with ImGui::Image. The preview document flows through the
/// runtime hot-reload pipeline: serialize IR → write resource/RmlUI/__preview__
/// → ReloadAllDocuments.
///
/// Threading (mirrors the RmlUIDemo sample split):
///   logic thread: Update() (UIManager::Update → RmlUi context + hot-reload
///                 poll), Draw() (ImGui + canvas interaction + input forwarding)
///   render thread: one-shot init command (textures + RmlUISystem::Init +
///                 UIManager::Create), UIPreviewPass execute (UIManager::Render)
///   m_ready is the atomic gate that lets Update() run only after init.
///
/// Designer canvas (LMB select/move, handles resize, palette drop, Alt = raw
/// RmlUi interaction pass-through) — all commits go through UIDesignerState
/// snapshot commands → RCSS #id rules → hot reload. See UIDesignerState.
class UIPreviewPanel : public UI::BasePanel
{
public:
	static String GetTypeName() { return "UI Preview"; }

	UIPreviewPanel(const String& in_name, Bool in_show);
	~UIPreviewPanel() override;

	void Init() override;
	void Update() override;
	void Draw() override;
	void Release() override;

	/// Called by the editor's UIPreviewPass execute lambda (render thread).
	static void ExecutePreviewPass(RHI::CommandList* cmd);
	/// True once the RmlUISystem instance is ready (render-thread init done).
	static bool IsReady();
	/// Host viewport (editor swapchain) — texture-format source. Set from
	/// EditorUI::Init BEFORE the render-thread init command runs.
	static void SetHostViewport(RHI::Viewport* viewport);

private:
	enum class EDragMode { None, Move, Resize };

	void LoadPreviewDocument();   // parse resource/RmlUI/DemoPanel → write __preview__ files
	void ForwardInput(const ImVec2& canvas_min);   // hover-gated mouse/key/char forwarding
	void EnsurePreviewFiles();
	void DrawCanvasInteraction(const ImVec2& canvas_min);
	static int HitTestHandle(const UIDesignerState::Box& box, float px, float py);
	static void ResizeByHandle(UIDesignerState::Box& box, int handle, float dx, float dy);
	void DrawSelectionOverlay(const ImVec2& canvas_min, ImDrawList* dl);

	RHI::Viewport* m_host_viewport = nullptr;   // Editor viewport (texture format source)
	UI::PreviewViewport* m_preview_viewport = nullptr;
	RHI::Texture* m_color_tex = nullptr;        // render-thread owned
	RHI::Texture* m_depth_tex = nullptr;
	std::atomic<bool> m_ready{ false };
	bool m_doc_shown = false;
	bool m_prev_f8 = false;
	bool m_mouse_down[3] = {};
	UIModelHandle m_model;
	UIDocHandle m_doc;
	UIPreviewData m_preview_data;   // live sample data driving {{ hp }} etc.
	UInt32 m_magic = 0x5A17C0DE;    // corruption sentinel

	// ---- designer canvas interaction (logic thread) ----
	UIDesignerState m_designer;           // shared document state + undo/redo
	EDragMode m_drag_mode = EDragMode::None;
	bool m_drag_active = false;           // passed the click-vs-drag threshold
	int m_resize_handle = -1;             // 0-7 corner/edge handle, -1 none
	Float32 m_drag_start_x = 0, m_drag_start_y = 0;   // canvas-local mouse at press
	UIDesignerState::Box m_drag_orig;     // selected box at press

	static UIPreviewPanel* s_instance;
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_PREVIEW_PANEL_
