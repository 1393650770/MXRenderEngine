#include "RmlUIDemo.h"

// Zero RmlUI backend includes.  Only generic UI + generated Widget headers.
#include "UI/UIManager.h"
#include "UI/UIRenderer.h"
#include "UI/UIInputBridge.h"
#include "UI/UIRenderPass.h"
#include "UI/RmlUI/RmlUISystem.h"     // for new RmlUISystem — only line that names the backend

// Generated UIWidget bindings — zero RmlUi includes (uses UIDataModelBinder)
#include "RmlUI/RmlUIDemo.UIBinding.Gen.h"

#include "RHI/RenderViewport.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderRHI.h"
#include "Platform/PlatformWindow.h"
#include "Input/InputSystem.h"
#include <iostream>

using namespace MXRender::UI;
using MXRender::UI::Widget::UIWidgetBindingTraits;

RmlUIDemoApp::RmlUIDemoApp() {}
RmlUIDemoApp::~RmlUIDemoApp() {}

void RmlUIDemoApp::OnHeal()
{
	m_hp = (m_hp + 10);
	if (m_hp > 100) m_hp = 100;
	UIManager::Get().DirtyVariable(m_hud_model, "hp");
	std::cout << "[RmlUIDemo] Heal clicked! HP now: " << m_hp << std::endl;
}

void RmlUIDemoApp::OnInitScene()
{
	std::cout << "[RmlUIDemo] OnInitScene" << std::endl;

	auto* rml_system = new MXRender::UI::RmlUI::RmlUISystem();
	rml_system->Init(viewport);
	UIManager::Create(rml_system);

	UIManager::Get().LoadFont("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-latin.ttf");
	UIManager::Get().LoadFont("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-zh_cn.ttf");

	m_hud_model = UIManager::Get().CreateDataModel("hud");
	if (m_hud_model.IsValid())
	{
		// Auto-generated BindDataModel from UI_BIND_* annotations.
		// Handles all fields (OneWay, TwoWay) and event callbacks.
		UIManager::Get().BindDataModel<UIWidgetBindingTraits<RmlUIDemoApp>>(m_hud_model, this);
	}

	m_hud_doc = UIManager::Get().LoadPanel("RmlUI/DemoPanel.rml");
	if (m_hud_doc.IsValid())
	{
		UIManager::Get().ShowPanel(m_hud_doc);
		std::cout << "[RmlUIDemo] Document loaded and shown." << std::endl;
	}
	else
	{
		std::cerr << "[RmlUIDemo] Failed to load DemoPanel.rml!" << std::endl;
	}

	auto size = viewport->GetViewportSize();
	MXRender::UI::RegisterUIPass(
		&graph,
		GetBackBufferResource(),
		UIManager::Get().GetRenderer(),
		RHIGetImmediateCommandList(),
		size[0], size[1],
		[this](MXRender::RHI::CommandList* cmd) {
			BindBackBufferTarget(cmd);
			UIManager::Get().Render(cmd);
		});

	for (auto& pass : graph.GetPasses())
		pass->SetIsCullable(false);
}

void RmlUIDemoApp::OnShutdownScene()
{
	std::cout << "[RmlUIDemo] OnShutdownScene" << std::endl;
	UIManager::Destroy();
}

void RmlUIDemoApp::OnUpdate(float dt)
{
	m_timer += dt;
	m_score = static_cast<int>(m_timer * 10.0f) % 1000;
	m_hp = (m_hp - 1 + 100) % 100;

	if (m_hud_model.IsValid())
	{
		if (m_hp != m_prev_hp) { UIManager::Get().DirtyVariable(m_hud_model, "hp"); m_prev_hp = m_hp; }
		if (m_score != m_prev_score) { UIManager::Get().DirtyVariable(m_hud_model, "score"); m_prev_score = m_score; }
		if (static_cast<int>(m_timer * 100) != static_cast<int>(m_prev_timer * 100)) {
			UIManager::Get().DirtyVariable(m_hud_model, "timer");
			m_prev_timer = m_timer;
		}
	}

	auto* bridge = UIManager::Get().GetInputBridge();
	auto* pw = GetPlatformWindow();
	if (!bridge || !pw) return;
	auto& input = MXRender::Input::InputSystem::Get();

	Float64 mx, my;
	pw->GetCursorPos(mx, my);
	bridge->ProcessMouseMove(static_cast<Int>(mx), static_cast<Int>(my));

	using MXRender::MouseButton;
	bridge->ProcessMouseButton(0, pw->GetMouseButton(MouseButton::Left));
	bridge->ProcessMouseButton(1, pw->GetMouseButton(MouseButton::Right));
	bridge->ProcessMouseButton(2, pw->GetMouseButton(MouseButton::Middle));

	Float32 scroll = input.GetScrollDelta();
	if (scroll != 0.0f)
		bridge->ProcessMouseScroll(0.0f, scroll);

	using namespace MXRender::Input;
	// F8 toggles the RmlUi built-in debugger (element tree, live style editing).
	// Level-latched: a single press must flip the toggle exactly once.
	const bool f8_down = input.IsKeyDown(EKey::F8);
	if (f8_down && !m_prev_f8_down)
		UIManager::Get().ToggleDebugger();
	m_prev_f8_down = f8_down;

	// Forward the full keyboard to the UI bridge so the debugger (tree
	// navigation, console) and future text inputs work. RmlUi ignores keys when
	// nothing is focused — game input reads InputSystem directly, unaffected.
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

	// Forward typed characters (debugger console input, future text boxes).
	// UTF-32 codepoints, cleared once per frame by InputSystem::BeginFrame.
	for (UInt32 c : input.GetCharsThisFrame())
		bridge->ProcessChar(c);

	UIManager::Get().Update(dt);
}

int main()
{
	RmlUIDemoApp app;
	return MXRender::Application::SampleApp::RunSample(app, "Sample 12 - RmlUI Game UI Demo");
}
