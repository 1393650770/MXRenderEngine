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
#include <filesystem>
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

	// First ready frame: bind the preview data model + load the document.
	// Manual binding via the abstract UIDataModelBinder (MetaParser does not
	// scan src/Editor; the 3c binding registry will formalize this).
	if (!m_doc_shown)
	{
		m_doc_shown = true;
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
	if (!OnBegin(ImGuiWindowFlags_NoCollapse))
		return;

	if (m_ready.load() && m_color_tex)
	{
		ImGui::Text("Live RmlUi preview — F8 toggles the debugger");
		ImGui::Image(MXRender::Application::EditorUI::GetPreviewTextureId(m_color_tex),
			ImVec2(640, 480));
		ForwardInput();
	}
	else
	{
		ImGui::Text("Initializing preview...");
	}
	OnEnd();
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

namespace
{
/// Walks up from cwd looking for a directory containing resource/RmlUI.
/// (The editor runs with cwd = build output; sources live in the repo.)
String FindProjectRoot()
{
	std::error_code ec;
	auto dir = std::filesystem::current_path(ec);
	if (ec) return "";
	for (int i = 0; i < 8; ++i)
	{
		if (std::filesystem::is_directory(dir / "resource" / "RmlUI", ec))
			return dir.string();
		ec.clear();
		dir = dir.parent_path();
		if (dir.empty()) break;
	}
	return "";
}
} // namespace

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

void UIPreviewPanel::ForwardInput()
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

	// Mouse position relative to the preview surface.
	if (mouse_free)
	{
		ImVec2 origin = ImGui::GetWindowPos();
		bridge->ProcessMouseMove((Int)(io.MousePos.x - origin.x), (Int)(io.MousePos.y - origin.y));
	}
	for (int b = 0; b < 3; ++b)
	{
		bool down = mouse_free && io.MouseDown[b];
		if (down != m_mouse_down[b])
		{
			bridge->ProcessMouseButton(b, down);
			m_mouse_down[b] = down;
		}
	}
	if (mouse_free && io.MouseWheel != 0.0f)
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
