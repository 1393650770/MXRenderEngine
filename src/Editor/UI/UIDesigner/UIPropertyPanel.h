#pragma once
#ifndef _UI_PROPERTY_PANEL_
#define _UI_PROPERTY_PANEL_

#include "Core/ConstDefine.h"
#include "UI/BasePanel.h"
#include "UI/UIDesigner/UIDesignerState.h"
#include "UI/UIDocumentModel/UIDocumentModel.h"

namespace MXRender::Tool::CssToRcss { struct RcssPropertySpec; }

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// Designer property panel for the selected element: an element tree for
/// navigation, text/attribute editing, and an RCSS editor whose widgets are
/// driven by the CssToRcssLib whitelist value families (Color → ColorEdit4,
/// Keyword → Combo, Length → DragFloat+unit, ...). Every edit goes through
/// UIDesignerState::SetPropertyUndoable etc. → RCSS #id rules → reload.
class UIPropertyPanel : public UI::BasePanel
{
public:
	static String GetTypeName() { return "UI Properties"; }

	UIPropertyPanel(const String& in_name, Bool in_show);
	void Init() override;
	void Update() override;
	void Draw() override;
	void Release() override;

private:
	void DrawNodeTree(UIDocModel::UIDocumentNode* node);   // recursive outline
	/// One RCSS property row; returns true when a new value should be committed.
	bool EditPropertyRow(const String& id, const String& name, const String& cur_value,
		String& out_new_value);
	void DrawAddPropertyCombo(const String& id);
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_PROPERTY_PANEL_
