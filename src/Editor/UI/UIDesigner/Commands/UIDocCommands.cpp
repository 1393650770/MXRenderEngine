#include "UIDocCommands.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

using namespace MXRender::UI::UIDocModel;

UIDocSnapshotCmd::UIDocSnapshotCmd(String desc, UIDesignerState* state,
	UIDesignerState::Snapshot before, UIDesignerState::Snapshot after)
	: m_state(state), m_before(std::move(before)), m_after(std::move(after))
	, m_desc(std::move(desc)) {}

namespace
{
/// Appends the template tree to parent (nullptr = body). Every element gets
/// its own unique id + a #id rule built from its template properties; the
/// root additionally gets position/left/top (the canvas drop contract).
UIDocumentNode* BuildNodeTree(const UIWidgetTemplate& tpl, UIDocumentNode* parent,
	UIDesignerState& state, Float32 root_x, Float32 root_y, bool is_root)
{
	auto node = std::make_unique<UIDocumentNode>();
	node->type = ENodeType::Element;
	node->tag_name = tpl.tag;
	node->attributes = tpl.attributes;
	node->source_line = 0;

	const String id = state.MakeUniqueId();
	node->SetAttribute("id", id);
	UIRuleSet& rule = UIDesignerState::FindOrCreateIdRule(state.Rcss(), id);
	for (const auto& p : tpl.properties)
		UIDesignerState::SetRuleProperty(rule, p.name, p.value);
	if (is_root)
	{
		UIDesignerState::SetRuleProperty(rule, "position", "absolute");
		UIDesignerState::SetRuleProperty(rule, "left", std::to_string((int)root_x) + "px");
		UIDesignerState::SetRuleProperty(rule, "top", std::to_string((int)root_y) + "px");
	}

	if (!tpl.text.empty())
	{
		auto text = std::make_unique<UIDocumentNode>();
		text->type = ENodeType::Text;
		text->text = tpl.text;
		text->parent = node.get();
		node->children.push_back(std::move(text));
	}
	for (const auto& child_tpl : tpl.children)
		BuildNodeTree(child_tpl, node.get(), state, 0, 0, false);

	UIDocumentNode* raw = node.get();
	raw->parent = parent;
	if (parent)
		parent->children.push_back(std::move(node));
	else
		state.Rml().nodes.push_back(std::move(node));
	return raw;
}
} // namespace

// =========================================================================
// InsertNodeCmd
// =========================================================================

InsertNodeCmd::InsertNodeCmd(String desc, UIDesignerState* state,
	const UIWidgetTemplate& tpl, String parent_id, Float32 x, Float32 y)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	state->ApplyMutation([&]()
	{
		UIDocumentNode* parent = parent_id.empty() ? nullptr : state->FindNodeById(parent_id);
		UIDocumentNode* inserted = BuildNodeTree(tpl, parent, *state, x, y, true);
		state->Select(inserted->GetId());
	});
	m_after = state->Capture();
}

// =========================================================================
// UIDeleteNodeCmd
// =========================================================================

UIDeleteNodeCmd::UIDeleteNodeCmd(String desc, UIDesignerState* state, String id)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	state->ApplyMutation([&]()
	{
		UIDocumentNode* node = state->FindNodeById(id);
		if (!node) return;
		// Remove the #id rules FIRST — RemoveIdRulesRecursive walks the node
		// tree, which must still be alive (erasing the node first leaves a
		// dangling pointer here → heap corruption / hang).
		UIDesignerState::RemoveIdRulesRecursive(state->Rcss(), node);
		if (node->parent)
		{
			auto& siblings = node->parent->children;
			for (auto it = siblings.begin(); it != siblings.end(); ++it)
				if (it->get() == node) { siblings.erase(it); break; }
		}
		else
		{
			auto& tops = state->Rml().nodes;
			for (auto it = tops.begin(); it != tops.end(); ++it)
				if (it->get() == node) { tops.erase(it); break; }
		}
		state->Select("");
	});
	m_after = state->Capture();
}

// =========================================================================
// SetBoxCmd
// =========================================================================

SetBoxCmd::SetBoxCmd(String desc, UIDesignerState* state, String id,
	UIDesignerState::Box old_box, UIDesignerState::Box new_box)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	(void)old_box;   // kept in the snapshot; the API shape mirrors drag commits
	state->ApplyMutation([&]()
	{
		UIRuleSet& rule = UIDesignerState::FindOrCreateIdRule(state->Rcss(), id);
		UIDesignerState::SetRuleProperty(rule, "position", "absolute");
		UIDesignerState::SetRuleProperty(rule, "left", std::to_string((int)new_box.x) + "px");
		UIDesignerState::SetRuleProperty(rule, "top", std::to_string((int)new_box.y) + "px");
		UIDesignerState::SetRuleProperty(rule, "width", std::to_string((int)new_box.w) + "px");
		UIDesignerState::SetRuleProperty(rule, "height", std::to_string((int)new_box.h) + "px");
	});
	m_after = state->Capture();
}

// =========================================================================
// SetPropertyCmd
// =========================================================================

SetPropertyCmd::SetPropertyCmd(String desc, UIDesignerState* state, String id,
	String name, String old_value, String new_value)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	(void)old_value;
	state->ApplyMutation([&]()
	{
		UIRuleSet& rule = UIDesignerState::FindOrCreateIdRule(state->Rcss(), id);
		if (new_value.empty())
			UIDesignerState::RemoveRuleProperty(rule, name);
		else
			UIDesignerState::SetRuleProperty(rule, name, new_value);
	});
	m_after = state->Capture();
}

// =========================================================================
// SetAttributeCmd
// =========================================================================

SetAttributeCmd::SetAttributeCmd(String desc, UIDesignerState* state, String id,
	String name, String old_value, String new_value)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	(void)old_value;
	state->ApplyMutation([&]()
	{
		UIDocumentNode* node = state->FindNodeById(id);
		if (!node) return;
		if (new_value.empty())
		{
			for (auto it = node->attributes.begin(); it != node->attributes.end(); ++it)
				if (it->name == name) { node->attributes.erase(it); break; }
		}
		else
		{
			node->SetAttribute(name, new_value);
		}
	});
	m_after = state->Capture();
}

// =========================================================================
// SetTextCmd
// =========================================================================

SetTextCmd::SetTextCmd(String desc, UIDesignerState* state, String id,
	String old_text, String new_text)
	: UIDocSnapshotCmd(std::move(desc), state, state->Capture(), {})
{
	(void)old_text;
	state->ApplyMutation([&]()
	{
		UIDocumentNode* node = state->FindNodeById(id);
		if (!node) return;
		// Replace the first text child; drop the rest of the text nodes.
		for (auto it = node->children.begin(); it != node->children.end();)
		{
			if ((*it)->type == ENodeType::Text)
			{
				if (new_text.empty())
					it = node->children.erase(it);
				else
				{
					(*it)->text = new_text;
					++it;
					// remove any further text nodes
					while (it != node->children.end() && (*it)->type == ENodeType::Text)
						it = node->children.erase(it);
				}
			}
			else
			{
				++it;
			}
		}
		if (!new_text.empty() && node->children.empty())
		{
			auto text = std::make_unique<UIDocumentNode>();
			text->type = ENodeType::Text;
			text->text = new_text;
			text->parent = node;
			node->children.push_back(std::move(text));
		}
	});
	m_after = state->Capture();
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
