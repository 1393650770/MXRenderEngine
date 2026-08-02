#include "Application/GameApp.h"
#include "World/GameWorld.h"
#include "Render/Core/RenderFrameData.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

void GameApp::OnInitScene()
{
	world_.reset(CreateGameWorld());
	CHECK_WITH_LOG(world_ == nullptr, "GameApp: CreateGameWorld returned null")

	camera_.SetViewport(GetViewportWidth(), GetViewportHeight());
	camera_.SetOrthoSize(30.0f);   // 30 world units tall (Noita-like scale)
	camera_.SetPosition(glm::vec2(0.0f, 0.0f));
	camera_.UpdateMatrices();

	OnGameInit();
}

void GameApp::OnShutdownScene()
{
	world_.reset();
}

void GameApp::OnUpdate(float dt)
{
	tick_clock_.Advance(dt);
	UInt32 ticks = tick_clock_.PopTicks(1.0f / 60.0f, 4);
	for (UInt32 i = 0; i < ticks; ++i)
	{
		if (world_)
			world_->Tick();
		++sim_tick_;
		OnGameTick();
	}
}

void GameApp::OnPrepareFrameContext(Render::FrameContext& ctx)
{
	// Current snapshot slot (already rotated by Tick; the app filled it in
	// OnGameTick). The render thread reads this pointer only.
	ctx.snapshot = world_ ? world_->GetWriteSnapshot() : nullptr;
}

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender