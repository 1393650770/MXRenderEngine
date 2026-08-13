#pragma once
#ifndef _UI_WIDGET_TEMPLATE_
#define _UI_WIDGET_TEMPLATE_

#include "Core/ConstDefine.h"
#include "UI/UIDocumentModel/UIDocumentModel.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// A palette-insertable UI widget definition: RML tag + attributes + inner
/// text + a starter #id rule (position/left/top are force-added at insert
/// time). Drawn from the DemoPanel / NoitaLikeHUD sample structures.
struct UIWidgetTemplate
{
	String name;                              // display name ("Button")
	String category;                          // "Layout" | "Basic" | "Control" | "Progress"
	String tag;                               // RML tag
	Vector<UIDocModel::UIAttribute> attributes;         // element attributes
	String text;                              // inner text ("" = none)
	Vector<UIDocModel::UICssProperty> properties;       // starter #id rule properties
	Vector<UIWidgetTemplate> children;        // nested widget tree (name/category unused)
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_WIDGET_TEMPLATE_
