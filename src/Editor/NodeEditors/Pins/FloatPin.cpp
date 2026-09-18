#include "FloatPin.h"
#include "MkGuiDrawUtils.h"

// -- FloatPinBase -----
FloatPinBase::FloatPinBase()
	: NodePin()
{
	setHasDefaultValue(true);
}

float FloatPinBase::editorComputeInputWidth() const
{
	if (m_connectedLinks.size() == 0)
	{
		return ImGui::CalcTextSize(m_name.c_str()).x + (50.f + 11.f) * MkGui::getUiScale();
	}

	return NodePin::editorComputeInputWidth();
}

MkCanvas::PinIcon FloatPinBase::editorGetPinIcon() const { return MkCanvas::PinIcon::Circle; }

ImVec4 FloatPinBase::editorGetPinColor() const { return ImVec4(156.f / 255.f, 253.f / 255.f, 65.f / 255.f, 1.f); }

void FloatPinBase::editorRenderContextMenu(const NodeEditorState& editorState) {}

// -- FloatPin -----
void FloatPin::editorRenderInputTextEntry(const NodeEditorState& editorState)
{
	if (m_connectedLinks.size() == 0)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(50.0f);
		ImGui::InputFloat("", &value);
	}
}

void FloatPin::copyValueFromSourcePin()
{
	FloatPinPtr sourcePin= std::dynamic_pointer_cast<FloatPin>(getConnectedSourcePin());

	if (sourcePin)
	{
		setValue(sourcePin->getValue());
	}
}

ImU32 FloatPin::editorGetLinkStyleColor() const { return IM_COL32(156, 253, 65, 255); }

// -- Float2Pin -----
void Float2Pin::copyValueFromSourcePin()
{
	Float2PinPtr sourcePin= std::dynamic_pointer_cast<Float2Pin>(getConnectedSourcePin());

	if (sourcePin)
	{
		setValue(sourcePin->getValue());
	}
}

void Float2Pin::editorRenderInputTextEntry(const NodeEditorState& editorState)
{
	if (m_connectedLinks.size() == 0)
	{
		ImGui::Dummy(ImVec2(11.0f, 1.0f));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		ImGui::InputFloat2("", value.data());
	}
}

// -- Float3Pin -----
void Float3Pin::copyValueFromSourcePin()
{
	Float3PinPtr sourcePin= std::dynamic_pointer_cast<Float3Pin>(getConnectedSourcePin());

	if (sourcePin)
	{
		setValue(sourcePin->getValue());
	}
}

void Float3Pin::editorRenderInputTextEntry(const NodeEditorState& editorState)
{
	if (m_connectedLinks.size() == 0)
	{
		ImGui::Dummy(ImVec2(11.0f, 1.0f));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::InputFloat3("", value.data());
	}
}

// -- Float4Pin -----
void Float4Pin::copyValueFromSourcePin()
{
	Float4PinPtr sourcePin= std::dynamic_pointer_cast<Float4Pin>(getConnectedSourcePin());

	if (sourcePin)
	{
		setValue(sourcePin->getValue());
	}
}

void Float4Pin::editorRenderInputTextEntry(const NodeEditorState& editorState)
{
	if (m_connectedLinks.size() == 0)
	{
		ImGui::Dummy(ImVec2(11.0f, 1.0f));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputFloat4("", value.data());
	}
}
