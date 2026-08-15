#include "UIPropertyPanel.h"

#include "RcssWhitelist.h"   // CssToRcssLib (public include dir)
#include "UI/UIManager.h"

#include <imgui.h>
#include <cctype>
#include <cstdio>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

using namespace MXRender::UI::UIDocModel;
using namespace MXRender::Tool::CssToRcss;

UIPropertyPanel::UIPropertyPanel(const String& in_name, Bool in_show)
	: UI::BasePanel(in_name, in_show) {}

void UIPropertyPanel::Init() {}
void UIPropertyPanel::Update() {}
void UIPropertyPanel::Release() {}

// =========================================================================
// Value helpers (hex ⇄ rgba, length split, keyword lists)
// =========================================================================

namespace
{

bool HexToRGBA(const String& hex, float out[4])
{
	if (hex.empty() || hex[0] != '#') return false;
	const size_t len = hex.size() - 1;
	if (len != 6 && len != 8) return false;
	auto nibble = [&](size_t i) -> int
	{
		const char c = hex[1 + i];
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		return -1;
	};
	int r = 0, g = 0, b = 0, a = 255;
	for (int i = 0; i < 2; ++i)
	{
		int hi = nibble(i * 2), lo = nibble(i * 2 + 1);
		if (hi < 0 || lo < 0) return false;
		if (i == 0) r = (hi << 4) | lo;
		else if (i == 1) g = (hi << 4) | lo;
	}
	for (int i = 0; i < 2; ++i)
	{
		int hi = nibble(4 + i * 2), lo = nibble(5 + i * 2);
		if (hi < 0 || lo < 0) return false;
		if (i == 0) b = (hi << 4) | lo;
		else a = (hi << 4) | lo;
	}
	out[0] = r / 255.0f; out[1] = g / 255.0f;
	out[2] = b / 255.0f; out[3] = a / 255.0f;
	return true;
}

String RGBAtoHex(const float c[4])
{
	char buf[16];
	snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X",
		(int)(c[0] * 255.0f + 0.5f), (int)(c[1] * 255.0f + 0.5f),
		(int)(c[2] * 255.0f + 0.5f), (int)(c[3] * 255.0f + 0.5f));
	return buf;
}

/// std::to_string(50.0f) → "50.000000" — six decimals leaking into RCSS
/// ("top: 50.000000px"). %g renders the shortest decimal form: "50", "12.5".
String FormatFloat(float v)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

/// "12px" / "12.5dp" / "12%" → (12, "px"); false for keywords like "auto".
bool ParseLength(const String& value, float& out_num, String& out_unit)
{
	size_t i = 0;
	if (i < value.size() && (value[i] == '-' || value[i] == '+')) ++i;
	while (i < value.size() && (std::isdigit((unsigned char)value[i]) || value[i] == '.'))
		++i;
	if (i == 0 || (i == 1 && (value[0] == '-' || value[0] == '+'))) return false;
	try { out_num = std::stof(value.substr(0, i)); }
	catch (...) { return false; }
	out_unit = value.substr(i);
	return true;
}

/// Splits a comma-separated keyword list into options.
Vector<String> SplitKeywords(const String& kws)
{
	Vector<String> out;
	String cur;
	for (char c : kws)
	{
		if (c == ',') { if (!cur.empty()) out.push_back(cur); cur.clear(); }
		else if (c != ' ') cur += c;
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}

} // namespace

// =========================================================================
// Draw
// =========================================================================

void UIPropertyPanel::Draw()
{
	// Docked into the editor DockSpace (right inspector column); the user can
	// float it out. No forced position — the dock/ini owns it.
	if (!OnBegin(ImGuiWindowFlags_NoCollapse))
		return;

	auto& state = UIDesignerState::Get();
	if (!state.IsLoaded())
	{
		ImGui::TextUnformatted("Designer not ready yet...");
		OnEnd();
		return;
	}

	const String sel = state.GetSelection();
	UIDocumentNode* node = sel.empty() ? nullptr : state.FindNodeById(sel);

	// ---- element tree (outline navigation) ----
	if (ImGui::CollapsingHeader("Element Tree", ImGuiTreeNodeFlags_DefaultOpen))
	{
		for (auto& top : state.Rml().nodes)
			DrawNodeTree(top.get());
	}

	ImGui::Separator();

	if (!node)
	{
		ImGui::TextWrapped("Select an element in the preview canvas (LMB) or the tree above.");
		OnEnd();
		return;
	}

	// ---- element header ----
	ImGui::Text("Element: <%s>  id: %s", node->tag_name.c_str(), sel.c_str());

	// ---- text content ----
	if (node->type == ENodeType::Element)
	{
		UIDocumentNode* first_text = nullptr;
		for (auto& c : node->children)
			if (c->type == ENodeType::Text) { first_text = c.get(); break; }
		char buf[256];
		String cur_text = first_text ? first_text->text : "";
		strncpy(buf, cur_text.c_str(), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputText("##text", buf, sizeof(buf)))
		{
			if (ImGui::IsItemDeactivatedAfterEdit())
				state.SetTextUndoable(sel, cur_text, buf);
		}
		ImGui::TextDisabled("Text (RML content)");
		ImGui::Spacing();
	}

	// ---- attributes ----
	if (!node->attributes.empty())
	{
		if (ImGui::CollapsingHeader("Attributes", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (const auto& attr : node->attributes)
			{
				char buf[128];
				strncpy(buf, attr.value.c_str(), sizeof(buf) - 1);
				buf[sizeof(buf) - 1] = '\0';
				ImGui::PushID(attr.name.c_str());
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(attr.name.c_str());
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-40);
				if (ImGui::InputText("##v", buf, sizeof(buf)))
				{
					if (ImGui::IsItemDeactivatedAfterEdit())
						state.SetAttributeUndoable(sel, attr.name, attr.value, buf);
				}
				ImGui::SameLine(0, 2);
				if (ImGui::SmallButton("x"))
					state.SetAttributeUndoable(sel, attr.name, attr.value, "");
				ImGui::PopID();
			}
		}
	}

	// ---- RCSS style (driven by the whitelist value families) ----
	UIRuleSet* rule = UIDesignerState::FindIdRule(state.Rcss(), sel);
	if (ImGui::CollapsingHeader("Style (RCSS)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (rule)
		{
			for (const auto& prop : rule->properties)
			{
				String new_value;
				if (EditPropertyRow(sel, prop.name, prop.value, new_value))
					state.SetPropertyUndoable(sel, prop.name, prop.value, new_value);
			}
		}
		DrawAddPropertyCombo(sel);
	}

	OnEnd();

	// Ctrl+Z/Y while this panel is hovered (the preview panel handles the canvas).
	if (ImGui::IsWindowHovered())
		state.HandleUndoRedoKeys();
}

void UIPropertyPanel::DrawNodeTree(UIDocumentNode* node)
{
	if (!node || node->type != ENodeType::Element) return;
	auto& state = UIDesignerState::Get();
	const String id = node->GetId();
	const bool selected = !id.empty() && id == state.GetSelection();

	ImGui::PushID(node);
	String label = "<" + node->tag_name + ">"
		+ (id.empty() ? "" : " #" + id);
	bool open = false;
	if (!node->children.empty())
		open = ImGui::TreeNodeEx(label.c_str(),
			ImGuiTreeNodeFlags_DefaultOpen | (selected ? ImGuiTreeNodeFlags_Selected : 0));
	else
	{
		ImGui::Selectable(label.c_str(), selected);
		open = false;
	}
	if (ImGui::IsItemClicked())
		state.Select(id);   // "" deselects
	if (open)
	{
		for (auto& child : node->children)
			DrawNodeTree(child.get());
		ImGui::TreePop();
	}
	ImGui::PopID();
}

// =========================================================================
// Family-driven property editor
// =========================================================================

bool UIPropertyPanel::EditPropertyRow(const String& id, const String& name,
	const String& cur_value, String& out_new_value)
{
	const RcssPropertySpec* spec = RcssWhitelist::Find(name.c_str());
	ImGui::PushID((id + "|" + name).c_str());
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(name.c_str());
	ImGui::SameLine();

	bool changed = false;

	if (spec)
	{
		switch (spec->family)
		{
		case EValueFamily::Color:
		{
			// Hex text editor. (ColorEdit4's picker wedges under pathological
			// mouse states — raw hex input is deterministic and the whitelist
			// emits canonical #rrggbb[aa] anyway.)
			char buf[64];
			strncpy(buf, cur_value.c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			ImGui::SetNextItemWidth(-40);
			if (ImGui::InputText("##v", buf, sizeof(buf)))
			{
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					float tmp[4];
					if (HexToRGBA(buf, tmp))   // only commit well-formed colors
					{
						out_new_value = buf;
						changed = true;
					}
				}
			}
			break;
		}
		case EValueFamily::Length:
		case EValueFamily::LengthPercent:
		case EValueFamily::NumberLengthPercent:
		{
			static const char* const kUnits[] = { "px", "dp", "em", "rem", "%" };
			float num = 0; String unit;
			if (!ParseLength(cur_value, num, unit))
				unit.clear();
			ImGui::SetNextItemWidth(64);
			ImGui::DragFloat("##v", &num, 0.5f, 0, 0, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				out_new_value = FormatFloat(num) + (unit.empty() ? "px" : unit);
				changed = true;
			}
			ImGui::SameLine(0, 2);
			ImGui::SetNextItemWidth(-40);
			int unit_idx = 0;
			for (int i = 0; i < 5; ++i)
				if (unit == kUnits[i]) { unit_idx = i; break; }
			if (ImGui::BeginCombo("##u", unit.empty() ? "px" : unit.c_str()))
			{
				for (int i = 0; i < 5; ++i)
					if (ImGui::Selectable(kUnits[i], i == unit_idx))
					{
						out_new_value = FormatFloat(num) + kUnits[i];
						changed = true;
					}
				ImGui::EndCombo();
			}
			break;
		}
		case EValueFamily::Number:
		{
			float num = 0;
			try { num = std::stof(cur_value); } catch (...) {}
			ImGui::SetNextItemWidth(-40);
			ImGui::DragFloat("##v", &num, 0.5f, 0, 0, "%.1f");
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				out_new_value = FormatFloat(num);
				changed = true;
			}
			break;
		}
		case EValueFamily::Keyword:
		{
			Vector<String> options = SplitKeywords(spec->keywords);
			String lower = cur_value;
			for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
			int cur = -1;
			for (size_t i = 0; i < options.size(); ++i)
				if (options[i] == lower) { cur = (int)i; break; }
			ImGui::SetNextItemWidth(-40);
			if (ImGui::BeginCombo("##v", cur >= 0 ? options[cur].c_str() : cur_value.c_str()))
			{
				for (size_t i = 0; i < options.size(); ++i)
					if (ImGui::Selectable(options[i].c_str(), (int)i == cur))
					{
						out_new_value = options[i];
						changed = true;
					}
				ImGui::EndCombo();
			}
			break;
		}
		case EValueFamily::String_:
		{
			char buf[256];
			String unquoted = cur_value;
			if (unquoted.size() >= 2 && unquoted.front() == '"' && unquoted.back() == '"')
				unquoted = unquoted.substr(1, unquoted.size() - 2);
			strncpy(buf, unquoted.c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			ImGui::SetNextItemWidth(-40);
			if (ImGui::InputText("##v", buf, sizeof(buf)))
			{
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					out_new_value = "\"" + String(buf) + "\"";
					changed = true;
				}
			}
			break;
		}
		default:
		{
			// Complex (transform/transition/...) / Shorthand / unknown — raw text.
			ImGui::SetNextItemWidth(-40);
			char buf[256];
			strncpy(buf, cur_value.c_str(), sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			if (ImGui::InputText("##v", buf, sizeof(buf)))
			{
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					out_new_value = buf;
					changed = true;
				}
			}
			break;
		}
		}
	}
	else
	{
		// Unknown property (RmlUi extension) — raw text.
		ImGui::SetNextItemWidth(-40);
		char buf[256];
		strncpy(buf, cur_value.c_str(), sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		if (ImGui::InputText("##v", buf, sizeof(buf)))
		{
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				out_new_value = buf;
				changed = true;
			}
		}
	}

	ImGui::SameLine(0, 2);
	if (ImGui::SmallButton("x"))
	{
		out_new_value = "";   // remove property
		changed = true;
	}
	ImGui::PopID();
	return changed;
}

void UIPropertyPanel::DrawAddPropertyCombo(const String& id)
{
	auto& state = UIDesignerState::Get();
	UIRuleSet* rule = UIDesignerState::FindIdRule(state.Rcss(), id);
	if (!rule) return;

	// Whitelisted properties not yet declared on this element, filtered to
	// families the panel can give a sensible default value for.
	static const char* kPreview = "Add property...";
	ImGui::SetNextItemWidth(-1);
	if (ImGui::BeginCombo("##addprop", kPreview))
	{
		for (const auto& spec : RcssWhitelist::All())
		{
			const bool declared = [&]()
			{
				for (const auto& p : rule->properties)
					if (p.name == spec.name) return true;
				return false;
			}();
			if (declared) continue;

			String default_value;
			switch (spec.family)
			{
			case EValueFamily::Color: default_value = "#000000"; break;
			case EValueFamily::Length: default_value = "0px"; break;
			case EValueFamily::LengthPercent: default_value = "0px"; break;
			case EValueFamily::Number: default_value = "0"; break;
			case EValueFamily::NumberLengthPercent: default_value = "0px"; break;
			case EValueFamily::Keyword:
				default_value = SplitKeywords(spec.keywords).empty()
					? "auto" : SplitKeywords(spec.keywords)[0];
				break;
			case EValueFamily::String_: default_value = "\"\""; break;
			default: continue;   // Complex / Shorthand — skip (edit existing instead)
			}

			if (ImGui::Selectable(spec.name))
				state.SetPropertyUndoable(id, spec.name, "", default_value);
		}
		ImGui::EndCombo();
	}
}

// =========================================================================
// Panel registration
// =========================================================================

namespace
{
UI::PanelRegister RegisterUIPropertyPanel([](const String& in_name, Bool in_show) -> UI::BasePanel*
{
	return new UIPropertyPanel(in_name, in_show);
}, UIPropertyPanel::GetTypeName());
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
