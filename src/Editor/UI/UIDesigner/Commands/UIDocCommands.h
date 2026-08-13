#pragma once
#ifndef _UI_DOC_COMMANDS_
#define _UI_DOC_COMMANDS_

#include "Core/ConstDefine.h"
#include "UI/RenderGraphEditor/Commands/CommandHistory.h"
#include "UI/UIDesigner/UIDesignerState.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// Snapshot-based undo commands for the UI designer. Each command captures
/// the full document text before/after its own mutation AT CONSTRUCTION time
/// (constructing already applies the change); Execute/Undo restore whole
/// snapshots. Correct by construction at O(document) per step — UI documents
/// are tiny, so snapshotting beats diffing.
class UIDocSnapshotCmd : public Command
{
public:
	UIDocSnapshotCmd(String desc, UIDesignerState* state,
		UIDesignerState::Snapshot before, UIDesignerState::Snapshot after);
	void Execute() override { if (m_state) m_state->Restore(m_after); }
	void Undo() override { if (m_state) m_state->Restore(m_before); }
	String GetDescription() const override { return m_desc; }

protected:
	UIDesignerState* m_state = nullptr;
	UIDesignerState::Snapshot m_before;
	UIDesignerState::Snapshot m_after;
	String m_desc;
};

/// Insert a widget from a palette template into parent ("" = body) at (x, y)
/// px. The new element gets a unique id + an absolute-positioned #id rule.
class InsertNodeCmd : public UIDocSnapshotCmd
{
public:
	InsertNodeCmd(String desc, UIDesignerState* state, const UIWidgetTemplate& tpl,
		String parent_id, Float32 x, Float32 y);
};

/// Delete an element + its subtree + all their #id rules.
class UIDeleteNodeCmd : public UIDocSnapshotCmd
{
public:
	UIDeleteNodeCmd(String desc, UIDesignerState* state, String id);
};

/// Commit a box (position/size) into the element's #id rule
/// (position/left/top/width/height — never inline style; hot reload re-reads
/// only the stylesheet).
class SetBoxCmd : public UIDocSnapshotCmd
{
public:
	SetBoxCmd(String desc, UIDesignerState* state, String id,
		UIDesignerState::Box old_box, UIDesignerState::Box new_box);
};

/// Set ("" = remove) one RCSS property on the element's #id rule.
class SetPropertyCmd : public UIDocSnapshotCmd
{
public:
	SetPropertyCmd(String desc, UIDesignerState* state, String id,
		String name, String old_value, String new_value);
};

/// Set ("" = remove) one RML attribute on the element.
class SetAttributeCmd : public UIDocSnapshotCmd
{
public:
	SetAttributeCmd(String desc, UIDesignerState* state, String id,
		String name, String old_value, String new_value);
};

/// Replace the element's first text-node content ("" removes it).
class SetTextCmd : public UIDocSnapshotCmd
{
public:
	SetTextCmd(String desc, UIDesignerState* state, String id,
		String old_text, String new_text);
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_DOC_COMMANDS_
