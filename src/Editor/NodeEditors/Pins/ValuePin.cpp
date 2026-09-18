#include "ValuePin.h"
#include "MkGuiDrawUtils.h"

// -- IntPin -----
float ValuePin::editorComputeInputWidth() const
{
	if (m_connectedLinks.size() == 0)
	{
		return ImGui::CalcTextSize(m_name.c_str()).x + (50.f + 11.f) * MkGui::getUiScale();
	}

	return NodePin::editorComputeInputWidth();
}

MkCanvas::PinIcon ValuePin::editorGetPinIcon() const { return MkCanvas::PinIcon::Circle; }

ImVec4 ValuePin::editorGetPinColor() const { return ImGui::ColorConvertU32ToFloat4(editorValuePinColor(1.f)); }

const ImU32 ValuePin::editorValuePinColor(float alpha) const
{
	return ImGui::GetColorU32(MkGui::getPropertyColor(alpha));
}