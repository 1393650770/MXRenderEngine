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
#include "Core/Job/JobSystem.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderViewport.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderTexture.h"
#include "Tool/BufferUtils.h"
#include "Tool/ShaderLibrary.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"
#include "World/GameWorld.h"
#include "World/FrameSnapshot.h"
#include "World/TerrainEditCommand.h"
#include "World/GpuPixelWorld.h"
#include "World/IPixelWorldSimulator.h"
#include "World/MaterialRegistry.h"
#include "ECS/ECSManager.h"
#include "Gameplay/Components.h"
#include "Gameplay/Systems.h"
#include "Gameplay/TrailUpdateSystem.h"
#include "Gameplay/SystemRegistry.h"
#include "Render/LineRenderer/LineRendererManager.h"
#include "Render/LineRenderer/LineRendererPass.h"
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

namespace
{
	// DX demo fence tag (EnqueueRenderCommand<...> template needs a tag type).
	struct DxDemoFenceTag {};

	struct SimPassData : public RenderGraphPassDataBase
	{
		VIRTUAL ~SimPassData() MYDEFAULT;
		VIRTUAL void METHOD(Release)() OVERRIDE {}
	};

	// Player sprite (matches sprite2d.vert layout: pos RG32F + uv RG32F).
	struct PlayerQuadVertex { float x, y, u, v; };

	// Shader uniform block (matches sprite2d.vert binding 0).
	struct PlayerParams { glm::mat4 mvp; };

	struct PlayerPassData : public RenderGraphPassDataBase
	{
		RenderPipelineState* pso = nullptr;
		ShaderResourceBinding* srb = nullptr;
		VIRTUAL ~PlayerPassData() MYDEFAULT;
		VIRTUAL void METHOD(Release)() OVERRIDE { delete srb; srb = nullptr; }
	};
}

World::GameWorld* NoitaLikeApp::CreateGameWorld()
{
	// GPU simulator backend (Phase 2) with a fixed seed for reproducibility.
	auto* world = new World::GameWorld(42);
	// Register systems (order = execution order).
	world->GetSystems().Register("Movement", std::make_unique<Gameplay::MovementSystem>());
	world->GetSystems().Register("Projectile", std::make_unique<Gameplay::ProjectileSystem>());
	world->GetSystems().Register("CameraFollow", std::make_unique<Gameplay::CameraFollowSystem>());
	// Trail update: reads Transform (conflicts with Movement's Transform
	// write -> auto-demoted to sequential, runs after Projectile so the
	// trail tip equals the projectile's current position).
	world->GetSystems().Register("TrailUpdate", std::make_unique<Gameplay::TrailUpdateSystem>());
	return world;
}

void NoitaLikeApp::OnGameInit()
{
	std::cout << "[NoitaLike] world created (seed=42, GPU backend) - logic thread id "
		<< std::this_thread::get_id() << std::endl;

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

	// [Stress] Spawn extra dynamic entities so the MovementSystem parallel
	// partition path actually slices large storages (the real game has ~10-30
	// entities, far below the parallel threshold). These entities are logic-
	// only: MovementSystem integrates them, nothing renders or reads them.
	{
		constexpr UInt32 kStressCount = 4000;
		auto& ecs = world_->GetECS();
		for (UInt32 i = 0; i < kStressCount; ++i)
		{
			auto e = ecs.CreateEntity();
			auto* tf = ecs.AddComponent<Gameplay::TransformComp>(e);
			auto* vel = ecs.AddComponent<Gameplay::VelocityComp>(e);
			tf->pos = glm::vec2((Float32)(i % 256), (Float32)(i / 256) * 0.5f);
			// NOTE: cast to Int BEFORE the subtraction - (i % 7) is UInt32 and
			// underflows for i%7 < 3, producing vel ~2.1e9 (entities flew
			// 3.6e7 units/tick to garbage positions).
			vel->vel = glm::vec2((Float32)((Int)(i % 7) - 3) * 0.5f, (Float32)((Int)(i % 5) - 2) * 0.5f);
		}
		std::cout << "[Stress] spawned " << kStressCount << " entities, workers="
			<< Core::JobSystem::Get().GetWorkerCount() << std::endl;
	}

	// Line/trail rendering facade (created before any trail component).
	Render::LineRendererManager::Create();
	world_->GetECS().RegisterComponent<Render::LineRendererComponent>();   // 必须先于 AddComponent

	// World rendering: sim pass (GPU simulation) + display pass + player.
	RegisterWorldPasses();

	// Trails overlay the world between player and UI (RDG order = registration).
	Render::RegisterLinePass(&graph, GetBackBufferResource(), RHIGetImmediateCommandList());

	// UI pass is registered LAST - the RDG timeline follows registration
	// order, so the UI must be the final writer of the backbuffer. Its
	// draw_fn uses a load-mode SetRenderTarget (no clear) so it overlays the
	// pixel world instead of wiping it (BindBackBufferTarget would clear).
	auto size = viewport->GetViewportSize();
	MXRender::UI::RegisterUIPass(
		&graph,
		GetBackBufferResource(),
		MXRender::UI::UIManager::Get().GetRenderer(),
		RHIGetImmediateCommandList(),
		size[0], size[1],
		[this](MXRender::RHI::CommandList* cmd) {
			// HUD bound fields are written HERE on the RENDER thread from the
			// snapshot (logic thread only fills the snapshot - bound fields
			// are render-thread-exclusive writers once the lockstep is gone).
			// DirtyVariable auto-dispatches inline on the render thread.
			if (CONST World::FrameSnapshot* snap = m_render_snapshot)
			{
				if (m_hp != snap->hud_hp)
				{
					m_hp = snap->hud_hp;
					if (m_hud_model.IsValid())
						MXRender::UI::UIManager::Get().DirtyVariable(m_hud_model, "hp");
				}
				if (m_score != snap->hud_score)
				{
					m_score = snap->hud_score;
					if (m_hud_model.IsValid())
						MXRender::UI::UIManager::Get().DirtyVariable(m_hud_model, "score");
				}
				if (m_wand != snap->hud_wand)
				{
					m_wand = snap->hud_wand;
					if (m_hud_model.IsValid())
						MXRender::UI::UIManager::Get().DirtyVariable(m_hud_model, "wand");
				}
			}
			// Load-mode render target: the world display pass already cleared
			// + drew. The dsv MUST be attached (RmlUI's PSO declares
			// depth_stencil_view - a dsv-less render pass fails pipeline bind
			// and renders nothing); empty clears + has_dsv_clear=false keeps
			// the world pixels intact.
			Vector<Texture*> rtvs = { GetBackBufferResource()->GetActual() };
			cmd->SetRenderTarget(rtvs, GetDepthStencil(), Vector<ClearValue>{}, false);
			MXRender::UI::UIManager::Get().Render(cmd);
		});

	for (auto& pass : graph.GetPasses())
		pass->SetIsCullable(false);

	// Camera: whole-world overview (192 cells tall = 960 px at 5 px/cell).
	// Fixed position - follow is disabled until player collision exists
	// (GPU solid-mask readback, Phase 4), otherwise the player falls through
	// the world and the camera chases it into the void.
	camera_.SetViewport(GetViewportWidth(), GetViewportHeight());
	camera_.SetOrthoSize(192.0f);
	camera_.SetPosition(glm::vec2(128.0f, 96.0f));
	camera_.UpdateMatrices();
	m_controller.Attach(GetPlatformWindow());
	m_controller.enable_pan = false;   // game camera: no manual pan

	PreplaceTerrain();
	SpawnPlayer();
}

void NoitaLikeApp::RegisterWorldPasses()
{
	auto* world = GetWorld();
	if (!world)
		return;

	// World display renderer: palette + params + display pass (GPU source).
	m_world_renderer.Init();

	// Simulation pass (render thread): drives the GPU simulation chain.
	// The logic thread increments m_pending_ticks once per fixed tick
	// (see OnGameTick); the execute lambda consumes it and records the
	// 6-dispatch chain on the render thread.
	// NOTE: do NOT capture a local simulator pointer in the execute lambda
	// ([&] would leave a dangling reference after this function returns) -
	// fetch it fresh from the world each execution.
	auto* sim_pass = graph.AddRenderPass<SimPassData>("NoitaSim", &graph,
		RHIGetImmediateCommandList(),
	[&](SimPassData& data, RenderGraphPassBuilder& builder, CommandList* cmd)
	{
		// World buffers are retained/external - no RDG declarations.
	},
	[&](CONST SimPassData& data, CommandList* cmd)
	{
		UInt32 ticks = m_pending_ticks.exchange(0);
		UInt32 frame = m_frame_count++;
		if (frame < 5 || frame % 120 == 0)
			std::cout << "[NoitaLike] frame=" << frame << " pending_ticks=" << ticks << std::endl;
		IPixelWorldSimulator::SimFrameContext ctx;
		ctx.cmd = cmd;
		ctx.pending_ticks = ticks;
		ctx.frame_index = frame;
		auto* sim = GetWorld() ? GetWorld()->GetSimulator() : nullptr;
		if (sim)
			sim->TickFrame(ctx);
	});
	sim_pass->SetIsCullable(false);
	sim_pass->SetShaderPath("Shader/world_apply_edits");

	// Display pass (render thread, AFTER sim so the state is fresh).
	m_world_renderer.RegisterDisplayPass(&graph, GetBackBufferResource(), GetDepthStencil(),
		RHIGetImmediateCommandList());
	auto* gpu_world = dynamic_cast<GpuPixelWorld*>(world->GetSimulator());
	if (gpu_world)
	{
		m_world_renderer.CreateDisplayBindings(gpu_world->GetStateBuffer());
		m_world_renderer.SetDisplaySource(true);   // GPU authoritative state
	}

	// Player sprite pass (AFTER display so it overlays the world).
	RegisterPlayerPass();
}

void NoitaLikeApp::RegisterPlayerPass()
{
	// One quad (4 vertices / 6 indices) rebuilt per frame around the player.
	RHI::BufferDesc vb_desc;
	vb_desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
	vb_desc.size = sizeof(PlayerQuadVertex) * 4;
	vb_desc.stride = sizeof(PlayerQuadVertex);
	m_player_vb = g_render_rhi->CreateBuffer(vb_desc);

	Vector<UInt16> indices = { 0, 1, 2, 2, 3, 0 };
	RHI::BufferDesc ib_desc;
	ib_desc.type = ENUM_BUFFER_TYPE::Index | ENUM_BUFFER_TYPE::Dynamic;
	ib_desc.size = (UInt32)(indices.size() * sizeof(UInt16));
	ib_desc.stride = sizeof(UInt16);
	m_player_ib = g_render_rhi->CreateBuffer(ib_desc);
	Tool::BufferUtils::Upload(m_player_ib, indices.data(), ib_desc.size);

	m_player_params = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(PlayerParams));

	auto* pass = graph.AddRenderPass<PlayerPassData>("PlayerPass", &graph,
		RHIGetImmediateCommandList(),
	[&](PlayerPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
	{
		builder.Write(GetBackBufferResource());

		Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
			"Shader/sprite2d.vert.spv");
		Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
			"Shader/sprite2d.frag.spv");

		RenderGraphiPipelineStateDesc pd{};
		pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
		pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		pd.render_targets = { GetBackBuffer() };
		pd.raster_state.sample_count = 1;
		pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pd.blend_state.render_targets.resize(1);

		VertexInputLayout pos_layout;
		pos_layout.binding = 0;
		pos_layout.location = 0;
		pos_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		pos_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
		pos_layout.offset = 0;

		VertexInputLayout uv_layout;
		uv_layout.binding = 0;
		uv_layout.location = 1;
		uv_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		uv_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
		uv_layout.offset = (UInt32)(sizeof(float) * 2);

		pd.vertex_input_layout.push_back(pos_layout);
		pd.vertex_input_layout.push_back(uv_layout);

		data.pso = g_render_rhi->CreateRenderPipelineState(pd);
		data.pso->CreateShaderResourceBinding(data.srb, false);
		data.srb->SetResource("params", m_player_params);
		data.srb->FlushDescriptorWrites();
		delete vs;
		delete ps;
	},
	[=](CONST PlayerPassData& data, CommandList* in_cmd)
	{
		// Read the render-minimum snapshot - NEVER reach into live world state
		// (the lockstep happens-before chain goes away with frame-sync switch).
		CONST World::FrameSnapshot* snap = m_render_snapshot;
		if (!snap)
			return;

		// Rebuild the quad around the player (world units, sprite shader is
		// solid white).
		Float32 hx = snap->player_half_size.x;
		Float32 hy = snap->player_half_size.y;
		if (hx <= 0.0f || hy <= 0.0f) { hx = 1.0f; hy = 1.0f; }
		PlayerQuadVertex quads[4] = {
			{ snap->player_pos.x - hx, snap->player_pos.y - hy, 0.0f, 0.0f },
			{ snap->player_pos.x + hx, snap->player_pos.y - hy, 1.0f, 0.0f },
			{ snap->player_pos.x + hx, snap->player_pos.y + hy, 1.0f, 1.0f },
			{ snap->player_pos.x - hx, snap->player_pos.y + hy, 0.0f, 1.0f },
		};
		Tool::BufferUtils::Upload(m_player_vb, quads, sizeof(quads));

		PlayerParams params;
		params.mvp = snap->camera_mvp;
		Tool::BufferUtils::Upload(m_player_params, &params, sizeof(params));

		// Load-mode render target: keep the pixel world already drawn, only
		// overlay the player quad.
		Vector<Texture*> rtvs = { GetBackBufferResource()->GetActual() };
		in_cmd->SetRenderTarget(rtvs, nullptr, Vector<ClearValue>{}, false);
		in_cmd->SetGraphicsPipeline(data.pso);
		in_cmd->SetShaderResourceBinding(data.srb);
		in_cmd->SetVertexBuffer(m_player_vb, 0, sizeof(PlayerQuadVertex), 0);
		in_cmd->SetIndexBuffer(m_player_ib, 0, false);
		in_cmd->DrawIndexed(6, 1, 0, 0, 0);
	});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/sprite2d");
}

void NoitaLikeApp::PreplaceTerrain()
{
	auto* world = GetWorld();
	if (!world)
		return;
	// A stone floor + sand pockets so the world is not empty at startup.
	// Written directly to the world (not through TerrainEditQueue) - the
	// queue's ExpandCircle would generate ~49k events and stall the frame.
	// Stone is SOLID: mirror it into the CPU collision mask here so the
	// player stands on the floor (queue edits mirror automatically in
	// GameWorld::SyncSolidMask; direct sink writes must do it manually).
	auto* sink = world->GetEditSink();
	auto& collision = world->GetCollision();
	if (!sink)
		return;
	// Stone floor: fill the bottom 24 rows.
	for (Int y = 0; y < 24; ++y)
	{
		for (Int x = 0; x < 256; ++x)
		{
			sink->ApplyEdit({ x, y, MaterialRegistry::GetStone(), kEditOpWrite });
			collision.SetSolid(x, y, true);
		}
	}
	// Sand pockets above the floor (sand is passable - no mask).
	for (Int y = 24; y < 40; ++y)
	{
		for (Int x = 100; x < 156; ++x)
			sink->ApplyEdit({ x, y, MaterialRegistry::GetSand(), kEditOpWrite });
	}
	std::cout << "[NoitaLike] terrain pre-placed (stone floor + sand pockets)" << std::endl;
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
	// Player spawns standing on the stone floor (stone top is y=24; half-size
	// 1.2 -> pos.y 25.5 rests on it). Spawning mid-air (y=96) made the player
	// slowly sink for ~6s under gravity before touching down.
	ecs.GetComponent<Gameplay::TransformComp>(m_player)->pos = glm::vec2(128.0f, 25.5f);
	ecs.GetComponent<Gameplay::TransformComp>(m_player)->half_size = glm::vec2(1.2f, 1.2f);
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

	// Trail component: Unity LineRenderer-style, follows the entity lifecycle
	// (TrailUpdateSystem appends its position every tick; the trail vanishes
	// with the entity).
	auto* lr = ecs.AddComponent<Render::LineRendererComponent>(proj);
	lr->SetWidth(0.35f, 0.15f);    // 头粗尾细（世界单位）
	lr->SetColor(glm::vec4(1.0f, 0.85f, 0.4f, 1.0f), glm::vec4(1.0f, 0.35f, 0.15f, 1.0f));  // 橙→红
	lr->AddPoint(ptf->pos);        // 初始点

	std::cout << "[NoitaLike] shot at (" << aim_world.x << ", " << aim_world.y << ")" << std::endl;
}

void NoitaLikeApp::SyncHud()
{
	// Fill the render-minimum snapshot (logic thread, tick end). The bound UI
	// fields (m_hp/m_score/m_wand) are written on the RENDER thread from these
	// values in the UI pass - the logic thread never touches them anymore.
	World::FrameSnapshot* snap = world_ ? world_->GetWriteSnapshot() : nullptr;
	if (!snap)
		return;
	snap->frame_tick = GetSimTick();

	auto& ecs = GetWorld()->GetECS();

	// Player quad + camera (render-thread reads these from the snapshot).
	if (auto* tf = ecs.GetComponent<Gameplay::TransformComp>(m_player))
	{
		snap->player_pos = tf->pos;
		snap->player_half_size = tf->half_size;
	}
	snap->camera_mvp = camera_.GetViewProjectionMatrix();

	// HUD values.
	if (auto* hp = ecs.GetComponent<Gameplay::HealthComp>(m_player))
		snap->hud_hp = (Int)hp->hp;
	snap->hud_score = (Int)(GetSimTick() / 60);
	snap->hud_wand = 0;
}

void NoitaLikeApp::OnPreRender(Render::FrameContext& ctx)
{
	// Render thread: capture the frame's snapshot for the execute lambdas.
	m_render_snapshot = ctx.snapshot;
	// Inject the frame number + camera MVP for the line pass (execute lambdas
	// cannot see the FrameContext - this is the single render-side entry).
	Render::LineRendererManager::Get().SetRenderFrame(ctx.frame_number,
		ctx.snapshot ? ctx.snapshot->camera_mvp : glm::mat4(1.0f));
	GameApp::OnPreRender(ctx);
}

void NoitaLikeApp::OnGameTick()
{
	HandleInput();
	SyncHud();

	// [Player-probe] temporary player telemetry.
	{
		static UInt32 s_probe = 0;
		if (++s_probe % 60 == 0)
		{
			auto& ecs = world_->GetECS();
			auto* tf = ecs.GetComponent<Gameplay::TransformComp>(m_player);
			auto* vel = ecs.GetComponent<Gameplay::VelocityComp>(m_player);
			if (tf && vel)
				std::cout << "[Player] pos=(" << tf->pos.x << "," << tf->pos.y
					<< ") vel=(" << vel->vel.x << "," << vel->vel.y
					<< ") solid_below=" << world_->GetCollision().IsSolid((Int)tf->pos.x, (Int)(tf->pos.y - tf->half_size.y))
					<< " solid_here=" << world_->GetCollision().IsSolid((Int)tf->pos.x, (Int)tf->pos.y) << std::endl;
		}
	}

	// [DX] ENQUEUE_RENDER_COMMAND demo: every 120 ticks (2s) enqueue a command
	// that executes on the render thread (flush point in RenderThreadMain);
	// the fence proves consumption on a later tick. Logged ids let you compare
	// against the logic-thread id printed in OnGameInit.
	if (++m_dx_demo_counter % 120 == 0)
	{
		// Macro path: fire-and-forget statement (tag type is local, name
		// stringified into the profiling label).
		ENQUEUE_RENDER_COMMAND(DxDemoCommand)([]()
		{
			ASSERT_RENDER_THREAD();
			std::cout << "[DX-Demo] macro command executed on render thread "
				<< std::this_thread::get_id() << std::endl;
		});
		// Template path: returns a fence for consumption tracking.
		m_dx_demo_fence = EnqueueRenderCommand<DxDemoFenceTag>("DxDemoFence", []()
		{
			ASSERT_RENDER_THREAD();
		});
		m_dx_demo_pending = true;
	}
	if (m_dx_demo_pending && m_dx_demo_fence.IsComplete())
	{
		std::cout << "[DX-Demo] fence complete (render thread consumed the command)" << std::endl;
		m_dx_demo_pending = false;
	}

	// RmlUI per-frame update (layout dirty-mark propagation, data binding
	// refresh, animations). Same call as RmlUIDemo::OnUpdate - without it
	// data-style-width/data-attr-text never refresh and the HUD stays inert.
	MXRender::UI::UIManager::Get().Update(1.0f / 60.0f);

	// One simulation tick per game tick (consumed by the sim pass on the
	// render thread).
	m_pending_ticks.fetch_add(1);
	// Camera stays at the whole-world overview (see OnGameInit) until player
	// collision exists (Phase 4) - following the player would chase it into
	// the void once it falls through the world.
}

void NoitaLikeApp::OnShutdownScene()
{
	// Player sprite buffers (SRB is owned by PlayerPassData::Release).
	delete m_player_vb; m_player_vb = nullptr;
	delete m_player_ib; m_player_ib = nullptr;
	delete m_player_params; m_player_params = nullptr;

	m_world_renderer.Shutdown();
	MXRender::UI::UIManager::Destroy();
	GameApp::OnShutdownScene();
	// Last: world (entities/components) died above; no collector runs after.
	Render::LineRendererManager::Destroy();
}

int main()
{
	NoitaLikeApp app;
	return SampleApp::RunSample(app, "MXRender NoitaLike");
}
