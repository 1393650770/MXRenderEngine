#pragma once
#ifndef _RMLUIDEMO_
#define _RMLUIDEMO_

#include "Core/ConstDefine.h"
#include "Application/SampleApp.h"
#include "UI/UIHandleTypes.h"
#include "UI/Widget/UIWidgetMacros.h"

// Zero RmlUI backend includes — only generic UI + UIWidget headers

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIManager;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

class RmlUIDemoApp : public MXRender::Application::SampleApp
{
#pragma region METHOD
public:
	RmlUIDemoApp();
	VIRTUAL ~RmlUIDemoApp();

	VIRTUAL void METHOD(OnInitScene)() OVERRIDE;
	VIRTUAL void METHOD(OnShutdownScene)() OVERRIDE;
	VIRTUAL void METHOD(OnUpdate)(float dt) OVERRIDE;

	/// UI event callback — zero Rml parameters.
	void METHOD(OnHeal)();

protected:
private:
#pragma endregion

#pragma region MEMBER
public:
	// Class marker — enables MetaParser discovery of UIBind annotations

	// OneWay fields
	UI_BIND(Enable, FIELD_AS=hp, EVENT = OnHeal)
	int   m_hp = 100;
	UI_BIND(Enable, FIELD_AS=score)
	int   m_score = 0;
	UI_BIND(Enable, FIELD_AS=timer)
	float m_timer = 0.0f;

	// display name == field name
	UI_BIND(Enable)
	String m_player_name = "Player2222";

	// TwoWay field (RML range input <-> C++)
	UI_BIND(Enable, TWO_WAY)
	int m_volume = 80;

	// Previous values for manual diff
	int   m_prev_hp = 100;
	int   m_prev_score = 0;
	float m_prev_timer = 0.0f;
protected:
private:
	MXRender::UI::UIModelHandle m_hud_model;
	MXRender::UI::UIDocHandle   m_hud_doc;
#pragma endregion
};
#endif // !_RMLUIDEMO_
