#include "UIPalette.h"
#include "UI/UIDesigner/UIDesignerState.h"

#include <imgui.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

using namespace MXRender::UI::UIDocModel;

const char* const UIPalettePanel::kDragPayload = "UI_WIDGET_TEMPLATE";

UIPalettePanel::UIPalettePanel(const String& in_name, Bool in_show)
	: UI::BasePanel(in_name, in_show) {}

void UIPalettePanel::Init() {}
void UIPalettePanel::Update() {}
void UIPalettePanel::Release() {}

// =========================================================================
// Template library — drawn from the DemoPanel / NoitaLikeHUD sample
// structures (2026-08). Insertion force-adds position/left/top; the rest of
// the starter rule is editable in the property panel.
// =========================================================================

const Vector<UIWidgetTemplate>& UIPalettePanel::Templates()
{
	static const Vector<UIWidgetTemplate> kTemplates = {
		{ "Container", "Layout", "div", {}, "",
			{ { "display", "flex" }, { "flex-direction", "column" },
			  { "gap", "8dp" }, { "background-color", "#1e2a38" }, { "padding", "8dp" } },
			{} },
		{ "Text", "Basic", "text", {}, "Text", {}, {} },
		{ "Button", "Basic", "button", {}, "Button",
			{ { "background-color", "#2a6df4" }, { "color", "#ffffff" },
			  { "padding", "4dp 12dp" }, { "border-radius", "4dp" } },
			{} },
		{ "Slider", "Control", "input",
			{ { "type", "range" }, { "min", "0" }, { "max", "100" }, { "value", "50" } }, "",
			{ { "width", "160dp" }, { "height", "8dp" } },
			{} },
		{ "Progress Bar", "Progress", "div", {}, "",
			{ { "width", "160dp" }, { "height", "16dp" },
			  { "background-color", "#333333" }, { "border-radius", "4dp" } },
			{ { "", "", "div", {}, "Fill",
				{ { "position", "absolute" }, { "left", "0dp" }, { "top", "0dp" },
				  { "height", "100%" }, { "width", "50%" },
				  { "background-color", "#4caf50" } },
				{} } } },
	};
	return kTemplates;
}

const UIWidgetTemplate* UIPalettePanel::FindTemplate(const String& name)
{
	for (const auto& tpl : Templates())
		if (tpl.name == name)
			return &tpl;
	return nullptr;
}

// =========================================================================
// Panel
// =========================================================================

void UIPalettePanel::Draw()
{
	// Docked into the editor DockSpace (bottom-right tools area); the user can
	// float it out. No forced position — the dock/ini owns it.
	if (!OnBegin(ImGuiWindowFlags_NoCollapse))
		return;

	ImGui::TextUnformatted("Drag into the preview canvas, or double-click to insert at center.");
	ImGui::Separator();

	static const char* const kCategories[] = { "Layout", "Basic", "Control", "Progress" };
	for (const char* cat : kCategories)
	{
		if (!ImGui::CollapsingHeader(cat, ImGuiTreeNodeFlags_DefaultOpen))
			continue;
		for (const auto& tpl : Templates())
		{
			if (tpl.category != cat) continue;
			if (ImGui::Selectable(tpl.name.c_str()))
			{
				if (ImGui::IsMouseDoubleClicked(0))
					InsertTemplate(tpl, 320.0f, 240.0f);
			}
			if (ImGui::BeginDragDropSource())
			{
				ImGui::SetDragDropPayload(kDragPayload, tpl.name.c_str(),
					tpl.name.size() + 1, ImGuiCond_Once);
				ImGui::TextUnformatted(tpl.name.c_str());
				ImGui::EndDragDropSource();
			}
		}
	}

	OnEnd();
}

void UIPalettePanel::InsertTemplate(const UIWidgetTemplate& tpl, Float32 x, Float32 y)
{
	if (!UIDesignerState::Get().IsLoaded()) return;
	std::cout << "[UIDesigner] palette insert: " << tpl.name << " at " << (int)x << "," << (int)y << std::endl;
	UIDesignerState::Get().InsertNodeUndoable(tpl, "", x, y);
}

// =========================================================================
// Panel registration
// =========================================================================

namespace
{
UI::PanelRegister RegisterUIPalettePanel([](const String& in_name, Bool in_show) -> UI::BasePanel*
{
	return new UIPalettePanel(in_name, in_show);
}, UIPalettePanel::GetTypeName());
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
