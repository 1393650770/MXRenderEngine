#pragma once
#ifndef _UI_PALETTE_PANEL_
#define _UI_PALETTE_PANEL_

#include "Core/ConstDefine.h"
#include "UI/BasePanel.h"
#include "UI/UIDesigner/UIWidgetTemplate.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// Widget palette: drag a template into the preview canvas, or double-click
/// to insert at the canvas center. The drag payload is the template NAME
/// ("UI_WIDGET_TEMPLATE") — the canvas resolves it through Templates().
class UIPalettePanel : public UI::BasePanel
{
public:
	static String GetTypeName() { return "UI Palette"; }

	UIPalettePanel(const String& in_name, Bool in_show);
	void Init() override;
	void Update() override;
	void Draw() override;
	void Release() override;

	static const Vector<UIWidgetTemplate>& Templates();
	static const UIWidgetTemplate* FindTemplate(const String& name);
	static const char* const kDragPayload;   // "UI_WIDGET_TEMPLATE"

private:
	void InsertTemplate(const UIWidgetTemplate& tpl, Float32 x, Float32 y);
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_PALETTE_PANEL_
