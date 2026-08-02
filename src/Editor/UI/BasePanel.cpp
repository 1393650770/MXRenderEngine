#include "BasePanel.h"
#include "imgui.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

Map<String, MXRender::UI::PanelCreateFunction> BasePanel::panel_createfunc_map;

BasePanel::BasePanel(CONST String& in_name, Bool in_show /*= true*/) :name(in_name), is_show(in_show)
{
}

Bool BasePanel::OnBegin(Int window_flags)
{
	if (!is_show)
		return false;

	ImGui::SetNextWindowSize(ImVec2(480, 640), ImGuiCond_FirstUseEver);

	ImGui::Begin(name.c_str(), &is_show, window_flags | ImGuiWindowFlags_NoCollapse);
	//ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4.0f,2.0f));
	// --  FPS 移至编辑器窗口级别显示，每个面板不再单独显示
	return true;
}

void BasePanel::OnEnd() CONST
{
	//ImGui::PopStyleVar();
	ImGui::End();
}

CONST String& BasePanel::GetName() CONST
{
	return name;
}

BasePanel* BasePanel::CreatePanel(CONST String& in_type_name, CONST String& in_name, Bool in_show /*= true*/)
{
	return panel_createfunc_map[in_type_name](in_name, in_show);
}

void BasePanel::ShowLabel(CONST Char* label, Vector<Int> color)
{
	ImColor col(color[0], color[1], color[2], color[3]);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
	auto size = ImGui::CalcTextSize(label);

	auto padding = ImGui::GetStyle().FramePadding;
	auto spacing = ImGui::GetStyle().ItemSpacing;

	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x+spacing.x, ImGui::GetCursorPos().y  -spacing.y));

	auto rectMin = ImVec2(ImGui::GetCursorScreenPos().x - padding.x, ImGui::GetCursorScreenPos().y - padding.y);
	auto rectMax = ImVec2(ImGui::GetCursorScreenPos().x + size.x + padding.x, ImGui::GetCursorScreenPos().y + size.y + padding.y);

	auto drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(rectMin, rectMax, col, size.y * 0.15f);
	ImGui::TextUnformatted(label);

}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE