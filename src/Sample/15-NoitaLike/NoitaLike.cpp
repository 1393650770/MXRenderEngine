// Sample 15-NoitaLike: Noita-style game framework demo.
//
// Demonstrates:
//   - GameApp fixed-tick loop (60 Hz, max 4 catch-up)
//   - GameWorld composition root (ECS + GpuPixelWorld + TerrainEditQueue +
//     GameplayEventBus + PixelCollision)
//   - Player movement (WASD) with solid-mask collision (walks through sand/water)
//   - Mouse-aim shooting: projectiles explode terrain (TerrainEditQueue)
//   - RmlUI HUD (HP bar via data-style-width)

#include "NoitaLike.h"
// The generated binding header references NoitaLikeApp unqualified from
// MXRender::UI::Widget - make it visible there BEFORE including the header.
namespace MXRender { namespace UI { namespace Widget {
	using Application::NoitaLikeApp;
} } }
#include "Application/Window.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderViewport.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"
#include "World/GameWorld.h"
#include "World/TerrainEditCommand.h"
#include "ECS/ECSManager.h"
#include "Gameplay/Components.h"
#include "Gameplay/Systems.h"
#include "Gameplay/SystemRegistry.h"
#include "UI/UIManager.h"
#include "UI/RmlUI/RmlUISystem.h"
#include "UI/UIRenderPass.h"
#include "RmlUI/NoitaLike.UIBinding.Gen.h"
#include <iostream>
using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::World;

World::GameWorld* NoitaLikeApp::CreateGameWorld()
{
	// GPU simulator backend (Phase 2) with a fixed seed for reproducibility.
	auto* world = new World::GameWorld(42);
	// Register systems (order = execution order).
	world->GetSystems().Register("Movement", std::make_unique<Gameplay::MovementSystem>());
	world->GetSystems().Register("Projectile", std::make_unique<Gameplay::ProjectileSystem>());
	world->GetSystems().Register("CameraFollow", std::make_unique<Gameplay::CameraFollowSystem>());
	return world;
}

void NoitaLikeApp::OnGameInit()
{
	std::cout << "[NoitaLike] world created (seed=42, GPU backend)" << std::endl;

	// RmlUI HUD.
	auto* rml_system = new MXRender::UI::RmlUI::RmlUISystem();
	rml_system->Init(viewport);
	MXRender::UI::UIManager::Create(rml_system);

	MXRender::UI::UIManager::Get().LoadFont("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-latin.ttf");
	MXRender::UI::UIManager::Get().LoadFont("Font/ark-pixel-font-10px-monospaced-ttf-v2026.07.20/ark-pixel-10px-monospaced-zh_cn.ttf");

	m_hud_model = MXRender::UI::UIManager::Get().CreateDataModel("hud");
	if (m_hud_model.IsValid())
	{
		MXRender::UI::UIManager::Get().BindDataModel<MXRender::UI::Widget::UIWidgetBindingTraits<NoitaLikeApp>>(m_hud_model, this);
	}

	m_hud_doc = MXRender::UI::UIManager::Get().LoadPanel("RmlUI/NoitaLikeHUD.rml");
	if (m_hud_doc.IsValid())
	{
		MXRender::UI::UIManager::Get().ShowPanel(m_hud_doc);
		std::cout << "[NoitaLike] HUD shown" << std::endl;
	}

	auto size = viewport->GetViewportSize();
	MXRender::UI::RegisterUIPass(
		&graph,
		GetBackBufferResource(),
		MXRender::UI::UIManager::Get().GetRenderer(),
		RHIGetImmediateCommandList(),
		size[0], size[1],
		[this](MXRender::RHI::CommandList* cmd) {
			BindBackBufferTarget(cmd);
			MXRender::UI::UIManager::Get().Render(cmd);
		});

	for (auto& pass : graph.GetPasses())
		pass->SetIsCullable(false);

	// Camera setup.
	camera_.SetViewport(GetViewportWidth(), GetViewportHeight());
	camera_.SetOrthoSize(30.0f);
	camera_.SetPosition(glm::vec2(128.0f, 96.0f));
	camera_.UpdateMatrices();
	m_controller.Attach(GetPlatformWindow());
	m_controller.enable_pan = false;   // game camera: no manual pan

	SpawnPlayer();
}

void NoitaLikeApp::SpawnPlayer()
{
	auto& world = *GetWorld();
	auto& ecs = world.GetECS();
	m_player = ecs.CreateEntity();
	ecs.AddComponent<Gameplay::TransformComp>(m_player);
	ecs.AddComponent<Gameplay::VelocityComp>(m_player);
	ecs.AddComponent<Gameplay::PlayerComp>(m_player);
	ecs.AddComponent<Gameplay::HealthComp>(m_player);
	ecs.AddComponent<Gameplay::WandComp>(m_player);
	// Player spawns in the middle of the world.
	ecs.GetComponent<Gameplay::TransformComp>(m_player)->pos = glm::vec2(128.0f, 100.0f);
}

void NoitaLikeApp::HandleInput()
{
	auto& input = MXRender::Input::InputSystem::Get();
	auto* tf = GetWorld()->GetECS().GetComponent<Gameplay::TransformComp>(m_player);
	auto* vel = GetWorld()->GetECS().GetComponent<Gameplay::VelocityComp>(m_player);
	auto* player = GetWorld()->GetECS().GetComponent<Gameplay::PlayerComp>(m_player);
	if (!tf || !vel || !player)
		return;

	using namespace MXRender::Input;
	glm::vec2 move(0.0f, 0.0f);
	if (input.IsKeyDown(EKey::A)) move.x -= 1.0f;
	if (input.IsKeyDown(EKey::D)) move.x += 1.0f;
	if (input.IsKeyDown(EKey::W)) move.y += 1.0f;
	if (input.IsKeyDown(EKey::S)) move.y -= 1.0f;

	// Horizontal movement is direct; vertical sets velocity (gravity handled
	// by MovementSystem). Jump on W.
	if (move.x != 0.0f)
		vel->vel.x = move.x * player->speed;
	else
		vel->vel.x *= 0.9f;   // friction

	if (move.y > 0.0f && std::abs(vel->vel.y) < 0.5f)
		vel->vel.y = player->jump_speed;

	// Mouse aim + shoot.
	Bool fire = input.IsMouseDown((Int)MouseButton::Left);
	if (fire && !m_prev_fire)
		Shoot();
	m_prev_fire = fire;
}

void NoitaLikeApp::Shoot()
{
	auto& world = *GetWorld();
	auto& ecs = world.GetECS();
	auto* tf = ecs.GetComponent<Gameplay::TransformComp>(m_player);
	if (!tf)
		return;

	// Aim at mouse position in world space.
	Float32 mx = 0.0f, my = 0.0f;
	MXRender::Input::InputSystem::Get().GetMousePos(mx, my);
	glm::vec2 aim_world = camera_.ScreenToWorld(mx, my);
	glm::vec2 dir = glm::normalize(aim_world - tf->pos);

	// Spawn projectile entity.
	MXRender::ECS::EntityHandle proj = ecs.CreateEntity();
	ecs.AddComponent<Gameplay::TransformComp>(proj);
	ecs.AddComponent<Gameplay::VelocityComp>(proj);
	ecs.AddComponent<Gameplay::ProjectileComp>(proj);
	auto* ptf = ecs.GetComponent<Gameplay::TransformComp>(proj);
	auto* pvel = ecs.GetComponent<Gameplay::VelocityComp>(proj);
	ptf->pos = tf->pos + dir * 1.0f;
	ptf->half_size = glm::vec2(0.25f, 0.25f);
	pvel->vel = dir * 12.0f;
	std::cout << "[NoitaLike] shot at (" << aim_world.x << ", " << aim_world.y << ")" << std::endl;
}

void NoitaLikeApp::SyncHud()
{
	auto* hp = GetWorld()->GetECS().GetComponent<Gameplay::HealthComp>(m_player);
	if (hp)
	{
		m_hp = (Int)hp->hp;
		if (m_hp != m_prev_hp && m_hud_model.IsValid())
		{
			MXRender::UI::UIManager::Get().DirtyVariable(m_hud_model, "hp");
			m_prev_hp = m_hp;
		}
	}
	UInt64 tick = GetSimTick();
	if ((Int)(tick / 60) != m_prev_score)
	{
		m_score = (Int)(tick / 60);
		if (m_hud_model.IsValid())
			MXRender::UI::UIManager::Get().DirtyVariable(m_hud_model, "score");
		m_prev_score = m_score;
	}
}

void NoitaLikeApp::OnGameTick()
{
	HandleInput();
	SyncHud();

	// Camera follows the player.
	auto* tf = GetWorld()->GetECS().GetComponent<Gameplay::TransformComp>(m_player);
	if (tf)
	{
		camera_.SetPosition(tf->pos);
		camera_.UpdateMatrices();
	}
}

void NoitaLikeApp::OnShutdownScene()
{
	MXRender::UI::UIManager::Destroy();
	GameApp::OnShutdownScene();
}

int main()
{
	NoitaLikeApp app;
	return SampleApp::RunSample(app, "MXRender NoitaLike");
}
