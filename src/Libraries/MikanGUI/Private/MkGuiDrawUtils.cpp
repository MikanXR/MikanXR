#include "MkGuiDrawUtils.h"
#include "MkGuiScopedFont.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiStyleManager.h"
#include "MkGuiScopedDragDropTarget.h"
#include "StringUtils.h"
#include "IMkTexture.h"

#include "IconsForkAwesome.h"

#include <algorithm>

namespace MkGui
{
float getUiScale()
{
	const ImGuiStyle& style= ImGui::GetStyle();

	return style.FontScaleMain * style.FontScaleDpi;
}

static std::string makeImGuiElementName(const std::string& name) { return StringUtils::stringify("##", name); }

// Moves the cursor to the value column and returns the width the value should
// fill: the rest of the panel, floored at the style's value width so a panel
// narrowed past the column keeps a usable field instead of collapsing it.
static float beginValueColumn(MkGuiStyleConstPtr style)
{
	ImGui::SameLine((float)style->getLabelWidth());
	return std::max((float)style->getValueWidth(), ImGui::GetContentRegionAvail().x);
}

bool drawPropertySheetHeader(MkGuiStyleConstPtr style, const std::string headerText)
{
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	MkGuiScopedStyle headerStyle(style);
	return ImGui::CollapsingHeader(headerText.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth);
}

void drawStaticTextProperty(MkGuiStyleConstPtr style, const std::string label, const std::string text)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	MkGuiScopedStyle textStyle(style);
	ImGui::TextUnformatted(text.c_str());
}

bool drawCheckBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
						  bool& inout_value)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::Checkbox(imguiElementName.c_str(), &inout_value);
}

bool drawIntProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label, int& inout_value)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::InputInt(imguiElementName.c_str(), &inout_value);
}

bool drawFloatProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
					   float& inout_value)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::InputFloat(imguiElementName.c_str(), &inout_value);
}

bool drawFloatSliderProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
							 float& inout_value, float srcMin, float srcMax, float displayMin, float displayMax,
							 const char* displayFormat)
{
	const float srcRange= srcMax - srcMin;
	float displayValue=
		(srcRange != 0.0f) ? displayMin + (inout_value - srcMin) / srcRange * (displayMax - displayMin) : displayMin;

	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	if (ImGui::SliderFloat(imguiElementName.c_str(), &displayValue, displayMin, displayMax, displayFormat))
	{
		const float displayRange= displayMax - displayMin;
		inout_value=
			(displayRange != 0.0f) ? srcMin + (displayValue - displayMin) / displayRange * (srcMax - srcMin) : srcMin;
		return true;
	}
	return false;
}

bool drawFloat2Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label, float* inout_v)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);

	// The value column is split 2 ways, so the theme's generous horizontal frame
	// padding would leave no room for the text and push it off center
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, ImGui::GetStyle().FramePadding.y));
	const bool bChanged= ImGui::InputFloat2(imguiElementName.c_str(), inout_v);
	ImGui::PopStyleVar();

	return bChanged;
}

bool drawFloat3Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label, float* inout_v)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);

	// The value column is split 3 ways, so the theme's generous horizontal frame
	// padding would leave no room for the text and push it off center
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, ImGui::GetStyle().FramePadding.y));
	const bool bChanged= ImGui::InputFloat3(imguiElementName.c_str(), inout_v);
	ImGui::PopStyleVar();

	return bChanged;
}

bool drawFloat4Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label, float* inout_v)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);

	// The value column is split 4 ways, so the theme's generous horizontal frame
	// padding would leave no room for the text and push it off center
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, ImGui::GetStyle().FramePadding.y));
	const bool bChanged= ImGui::InputFloat4(imguiElementName.c_str(), inout_v);
	ImGui::PopStyleVar();

	return bChanged;
}

bool drawStringProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label, char* buf,
						size_t bufSize)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::InputText(imguiElementName.c_str(), buf, bufSize, ImGuiInputTextFlags_EnterReturnsTrue);
}

void InlineRenameState::begin(int id, const std::string& currentName)
{
	activeId= id;
	bFocusPending= true;
	strncpy_s(buffer, sizeof(buffer), currentName.c_str(), _TRUNCATE);
}

void InlineRenameState::end()
{
	activeId= -1;
	bFocusPending= false;
}

static void trimInlineRenameBuffer(char* buffer, size_t bufSize)
{
	const std::string text(buffer);
	const size_t firstIndex= text.find_first_not_of(" \t");
	const size_t lastIndex= text.find_last_not_of(" \t");
	const std::string trimmed=
		(firstIndex == std::string::npos) ? std::string() : text.substr(firstIndex, lastIndex - firstIndex + 1);
	strncpy_s(buffer, bufSize, trimmed.c_str(), _TRUNCATE);
}

eInlineRenameResult drawInlineRenameField(InlineRenameState& state, const std::string& fieldId)
{
	if (state.bFocusPending)
	{
		ImGui::SetKeyboardFocusHere();
		state.bFocusPending= false;
	}

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	const bool bEntered= ImGui::InputText(fieldId.c_str(), state.buffer, sizeof(state.buffer),
										  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

	eInlineRenameResult result= eInlineRenameResult::editing;
	if (bEntered)
	{
		result= eInlineRenameResult::committed;
	}
	else if (ImGui::IsItemDeactivated())
	{
		// ImGui restores the original text on Escape, so a missed Escape still
		// commits the old name, which the caller treats as no change
		result= ImGui::IsKeyPressed(ImGuiKey_Escape, false) ? eInlineRenameResult::cancelled
															: eInlineRenameResult::committed;
	}

	if (result == eInlineRenameResult::committed)
	{
		trimInlineRenameBuffer(state.buffer, sizeof(state.buffer));
		if (state.buffer[0] == '\0')
			result= eInlineRenameResult::cancelled;
	}

	if (result != eInlineRenameResult::editing)
		state.end();

	return result;
}

bool isRenameClickOnSelectedItem(bool bWasSelected, int itemId, int& inout_pressedId)
{
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		// The second press of a double click never starts a rename
		inout_pressedId= (bWasSelected && !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ? itemId : -1;
		return false;
	}

	// A press anywhere else disarms this row
	if (inout_pressedId == itemId && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		inout_pressedId= -1;
		return false;
	}

	if (inout_pressedId == itemId && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		inout_pressedId= -1;

		// The drag delta reads zero unless the press travelled past the drag
		// threshold at some point, which is exactly the case that is a drag
		const ImVec2 dragDelta= ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
		const bool bDragged= dragDelta.x != 0.f || dragDelta.y != 0.f;
		return ImGui::IsItemHovered() && !bDragged;
	}

	return false;
}

bool drawSimpleComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
								const char* items, int& inout_selectedIdex)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	MkGuiScopedStyle comboStyle(style);
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::Combo(imguiElementName.c_str(), &inout_selectedIdex, items);
}

void drawImageProperty(MkGuiStyleConstPtr style, const std::string label, IMkTextureConstPtr image)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	uint32_t glTextureId= image ? image->getGlTextureId() : 0;
	const float previewSize= 100.f * getUiScale();
	ImGui::Image((ImTextureID)(intptr_t)glTextureId, ImVec2(previewSize, previewSize));
}

void drawImage(IMkTextureConstPtr image, float width, float height)
{
	uint32_t glTextureId= image ? image->getGlTextureId() : 0;
	const float uiScale= getUiScale();
	ImGui::Image((ImTextureID)(intptr_t)glTextureId, ImVec2(width * uiScale, height * uiScale));
}

bool drawImageButton(MkGuiStyleConstPtr style, const std::string& fieldName, const std::string& imageName)
{
	const MkGuiStyleTextureEntry* entry= style->findTexture(imageName);
	if (!entry)
		return false;
	uint32_t glTextureId= entry->texture ? entry->texture->getGlTextureId() : 0;
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	const float uiScale= getUiScale();
	return ImGui::ImageButton(imguiElementName.c_str(), (ImTextureID)(intptr_t)glTextureId,
							  ImVec2(entry->x * uiScale, entry->y * uiScale));
}

void sameLineIfFits(float itemWidth)
{
	const float lastItemRight= ImGui::GetItemRectMax().x;
	const float nextItemRight= lastItemRight + ImGui::GetStyle().ItemSpacing.x + itemWidth;
	const float visibleRight= ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
	if (nextItemRight < visibleRight)
	{
		ImGui::SameLine();
	}
}

bool drawGlyphButtonWithLabel(const std::string& fieldName, const std::string& glyph, const std::string& label,
							  float buttonSize, float glyphSize)
{
	// The caller's button size is authored in pixels and takes the scale. The glyph
	// size does not: it feeds a font push, which ImGui scales on its own.
	const float resolvedButtonSize= (buttonSize > 0.f) ? buttonSize * getUiScale() : ImGui::GetFrameHeight();
	const std::string buttonLabel= glyph + makeImGuiElementName(fieldName);

	bool bClicked= false;
	if (glyphSize > 0.f)
	{
		MkGuiScopedFont glyphFont(ImGui::GetFont(), glyphSize);
		bClicked= ImGui::Button(buttonLabel.c_str(), ImVec2(resolvedButtonSize, resolvedButtonSize));
	}
	else
	{
		bClicked= ImGui::Button(buttonLabel.c_str(), ImVec2(resolvedButtonSize, resolvedButtonSize));
	}

	// Drawn outside any pushed glyph font, so the label keeps the normal text
	// size, nudged down to sit centered against the button
	ImGui::SameLine();
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (resolvedButtonSize - ImGui::GetTextLineHeight()) * 0.5f);
	ImGui::TextUnformatted(label.c_str());

	return bClicked;
}

const char* ComboBoxDataSource::itemGetter(void* data, int idx)
{
	auto* dataSource= (ComboBoxDataSource*)data;

	if (idx >= 0 && idx < dataSource->getEntryCount())
	{
		return dataSource->getEntryDisplayString(idx).c_str();
	}

	return nullptr;
}

bool drawComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
						  ComboBoxDataSource* dataSource, int& inout_selectedIdex)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	MkGuiScopedStyle comboStyle(style);
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::Combo(imguiElementName.c_str(), &inout_selectedIdex, &ComboBoxDataSource::itemGetter, dataSource,
						dataSource->getEntryCount());
}

bool drawEnumComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
							  const std::vector<std::string>& entries, int& inout_selectedIndex)
{
	ImGui::TextUnformatted(label.c_str());
	ImGui::SetNextItemWidth(beginValueColumn(style));
	MkGuiScopedStyle comboStyle(style);
	const std::string imguiElementName= makeImGuiElementName(fieldName);
	return ImGui::Combo(
		imguiElementName.c_str(), &inout_selectedIndex,
		[](void* data, int n) -> const char*
		{
			auto* v= static_cast<const std::vector<std::string>*>(data);
			if (n >= 0 && n < (int)v->size())
			{
				return (*v)[n].c_str();
			}
			return nullptr;
		},
		(void*)&entries, (int)entries.size());
}

bool drawRadioButtonsProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
							  const std::vector<std::string>& entries, int& inout_selectedIndex)
{
	ImGui::TextUnformatted(label.c_str());
	const int prevIndex= inout_selectedIndex;
	for (int i= 0; i < (int)entries.size(); i++)
	{
		const std::string id= StringUtils::stringify(entries[i], "##", fieldName, "_", i);
		ImGui::RadioButton(id.c_str(), &inout_selectedIndex, i);
	}
	return inout_selectedIndex != prevIndex;
}

const std::string& getVariableIcon()
{
	static std::string icon= ICON_FK_SQUARE;
	return icon;
}

const std::string& getArrayIcon()
{
	static std::string icon= ICON_FK_TH;
	return icon;
}

ImVec4 getPinHoveredColor(float alpha) { return ImVec4(0.53f, 0.937f, 0.765f, alpha); }

ImVec4 getBooleanColor(float alpha) { return ImVec4(0.5f, 0.0f, 0.0f, alpha); }

ImVec4 getEnumColor(float alpha) { return ImVec4(0.f, 0.278f, 0.302f, alpha); }

ImVec4 getIntColor(float alpha) { return ImVec4(0.176f, 0.529f, 0.329f, alpha); }

ImVec4 getIntVectorColor(float alpha) { return ImVec4(0.557f, 0.886f, 0.722f, alpha); }

ImVec4 getFloatColor(float alpha) { return ImVec4(0.624f, 0.973f, 0.267f, alpha); }

ImVec4 getFloatVectorColor(float alpha) { return ImVec4(1.f, 0.78f, 0.173f, alpha); }

ImVec4 getMatrixColor(float alpha) { return ImVec4(0.965f, 0.396f, 0.024f, alpha); }

ImVec4 getPropertyColor(float alpha) { return ImVec4(0.f, 0.631f, 0.929f, alpha); }

ImVec4 getTextureColor(float alpha) { return ImVec4(0.6f, 0.263f, 0.969f, alpha); }

ImVec4 getComponentColor(float alpha) { return ImVec4(0.008f, 0.643f, 0.949f, alpha); }

bool receiveDragDropPayload(const std::string& payloadType, const std::function<void(const void*)>& onPayloadReceived)
{
	MkGuiScopedDragDropTarget ddt;
	if (ddt)
	{
		if (const ImGuiPayload* imguiPayload= ImGui::AcceptDragDropPayload(payloadType.c_str()))
		{
			if (imguiPayload->Data != nullptr && onPayloadReceived)
			{
				// Consume the payload here, inside the target scope: a payload is
				// only handed out on the delivery frame, and closing the target
				// on that frame clears the buffer this data lives in
				onPayloadReceived(imguiPayload->Data);
				return true;
			}
		}
	}

	return false;
}
}; // namespace MkGui