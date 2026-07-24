#include "RmlUIDemo.h"

// Zero RmlUI backend includes.  Only generic UI + generated Widget headers.
#include "UI/UIManager.h"
#include "UI/UIInputBridge.h"
#include "UI/UIRenderer.h"
#include "UI/UIRenderPass.h"

// Generated UIWidget bindings — replaces old RmlUI/AllRmlDataModel.h.
// This includes <RmlUi/...> internally; application code does NOT.
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

void RmlUIDemoApp::OnHeal(Rml::DataModelHandle, Rml::Event&)
{
	m_hp = (m_hp + 10);
	if (m_hp > 100) m_hp = 100;
	UIManager::Get().DirtyVariable(m_hud_model, "hp");
	std::cout << "[RmlUIDemo] Heal clicked! HP now: " << m_hp << std::endl;
}

void RmlUIDemoApp::OnInitScene()
{
	std::cout << "[RmlUIDemo] OnInitScene" << std::endl;

	UIManager::Create(viewport);
	UIManager::Get().EnableDebugger(true);

	UIManager::Get().LoadFontFace("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-latin.ttf");
	UIManager::Get().LoadFontFace("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-zh_cn.ttf");

	m_hud_model = UIManager::Get().CreateDataModel("hud");
	if (m_hud_model.IsValid())
	{
		// Auto-generated BindDataModel from UI_BIND_* annotations.
		// Handles all fields (OneWay, TwoWay) and event callbacks.
		void* ctor_opaque = UIManager::Get().GetModelConstructor(m_hud_model);
		UIWidgetBindingTraits<RmlUIDemoApp>::BindDataModel(ctor_opaque, this);
	}

	m_hud_doc = UIManager::Get().LoadDocument("RmlUI/DemoPanel.rml");
	if (m_hud_doc.IsValid())
	{
		UIManager::Get().ShowDocument(m_hud_doc);
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
	auto CheckKey = [&](const Key& k) {
		if (input.IsKeyPressed(k))
			bridge->ProcessKey(k, true);
		if (input.IsKeyReleased(k))
			bridge->ProcessKey(k, false);
	};
	CheckKey(EKey::Escape);
	CheckKey(EKey::Tab);
	CheckKey(EKey::Enter);
	CheckKey(EKey::Space);
	CheckKey(EKey::Up);
	CheckKey(EKey::Down);
	CheckKey(EKey::Left);
	CheckKey(EKey::Right);

	UIManager::Get().Update(dt);
}

int main()
{
	RmlUIDemoApp app;
	return MXRender::Application::SampleApp::RunSample(app, "Sample 12 - RmlUI Game UI Demo");
}
