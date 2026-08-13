#include "UIDesignerState.h"
#include "UI/UIDesigner/Commands/UIDocCommands.h"
#include "UI/UIDocumentModel/RmlParser.h"
#include "UI/UIDocumentModel/UIRcssParser.h"
#include "UI/UIDocumentModel/UIDocumentSerializer.h"
#include "UI/UIManager.h"
#include "Platform/PlatformFile.h"

#include <imgui.h>
#include <filesystem>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

using namespace MXRender::UI::UIDocModel;

UIDesignerState* UIDesignerState::s_instance = nullptr;

// =========================================================================
// Singleton plumbing
// =========================================================================

UIDesignerState& UIDesignerState::Get()
{
	// Fallback for the panels' very first Draw before UIPreviewPanel::Init
	// (Single mode makes that impossible in practice, but stay safe).
	static UIDesignerState s_fallback;
	return s_instance ? *s_instance : s_fallback;
}

void UIDesignerState::SetInstance(UIDesignerState* inst)
{
	s_instance = inst;
}

// =========================================================================
// Document loading
// =========================================================================

void UIDesignerState::EnsureLoaded()
{
	if (m_loaded) return;
	// Parse the preview files exactly as hot reload sees them (cwd-relative).
	Vector<UInt8> rml_bytes, rcss_bytes;
	if (Platform::PlatformFile::ReadFile("RmlUI/__preview__.rml", rml_bytes))
	{
		String text(rml_bytes.begin(), rml_bytes.end());
		m_doc = RmlParser::Parse(text, "RmlUI/__preview__.rml");
	}
	if (Platform::PlatformFile::ReadFile("RmlUI/__preview__.rcss", rcss_bytes))
	{
		String text(rcss_bytes.begin(), rcss_bytes.end());
		m_ss = UIRcssParser::Parse(text, "RmlUI/__preview__.rcss");
	}
	m_loaded = true;
}

// =========================================================================
// Selection / lookup
// =========================================================================

UIDocumentNode* UIDesignerState::FindNodeById(const String& id)
{
	std::function<UIDocumentNode*(UIDocumentNode*)> visit =
		[&](UIDocumentNode* node) -> UIDocumentNode*
	{
		if (node->type == ENodeType::Element && node->GetId() == id)
			return node;
		for (auto& child : node->children)
			if (auto* hit = visit(child.get()))
				return hit;
		return nullptr;
	};
	for (auto& top : m_doc.nodes)
		if (auto* hit = visit(top.get()))
			return hit;
	return nullptr;
}

String UIDesignerState::MakeUniqueId(const String& prefix)
{
	for (;;)
	{
		String cand = prefix + std::to_string(m_id_counter++);
		if (!FindNodeById(cand))
			return cand;
	}
}

// =========================================================================
// Mutation pipeline
// =========================================================================

void UIDesignerState::ApplyMutation(const std::function<void()>& fn)
{
	if (!m_loaded) return;
	try
	{
		fn();
		WriteFiles(UIDocumentSerializer::SerializeRml(m_doc),
			UIDocumentSerializer::SerializeRcss(m_ss));
		UIManager::Get().ReloadAllDocuments();
	}
	catch (const std::exception& e)
	{
		std::cerr << "[UIDesigner] mutation failed: " << e.what() << std::endl;
	}
}

UIDesignerState::Snapshot UIDesignerState::Capture() const
{
	Snapshot snap;
	snap.rml_text = UIDocumentSerializer::SerializeRml(m_doc);
	snap.rcss_text = UIDocumentSerializer::SerializeRcss(m_ss);
	return snap;
}

void UIDesignerState::Restore(const Snapshot& snap)
{
	if (!m_loaded) return;
	// Re-parse the snapshot text back into the IRs — keeps the IR canonical
	// (parent pointers rebuilt) and in-memory == on-disk.
	m_doc = RmlParser::Parse(snap.rml_text, m_doc.file_path);
	m_ss = UIRcssParser::Parse(snap.rcss_text, m_ss.file_path);
	WriteFiles(snap.rml_text, snap.rcss_text);
	UIManager::Get().ReloadAllDocuments();
	if (!m_selection.empty() && !FindNodeById(m_selection))
		m_selection.clear();
}

void UIDesignerState::WriteFiles(const String& rml_text, const String& rcss_text)
{
	Vector<UInt8> rml_bytes(rml_text.begin(), rml_text.end());
	Vector<UInt8> rcss_bytes(rcss_text.begin(), rcss_text.end());
	Platform::PlatformFile::WriteFileAtomic("RmlUI/__preview__.rml", rml_bytes);
	Platform::PlatformFile::WriteFileAtomic("RmlUI/__preview__.rcss", rcss_bytes);
	const String root = FindProjectRoot();
	if (!root.empty())
	{
		Platform::PlatformFile::WriteFileAtomic(
			root + "/resource/RmlUI/__preview__.rml", rml_bytes);
		Platform::PlatformFile::WriteFileAtomic(
			root + "/resource/RmlUI/__preview__.rcss", rcss_bytes);
	}
}

// =========================================================================
// Undoable convenience entries
// =========================================================================

void UIDesignerState::InsertNodeUndoable(const UIWidgetTemplate& tpl,
	const String& parent_id, Float32 x, Float32 y)
{
	cmd_queue.Enqueue(std::make_unique<InsertNodeCmd>(
		"Insert " + tpl.name, this, tpl, parent_id, x, y));
}

void UIDesignerState::DeleteNodeUndoable(const String& id)
{
	cmd_queue.Enqueue(std::make_unique<UIDeleteNodeCmd>("Delete #" + id, this, id));
}

void UIDesignerState::SetBoxUndoable(const String& id, const Box& old_box, const Box& new_box)
{
	cmd_queue.Enqueue(std::make_unique<SetBoxCmd>("Move #" + id, this, id, old_box, new_box));
}

void UIDesignerState::SetPropertyUndoable(const String& id, const String& name,
	const String& old_value, const String& new_value)
{
	cmd_queue.Enqueue(std::make_unique<SetPropertyCmd>(
		"Set " + name + " on #" + id, this, id, name, old_value, new_value));
}

void UIDesignerState::SetAttributeUndoable(const String& id, const String& name,
	const String& old_value, const String& new_value)
{
	cmd_queue.Enqueue(std::make_unique<SetAttributeCmd>(
		"Set " + name + " on #" + id, this, id, name, old_value, new_value));
}

void UIDesignerState::SetTextUndoable(const String& id, const String& old_text, const String& new_text)
{
	cmd_queue.Enqueue(std::make_unique<SetTextCmd>(
		"Edit text on #" + id, this, id, old_text, new_text));
}

// =========================================================================
// Per-frame plumbing
// =========================================================================

void UIDesignerState::Tick()
{
	cmd_queue.ProcessAll(command_history);
}

void UIDesignerState::HandleUndoRedoKeys()
{
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantTextInput) return;   // typing in an edit box — don't steal Ctrl+Z
	const bool ctrl = io.KeyCtrl || io.KeySuper;
	if (!ctrl) return;
	const bool shift = io.KeyShift;
	if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
	{
		if (shift) command_history.Redo();
		else command_history.Undo();
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
	{
		command_history.Redo();
	}
}

// =========================================================================
// RCSS rule helpers
// =========================================================================

UIRuleSet* UIDesignerState::FindIdRule(UIStyleSheet& ss, const String& id)
{
	const String target = "#" + id;
	for (auto& rule : ss.rules)
		for (const auto& sel : rule.selectors)
			if (sel == target)   // exact match — never hijack `div#id` / `body #id`
				return &rule;
	return nullptr;
}

UIRuleSet& UIDesignerState::FindOrCreateIdRule(UIStyleSheet& ss, const String& id)
{
	if (auto* rule = FindIdRule(ss, id))
		return *rule;
	UIRuleSet rule;
	rule.selectors.push_back("#" + id);
	rule.source_line = 0;
	ss.rules.push_back(std::move(rule));
	return ss.rules.back();
}

void UIDesignerState::SetRuleProperty(UIRuleSet& rule, const String& name, const String& value)
{
	for (auto& p : rule.properties)
		if (p.name == name) { p.value = value; return; }
	rule.properties.push_back(UICssProperty{ name, value, 0 });
}

void UIDesignerState::RemoveRuleProperty(UIRuleSet& rule, const String& name)
{
	for (size_t i = 0; i < rule.properties.size(); ++i)
		if (rule.properties[i].name == name)
		{
			rule.properties.erase(rule.properties.begin() + i);
			return;
		}
}

void UIDesignerState::RemoveIdRulesRecursive(UIStyleSheet& ss, UIDocumentNode* node)
{
	if (!node) return;
	if (node->type == ENodeType::Element)
	{
		const String id = node->GetId();
		if (!id.empty())
		{
			for (auto it = ss.rules.begin(); it != ss.rules.end();)
			{
				bool matches = false;
				for (const auto& sel : it->selectors)
					if (sel == "#" + id) { matches = true; break; }
				if (matches) it = ss.rules.erase(it);
				else ++it;
			}
		}
	}
	for (auto& child : node->children)
		RemoveIdRulesRecursive(ss, child.get());
}

// =========================================================================
// Project root discovery
// =========================================================================

String FindProjectRoot()
{
	std::error_code ec;
	auto dir = std::filesystem::current_path(ec);
	if (ec) return "";
	for (int i = 0; i < 8; ++i)
	{
		if (std::filesystem::is_directory(dir / "resource" / "RmlUI", ec))
			return dir.string();
		ec.clear();
		dir = dir.parent_path();
		if (dir.empty()) break;
	}
	return "";
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
