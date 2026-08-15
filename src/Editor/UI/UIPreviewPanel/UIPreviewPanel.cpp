#include "UIPreviewPanel.h"
#include "PreviewViewport.h"
#include "UIPreviewData.h"

// The only line that names the RmlUI backend — the rest goes through UIManager.
#include "UI/RmlUI/RmlUISystem.h"
#include "UI/UIInputBridge.h"
#include "UI/UIDataModelBinder.h"

// Document model (round-trip pipeline for the preview document)
#include "UI/UIDocumentModel/RmlParser.h"
#include "UI/UIDocumentModel/UIRcssParser.h"
#include "UI/UIDocumentModel/UIDocumentSerializer.h"
#include "UI/UIDesigner/UIPalette.h"

#include "Render/Core/CommandQueue.h"
#include "RHI/RenderRHI.h"
#include "RHI/ResourceManager.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderCommandList.h"
#include "Platform/PlatformFile.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"

#include "EditorRender/EditorUI.h"

#include <imgui.h>
#include <cmath>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

using namespace MXRender::UI;
using namespace MXRender::UI::UIDocModel;

UIPreviewPanel* UIPreviewPanel::s_instance = nullptr;
static RHI::Viewport* s_host_viewport = nullptr;   // set before the panel exists (Single mode: init runs inline)

UIPreviewPanel::UIPreviewPanel(const String& in_name, Bool in_show)
	: UI::BasePanel(in_name, in_show)
{
	s_instance = this;
}

UIPreviewPanel::~UIPreviewPanel()
{
	if (s_instance == this) s_instance = nullptr;
}

// =========================================================================
// Lifecycle
// =========================================================================

void UIPreviewPanel::Init()
{
	// Host viewport is the Editor's swapchain viewport — only used as a
	// texture-format source on the render thread.
	m_host_viewport = s_host_viewport;   // wired by EditorUI::Init before AddPanelUI
	UIDesignerState::SetInstance(&m_designer);
	m_preview_viewport = new UI::PreviewViewport();

	// Seed the preview document from the demo panel (round-trip: parse →
	// serialize → write __preview__ files → hot reload picks them up).
	EnsurePreviewFiles();

	// Render-thread one-shot init: textures + RmlUISystem + UIManager.
	ENQUEUE_RENDER_COMMAND(UIPreviewInit)([this]()
	{
		// Texture format MUST match the editor backbuffer (RmlUIRenderer's
		// PSOs bind against it) — depth D24S8 to match the editor DSV.
		auto* host = m_host_viewport;   // wired before Init in the pipeline
		if (!host)
		{
			std::cerr << "[UIPreview] no host viewport — preview disabled" << std::endl;
			return;   // m_ready stays false: Update()/Draw() show the placeholder
		}
		const UInt32 w = 640, h = 480;

		RHI::TextureDesc color_desc;
		color_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
		color_desc.width = w;
		color_desc.height = h;
		color_desc.format = host->GetCurrentBackBufferRTV()->GetTextureDesc().format;
		color_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT
			| ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
		color_desc.clear_value = RHI::ClearValue{ 0.12f, 0.12f, 0.15f, 1.0f };
		m_color_tex = ::Resolve(RHICreateTexture(color_desc));

		RHI::TextureDesc depth_desc = color_desc;
		depth_desc.format = ENUM_TEXTURE_FORMAT::D24S8;
		depth_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT;
		depth_desc.clear_value = RHI::ClearValue{ 1.0f, 0 };
		m_depth_tex = ::Resolve(RHICreateTexture(depth_desc));

		m_preview_viewport->SetTargets(m_color_tex, m_depth_tex, w, h);

		auto* rml_system = new MXRender::UI::RmlUI::RmlUISystem();
		rml_system->Init(m_preview_viewport);
		UIManager::Create(rml_system);
		std::cerr << "[UIPreview] RmlUISystem initialized inside the editor" << std::endl;

		m_ready.store(true);   // LAST — gates logic-thread Update()
	});
}

void UIPreviewPanel::Update()
{
	if (!m_ready.load()) return;
	if (!UIManager::Get().GetRenderer()) return;   // destroyed

	// Designer document state (parse the current __preview__ files once) and
	// drain the undo/redo command queue.
	UIDesignerState::Get().EnsureLoaded();
	UIDesignerState::Get().Tick();

	// First ready frame: bind the preview data model + load the document.
	// Manual binding via the abstract UIDataModelBinder (MetaParser does not
	// scan src/Editor; the 3c binding registry will formalize this).
	if (!m_doc_shown)
	{
		m_doc_shown = true;
		// The preview document uses the demo panel's font — without it every
		// element logs "No font face defined" during LoadDocument, which the
		// hot-reload log capture counts as a parse failure and aborts reloads.
		UIManager::Get().LoadFont(
			"Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-latin.ttf");
		UIManager::Get().LoadFont(
			"Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-zh_cn.ttf");
		m_model = UIManager::Get().CreateDataModel("preview");
		if (m_model.IsValid())
		{
			if (auto* binder = UIManager::Get().GetModelBinder(m_model))
			{
				binder->Bind("hp", &m_preview_data.m_hp);
				binder->Bind("score", &m_preview_data.m_score);
				binder->Bind("player_name", &m_preview_data.m_player_name);
				// Explicit std::function types disambiguate the Int/Float32 overloads.
				binder->BindTwoWay("m_volume",
					std::function<Int()>([&]() { return m_preview_data.m_volume; }),
					std::function<void(Int)>([&](Int v) { m_preview_data.m_volume = v; }));
			}
		}
		m_doc = UIManager::Get().LoadPanel("RmlUI/__preview__.rml");
		if (m_doc.IsValid())
		{
			UIManager::Get().ShowPanel(m_doc);
			std::cerr << "[UIPreview] preview document shown" << std::endl;
		}
		else
		{
			std::cerr << "[UIPreview] failed to load __preview__.rml" << std::endl;
		}
	}

	// Live sample data (drives {{ hp }} etc. so bindings visibly tick).
	m_preview_data.m_hp = (m_preview_data.m_hp + 1) % 100;
	m_preview_data.m_score += 1;
	UIManager::Get().DirtyVariable(m_model, "hp");
	UIManager::Get().DirtyVariable(m_model, "score");

	UIManager::Get().Update(1.0f / 60.0f);
}

void UIPreviewPanel::Draw()
{
	// The preview canvas is a fixed 640x480 image. The panel lives in the
	// editor DockSpace (central tab, see BuildDefaultDockLayout); the user can
	// float it out at will. No forced position — the dock/ini owns it.

	if (!OnBegin(ImGuiWindowFlags_NoCollapse))
		return;

	// Minimum size so the 640x480 canvas stays usable when undocked.
	ImGui::SetWindowSize(ImVec2(680, 560), ImGuiCond_FirstUseEver);

	if (m_ready.load() && m_color_tex)
	{
		ImGui::Text("Live RmlUi preview — LMB select/move, Alt = native input, F8 debugger");
		const ImVec2 canvas_min = ImGui::GetCursorScreenPos();
		ImGui::Image(MXRender::Application::EditorUI::GetPreviewTextureId(m_color_tex),
			ImVec2(640, 480));
		DrawCanvasInteraction(canvas_min);
		ForwardInput(canvas_min);
	}
	else
	{
		ImGui::Text("Initializing preview...");
	}
	OnEnd();
}

// =========================================================================
// Designer canvas: selection / move / resize / palette drop
//
// Edit mode (no modifier): LMB picks (PickElementAt → id of the element or
// its nearest id'd ancestor), drag moves, corner/edge handles resize — all
// transient (inline-style overlays that vanish on reload), committed into the
// #id rule by ONE snapshot command at release. Alt = raw RmlUi interaction
// pass-through (buttons, sliders work normally).
// =========================================================================

void UIPreviewPanel::DrawCanvasInteraction(const ImVec2& canvas_min)
{
	auto& ds = UIDesignerState::Get();
	if (!ds.IsLoaded()) return;

	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 canvas_max = canvas_min + ImVec2(640.0f, 480.0f);
	const bool hovered = ImGui::IsWindowHovered()
		&& ImGui::IsMouseHoveringRect(canvas_min, canvas_max);
	const bool native = io.KeyAlt;   // Alt = raw RmlUi interaction

	if (hovered && !native)
	{
		const float px = io.MousePos.x - canvas_min.x;
		const float py = io.MousePos.y - canvas_min.y;

		if (io.MouseClicked[0])
		{
			// 1. resize handle on the current selection?
			const String& sel = ds.GetSelection();
			UIDesignerState::Box box;
			if (!sel.empty() && UIManager::Get().GetElementBox(sel, box.x, box.y, box.w, box.h))
			{
				const int h = HitTestHandle(box, px, py);
				if (h >= 0)
				{
					m_drag_mode = EDragMode::Resize;
					m_resize_handle = h;
					m_drag_start_x = px; m_drag_start_y = py;
					m_drag_orig = box;
				}
			}
			// 2. pick + prepare move (only if a handle wasn't grabbed)
			if (m_drag_mode == EDragMode::None)
			{
				const String hit = UIManager::Get().PickElementAt((int)px, (int)py);
				if (hit != sel)
					ds.Select(hit);
				if (!hit.empty())
				{
					UIDesignerState::Box hbox;
					if (UIManager::Get().GetElementBox(hit, hbox.x, hbox.y, hbox.w, hbox.h))
					{
						m_drag_mode = EDragMode::Move;   // activates past the threshold
						m_drag_start_x = px; m_drag_start_y = py;
						m_drag_orig = hbox;
					}
				}
			}
		}
		else if (io.MouseDown[0] && m_drag_mode != EDragMode::None)
		{
			const float dx = px - m_drag_start_x;
			const float dy = py - m_drag_start_y;
			if (!m_drag_active && (fabsf(dx) > 4.0f || fabsf(dy) > 4.0f))
				m_drag_active = true;
			if (m_drag_active)
			{
				const String& sel = ds.GetSelection();
				if (!sel.empty())
				{
					if (m_drag_mode == EDragMode::Move)
					{
						// Transient overlay — vanished by the reload on release.
						UIManager::Get().SetElementBoxTransient(sel,
							m_drag_orig.x + dx, m_drag_orig.y + dy,
							m_drag_orig.w, m_drag_orig.h);
					}
					else
					{
						UIDesignerState::Box nb = m_drag_orig;
						ResizeByHandle(nb, m_resize_handle, dx, dy);
						UIManager::Get().SetElementBoxTransient(sel,
							nb.x, nb.y, nb.w, nb.h);
					}
				}
			}
		}
		if (io.MouseReleased[0] && m_drag_mode != EDragMode::None)
		{
			// Commit ONE undoable box change (SetBoxCmd → #id rule → reload).
			if (m_drag_active)
			{
				const String& sel = ds.GetSelection();
				UIDesignerState::Box nb;
				if (!sel.empty() && UIManager::Get().GetElementBox(sel, nb.x, nb.y, nb.w, nb.h))
					ds.SetBoxUndoable(sel, m_drag_orig, nb);
			}
			m_drag_mode = EDragMode::None;
			m_drag_active = false;
			m_resize_handle = -1;
		}
	}
	else if (!io.MouseDown[0] && m_drag_mode != EDragMode::None)
	{
		// left the canvas / native mode with the button up — cancel the drag
		m_drag_mode = EDragMode::None;
		m_drag_active = false;
		m_resize_handle = -1;
	}

	// ---- palette drag-drop insert ----
	if (hovered && !native && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(UIPalettePanel::kDragPayload))
		{
			if (const UIWidgetTemplate* tpl = UIPalettePanel::FindTemplate(
				String((const char*)payload->Data)))
			{
				const float px = io.MousePos.x - canvas_min.x;
				const float py = io.MousePos.y - canvas_min.y;
				ds.InsertNodeUndoable(*tpl, "", px, py);
			}
		}
		ImGui::EndDragDropTarget();
	}

	// ---- selection overlay (box + resize handles) ----
	const String& sel = ds.GetSelection();
	UIDesignerState::Box box;
	if (!sel.empty() && UIManager::Get().GetElementBox(sel, box.x, box.y, box.w, box.h))
		DrawSelectionOverlay(canvas_min, ImGui::GetWindowDrawList());

	// Undo/redo while the canvas is hovered.
	if (hovered && !native)
		ds.HandleUndoRedoKeys();
}

void UIPreviewPanel::DrawSelectionOverlay(const ImVec2& canvas_min, ImDrawList* dl)
{
	auto& ds = UIDesignerState::Get();
	UIDesignerState::Box box;
	if (!UIManager::Get().GetElementBox(ds.GetSelection(), box.x, box.y, box.w, box.h))
		return;

	const ImVec2 bmin = canvas_min + ImVec2(box.x, box.y);
	const ImVec2 bmax = bmin + ImVec2(box.w, box.h);
	dl->AddRect(bmin, bmax, IM_COL32(255, 255, 255, 230), 0.0f, 0, 1.5f);
	const ImU32 handle_col = IM_COL32(64, 160, 255, 255);
	const float hs = 3.5f;
	const ImVec2 corners[4] = { bmin, { bmax.x, bmin.y }, bmax, { bmin.x, bmax.y } };
	for (int i = 0; i < 4; ++i)
		dl->AddRectFilled(corners[i] - ImVec2(hs, hs), corners[i] + ImVec2(hs, hs), handle_col);
	const ImVec2 mids[4] = {
		(corners[0] + corners[1]) * 0.5f, (corners[1] + corners[2]) * 0.5f,
		(corners[2] + corners[3]) * 0.5f, (corners[3] + corners[0]) * 0.5f };
	for (int i = 0; i < 4; ++i)
		dl->AddRectFilled(mids[i] - ImVec2(2.5f, 2.5f), mids[i] + ImVec2(2.5f, 2.5f), handle_col);
}

int UIPreviewPanel::HitTestHandle(const UIDesignerState::Box& box, float px, float py)
{
	const float c = 8.0f;   // corner hit radius
	const float e = 5.0f;   // edge hit radius
	const ImVec2 pts[4] = {
		{ box.x, box.y }, { box.x + box.w, box.y },
		{ box.x + box.w, box.y + box.h }, { box.x, box.y + box.h } };
	for (int i = 0; i < 4; ++i)
		if (fabsf(px - pts[i].x) <= c && fabsf(py - pts[i].y) <= c)
			return i * 2;   // corner handles: 0, 2, 4, 6
	struct { ImVec2 a, b; int idx; } edges[4] = {
		{ pts[0], pts[1], 1 }, { pts[1], pts[2], 3 },
		{ pts[2], pts[3], 5 }, { pts[3], pts[0], 7 } };
	for (const auto& ed : edges)
	{
		const ImVec2 mid = (ed.a + ed.b) * 0.5f;
		if (fabsf(px - mid.x) <= e && fabsf(py - mid.y) <= e)
			return ed.idx;
	}
	return -1;
}

void UIPreviewPanel::ResizeByHandle(UIDesignerState::Box& box, int handle, float dx, float dy)
{
	const float min = 8.0f;
	switch (handle)
	{
	case 0: box.x += dx; box.w -= dx; box.y += dy; box.h -= dy; break;   // top-left
	case 1: box.y += dy; box.h -= dy; break;                            // top
	case 2: box.w += dx; box.y += dy; box.h -= dy; break;               // top-right
	case 3: box.w += dx; break;                                         // right
	case 4: box.w += dx; box.h += dy; break;                            // bottom-right
	case 5: box.h += dy; break;                                         // bottom
	case 6: box.x += dx; box.w -= dx; box.h += dy; break;               // bottom-left
	case 7: box.x += dx; box.w -= dx; break;                            // left
	}
	if (box.w < min) { box.x -= (min - box.w); box.w = min; }
	if (box.h < min) { box.y -= (min - box.h); box.h = min; }
}

void UIPreviewPanel::Release()
{
	// Render thread is joined/drained before this runs (pipeline shutdown).
	UIManager::Destroy();
	delete m_color_tex; m_color_tex = nullptr;
	delete m_depth_tex; m_depth_tex = nullptr;
	delete m_preview_viewport; m_preview_viewport = nullptr;
	m_ready.store(false);
}

// =========================================================================
// Render-thread pass hook (called from the editor graph's UIPreviewPass)
// =========================================================================

void UIPreviewPanel::ExecutePreviewPass(RHI::CommandList* cmd)
{
	auto* panel = s_instance;
	if (!panel || !panel->m_ready.load() || !panel->m_color_tex) return;

	auto* color = panel->m_color_tex;
	auto* depth = panel->m_depth_tex;
	// Manual transitions — the textures live fully outside the RDG (ImGui
	// samples them right after graph execution).
	cmd->TransitionTextureState(color, ENUM_RESOURCE_STATE::RenderTarget);
	if (depth)
		cmd->TransitionTextureState(depth, ENUM_RESOURCE_STATE::DepthWrite);

	Vector<RHI::ClearValue> clears = { RHI::ClearValue{ 0.12f, 0.12f, 0.15f, 1.0f },
		RHI::ClearValue{ 1.0f, 0 } };
	cmd->SetRenderTarget({ color }, depth, clears, depth != nullptr);
	UIManager::Get().Render(cmd);

	cmd->TransitionTextureState(color, ENUM_RESOURCE_STATE::ShaderResource);
}

bool UIPreviewPanel::IsReady()
{
	return s_instance && s_instance->m_ready.load();
}

void UIPreviewPanel::SetHostViewport(RHI::Viewport* viewport)
{
	s_host_viewport = viewport;
	if (s_instance) s_instance->m_host_viewport = viewport;
}

// =========================================================================
// Preview document seeding
// =========================================================================

void UIPreviewPanel::EnsurePreviewFiles()
{
	// Parse the demo panel from the project source, serialize through the IR
	// (canonical form), write both copies (source + output) so hot reload
	// stays coherent either way.
	const String root = FindProjectRoot();
	const String src_rml = root + "/resource/RmlUI/DemoPanel.rml";
	const String src_rcss = root + "/resource/RmlUI/DemoPanel.rcss";
	const String out_rml = "RmlUI/__preview__.rml";
	const String out_rcss = "RmlUI/__preview__.rcss";

	Vector<UInt8> rml_bytes, rcss_bytes;
	if (Platform::PlatformFile::ReadFile(src_rml, rml_bytes))
	{
		String text(rml_bytes.begin(), rml_bytes.end());
		auto doc = RmlParser::Parse(text, src_rml);
		// Re-point the stylesheet href so the preview resolves its own rcss.
		doc.stylesheets.clear();
		doc.stylesheets.push_back("__preview__.rcss");
		doc.file_path = out_rml;
		// Re-point the data model to the preview's own ("preview") — the demo
		// panel's "hud" doesn't exist here, and RmlUi logs an Error during
		// LoadDocument that the hot-reload log capture counts as a failure.
		for (auto& a : doc.body_attributes)
			if (a.name == "data-model")
				a.value = "preview";
		String serialized = UIDocumentSerializer::SerializeRml(doc);
		Vector<UInt8> bytes(serialized.begin(), serialized.end());
		Platform::PlatformFile::WriteFileAtomic(out_rml, bytes);
		if (!root.empty())
			Platform::PlatformFile::WriteFileAtomic(root + "/resource/" + out_rml, bytes);
	}
	if (Platform::PlatformFile::ReadFile(src_rcss, rcss_bytes))
	{
		String text(rcss_bytes.begin(), rcss_bytes.end());
		auto ss = UIRcssParser::Parse(text, src_rcss);
		String serialized = UIDocumentSerializer::SerializeRcss(ss);
		Vector<UInt8> bytes(serialized.begin(), serialized.end());
		Platform::PlatformFile::WriteFileAtomic(out_rcss, bytes);
		if (!root.empty())
			Platform::PlatformFile::WriteFileAtomic(root + "/resource/" + out_rcss, bytes);
	}
}

// =========================================================================
// Input forwarding (hover-gated: ImGui owns the mouse unless the preview
// panel is hovered and not consuming)
// =========================================================================

void UIPreviewPanel::ForwardInput(const ImVec2& canvas_min)
{
	ImGuiIO& io = ImGui::GetIO();
	auto* bridge = UIManager::Get().GetInputBridge();
	if (!bridge) return;

	// F8 debugger toggle is GLOBAL (the editor has no other F8 use) — not
	// hover-gated like the mouse/keyboard forwarding below.
	{
		auto& input = MXRender::Input::InputSystem::Get();
		const bool f8 = input.IsKeyDown(MXRender::Input::EKey::F8);
		if (f8 && !m_prev_f8)
			UIManager::Get().ToggleDebugger();
		m_prev_f8 = f8;
	}

	const bool hovered = ImGui::IsWindowHovered();
	const bool mouse_free = hovered && !io.WantCaptureMouse;
	const bool native = io.KeyAlt;        // Alt = raw RmlUi interaction (no canvas editing)
	const bool editing = m_drag_active;   // LMB owned by the canvas

	// Mouse position relative to the preview surface (canvas-local = the
	// RmlUi viewport's 640x480 space).
	if (mouse_free && !editing)
	{
		bridge->ProcessMouseMove((Int)(io.MousePos.x - canvas_min.x),
			(Int)(io.MousePos.y - canvas_min.y));
	}
	for (int b = 0; b < 3; ++b)
	{
		const bool block = editing && b == 0;
		bool down = mouse_free && !block && io.MouseDown[b];
		if (down != m_mouse_down[b])
		{
			bridge->ProcessMouseButton(b, down);
			m_mouse_down[b] = down;
		}
	}
	if (mouse_free && !editing && io.MouseWheel != 0.0f)
	{
		bridge->ProcessMouseScroll(0.0f, io.MouseWheel);
		io.MouseWheel = 0.0f;
	}

	// Keyboard: forward when hovered (level-latched edges like the samples).
	if (hovered)
	{
		auto& input = MXRender::Input::InputSystem::Get();
		using namespace MXRender::Input;
		static const Key kAllKeys[] = {
			EKey::A, EKey::B, EKey::C, EKey::D, EKey::E, EKey::F, EKey::G, EKey::H,
			EKey::I, EKey::J, EKey::K, EKey::L, EKey::M, EKey::N, EKey::O, EKey::P,
			EKey::Q, EKey::R, EKey::S, EKey::T, EKey::U, EKey::V, EKey::W, EKey::X,
			EKey::Y, EKey::Z,
			EKey::K0, EKey::K1, EKey::K2, EKey::K3, EKey::K4,
			EKey::K5, EKey::K6, EKey::K7, EKey::K8, EKey::K9,
			EKey::F1, EKey::F2, EKey::F3, EKey::F4, EKey::F5, EKey::F6,
			EKey::F7, EKey::F8, EKey::F9, EKey::F10, EKey::F11, EKey::F12,
			EKey::Escape, EKey::Tab, EKey::Enter, EKey::Backspace, EKey::Delete, EKey::Space,
			EKey::Up, EKey::Down, EKey::Left, EKey::Right,
			EKey::LeftShift, EKey::RightShift, EKey::LeftCtrl, EKey::RightCtrl,
			EKey::LeftAlt, EKey::RightAlt,
		};
		for (const Key& k : kAllKeys)
		{
			if (input.IsKeyPressed(k))  bridge->ProcessKey(k, true);
			if (input.IsKeyReleased(k)) bridge->ProcessKey(k, false);
		}
		for (UInt32 c : input.GetCharsThisFrame())
			bridge->ProcessChar(c);
	}
}

// =========================================================================
// Panel registration
// =========================================================================

namespace
{
UI::PanelRegister RegisterUIPreviewPanel([](const String& in_name, Bool in_show) -> UI::BasePanel*
{
	return new UIPreviewPanel(in_name, in_show);
}, UIPreviewPanel::GetTypeName());
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
