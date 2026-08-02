#pragma once
#ifndef _GAME_APP_
#define _GAME_APP_

#include "Application/SampleApp.h"
#include "Application/FixedTickClock.h"
#include "Render/View/Camera2D.h"
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)
class GameWorld;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

// Game application base: owns the fixed-tick loop and the GameWorld.
// Subclasses implement CreateGameWorld (factory) and the render/HUD wiring.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GameApp, public Application::SampleApp)
#pragma region METHOD
public:
	GameApp() MYDEFAULT;
	VIRTUAL ~GameApp() MYDEFAULT;

	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE;
	VIRTUAL void OnUpdate(float dt) OVERRIDE FINAL;
	// Hand the current tick's snapshot slot to the render thread (FrameContext
	// carries it to OnPreRender/execute lambdas). NULL for no-world apps.
	VIRTUAL void METHOD(OnPrepareFrameContext)(Render::FrameContext& ctx) OVERRIDE;

	// Subclass hooks
	VIRTUAL World::GameWorld* METHOD(CreateGameWorld)() PURE;
	VIRTUAL void METHOD(OnGameInit)() {}
	VIRTUAL void METHOD(OnGameTick)() {}

	World::GameWorld* METHOD(GetWorld)() CONST { return world_.get(); }
	Render::Camera2D& METHOD(GetCamera)() { return camera_; }
	UInt64 METHOD(GetSimTick)() CONST { return sim_tick_; }

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	UniquePtr<World::GameWorld> world_;
	Render::Camera2D camera_;
	FixedTickClock tick_clock_;
	UInt64 sim_tick_ = 0;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAME_APP_