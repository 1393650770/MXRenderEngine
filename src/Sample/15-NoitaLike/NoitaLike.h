#pragma once
#ifndef _NOITA_LIKE_
#define _NOITA_LIKE_

#include "Application/GameApp.h"
#include "Application/CameraController2D.h"
#include "UI/Widget/UIWidgetMacros.h"
#include "UI/UIHandleTypes.h"
#include "ECS/ECSSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)
class GameWorld;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

// Noita-like sample app. UI_BIND fields MUST be public (MetaParser only
// generates binding code for public fields). UI_BIND annotations are only
// scanned in src/Sample (CompileFunc folder list) - keep the HUD state here.
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(NoitaLikeApp, public GameApp)
#pragma region METHOD
public:
	NoitaLikeApp() MYDEFAULT;
	VIRTUAL ~NoitaLikeApp() MYDEFAULT;

	VIRTUAL World::GameWorld* METHOD(CreateGameWorld)() OVERRIDE;
	VIRTUAL void METHOD(OnGameInit)() OVERRIDE;
	VIRTUAL void METHOD(OnGameTick)() OVERRIDE;
	VIRTUAL void METHOD(OnShutdownScene)() OVERRIDE;

	// ---- UI_BIND fields (public, scanned by MetaParser) ----
	UI_BIND(Enable, FIELD_AS=hp)
	Int m_hp = 100;
	UI_BIND(Enable, FIELD_AS=score)
	Int m_score = 0;
	UI_BIND(Enable, FIELD_AS=wand)
	Int m_wand = 0;

protected:
	void METHOD(HandleInput)();
	void METHOD(Shoot)();
	void METHOD(SpawnPlayer)();
	void METHOD(SyncHud)();

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	MXRender::Application::CameraController2D m_controller;
	MXRender::UI::UIModelHandle m_hud_model{};
	MXRender::UI::UIDocHandle m_hud_doc{};
	MXRender::ECS::EntityHandle m_player{};
	Bool m_prev_fire = false;
	Int m_prev_hp = -1;
	Int m_prev_score = -1;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NOITA_LIKE_