#pragma once
#ifndef _UI_DESIGNER_STATE_
#define _UI_DESIGNER_STATE_

#include "Core/ConstDefine.h"
#include "UI/UIDocumentModel/UIDocumentModel.h"
#include "UI/UIDesigner/UIWidgetTemplate.h"
#include "UI/RenderGraphEditor/Commands/EditorCommandQueue.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// Shared state of the in-editor UI designer: the document being edited
/// (UIDocument + UIStyleSheet IRs), the current selection, and the undo/redo
/// pipeline. Owned by UIPreviewPanel; palette/property panels reach it via
/// Get().
///
/// Write-back pipeline (the ONLY persistence path) — ApplyMutation:
///   mutate IR → serialize → atomic write source+output __preview__ files →
///   UIManager::ReloadAllDocuments() (thread-safe dispatch; inline in Single
///   mode). Inline-style transient overlays are NEVER persisted — hot reload
///   re-reads only the stylesheet (documented RmlUi limitation), which is
///   exactly why drag previews use them.
class UIDesignerState
{
public:
	/// Full-text document snapshot (the undo/redo currency — UI documents are
	/// tiny, so snapshotting is simple and always correct).
	struct Snapshot
	{
		String rml_text;
		String rcss_text;
	};

	struct Box
	{
		Float32 x = 0, y = 0, w = 0, h = 0;
	};

	static UIDesignerState& Get();
	static void SetInstance(UIDesignerState* inst);   // called by UIPreviewPanel::Init

	// ---- Document ----
	void EnsureLoaded();          // parse the current __preview__ files into the IRs (idempotent)
	bool IsLoaded() const { return m_loaded; }
	UIDocModel::UIDocument& Rml() { return m_doc; }
	UIDocModel::UIStyleSheet& Rcss() { return m_ss; }

	// ---- Selection ----
	void Select(const String& id) { m_selection = id; }
	const String& GetSelection() const { return m_selection; }
	/// Recursive id → node lookup (body-level first).
	UIDocModel::UIDocumentNode* FindNodeById(const String& id);
	/// Fresh id not present in the document ("ui_<n>").
	String MakeUniqueId(const String& prefix = "ui_");

	// ---- Mutation pipeline (the ONLY persistence path) ----
	/// Apply fn to the IRs, serialize, write both file copies, reload preview.
	void ApplyMutation(const std::function<void()>& fn);
	Snapshot Capture() const;
	/// Restore a snapshot (IRs + files + reload). Drops the selection if gone.
	void Restore(const Snapshot& snap);

	// ---- Undoable convenience entries (the command ctor applies the change) ----
	void InsertNodeUndoable(const UIWidgetTemplate& tpl, const String& parent_id,
		Float32 x, Float32 y);
	void DeleteNodeUndoable(const String& id);
	void SetBoxUndoable(const String& id, const Box& old_box, const Box& new_box);
	void SetPropertyUndoable(const String& id, const String& name,
		const String& old_value, const String& new_value);   // "" new_value = remove
	void SetAttributeUndoable(const String& id, const String& name,
		const String& old_value, const String& new_value);   // "" new_value = remove
	void SetTextUndoable(const String& id, const String& old_text, const String& new_text);

	// ---- Per-frame plumbing ----
	/// Drain the command queue into the history. Call once per frame
	/// (UIPreviewPanel::Update).
	void Tick();
	/// Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y against this designer's history. Call
	/// from each designer panel's Draw, gated by IsWindowHovered (at most one
	/// panel is hovered per frame, so no double-undo).
	void HandleUndoRedoKeys();

	// ---- RCSS rule helpers (used by commands + the property panel) ----
	static UIDocModel::UIRuleSet* FindIdRule(UIDocModel::UIStyleSheet& ss, const String& id);
	static UIDocModel::UIRuleSet& FindOrCreateIdRule(UIDocModel::UIStyleSheet& ss, const String& id);
	static void SetRuleProperty(UIDocModel::UIRuleSet& rule, const String& name, const String& value);
	static void RemoveRuleProperty(UIDocModel::UIRuleSet& rule, const String& name);
	/// Removes the #id rules of the node and its whole subtree (delete cleanup).
	static void RemoveIdRulesRecursive(UIDocModel::UIStyleSheet& ss, UIDocModel::UIDocumentNode* node);

	EditorCommandQueue cmd_queue;
	CommandHistory command_history;

private:
	void WriteFiles(const String& rml_text, const String& rcss_text);

	UIDocModel::UIDocument m_doc;
	UIDocModel::UIStyleSheet m_ss;
	bool m_loaded = false;
	String m_selection;
	UInt32 m_id_counter = 1;

	static UIDesignerState* s_instance;
};

/// Project source root (the dir containing resource/RmlUI), walked up from
/// cwd — the editor runs with cwd = build output, sources live in the repo.
String FindProjectRoot();

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_DESIGNER_STATE_
