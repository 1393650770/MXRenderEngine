#pragma once
#ifndef _UI_PREVIEW_DATA_
#define _UI_PREVIEW_DATA_

#include "Core/ConstDefine.h"
#include "UI/Widget/UIWidgetMacros.h"

/// Sample data model bound to the in-editor preview document. MetaParser
/// scans src/Editor headers and generates UIWidgetBindingTraits<UIPreviewData>
/// (src/_Generated/RmlUI/UIPreviewData.UIBinding.Gen.h) on build.
class UIPreviewData
{
public:
	UI_BIND(Enable, FIELD_AS=hp)
	int m_hp = 80;

	UI_BIND(Enable, FIELD_AS=score)
	int m_score = 1000;

	UI_BIND(Enable, FIELD_AS=player_name)
	String m_player_name = "Editor";

	UI_BIND(Enable, TWO_WAY)
	int m_volume = 50;
};

#endif // !_UI_PREVIEW_DATA_
