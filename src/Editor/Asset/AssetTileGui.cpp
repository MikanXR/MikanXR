#include "AssetTileGui.h"
#include "AssetReference.h"
#include "IMkTexture.h"
#include "MkGuiDrawUtils.h"

#include "imgui.h"

#include <algorithm>

namespace AssetTileGui
{
// The tile's preview square, inset from the frame
static constexpr float k_previewSize= 100.f;
static constexpr float k_previewInset= 10.f;
// Row spacing between tiles
static constexpr float k_tileGap= 10.f;
// The frame every tile draws, whatever the window's own frame style
static constexpr float k_tileBorderSize= 1.f;
static constexpr float k_tileRounding= 3.f;

// Width of one list column, so a name of ordinary length shows whole
static constexpr float k_listColumnWidth= 200.f;

std::string truncateTextWithEllipsis(const std::string& text, float maxWidth)
{
	if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
	{
		return text;
	}

	std::string truncated= text;
	while (!truncated.empty() && ImGui::CalcTextSize((truncated + "...").c_str()).x > maxWidth)
	{
		// Pop one codepoint (skip UTF-8 continuation bytes)
		truncated.pop_back();
		while (!truncated.empty() && ((unsigned char)truncated.back() & 0xC0) == 0x80)
		{
			truncated.pop_back();
		}
	}

	return truncated + "...";
}

float getTileHeight()
{
	const float uiScale= MkGui::getUiScale();

	// Preview, a gap, one text row, and the inset above and below
	return k_previewInset * uiScale + k_previewSize * uiScale + ImGui::GetStyle().ItemSpacing.y
		   + ImGui::GetTextLineHeight() + k_previewInset * uiScale;
}

ItemResult beginTile(const std::string& idStr, bool bSelected)
{
	ItemResult result;
	const float uiScale= MkGui::getUiScale();
	const float tileHeight= getTileHeight();

	ImGui::Dummy(ImVec2(k_tileGap * uiScale, tileHeight));
	ImGui::SameLine();
	ImGui::BeginGroup();

	// The frame is the same in every window, so the tile reads the same in the
	// project panel and the editors. A selected tile keeps the active tint at rest.
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, k_tileBorderSize * uiScale);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, k_tileRounding * uiScale);
	if (bSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
	}
	result.bClicked= ImGui::Button(idStr.c_str(), ImVec2(k_tileWidth * uiScale, tileHeight));
	if (bSelected)
	{
		ImGui::PopStyleColor();
	}
	ImGui::PopStyleVar(2);

	// Read the hover state before a drag source claims the item
	result.bHovered= ImGui::IsItemHovered();
	result.bDoubleClicked= result.bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	return result;
}

void endTile(AssetReferencePtr assetRef, const std::string& displayName)
{
	IMkTexturePtr texture= assetRef ? assetRef->getPreviewTexture() : IMkTexturePtr();
	const float uiScale= MkGui::getUiScale();
	const float tileHeight= getTileHeight();
	const float previewSize= k_previewSize * uiScale;
	const float previewInset= k_previewInset * uiScale;

	// Back up into the frame: the preview sits inset from its top left corner
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + previewInset);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (tileHeight - previewInset) - ImGui::GetStyle().ItemSpacing.y);

	if (texture && texture->getGlTextureId() != 0)
	{
		ImGui::Image((ImTextureID)(intptr_t)texture->getGlTextureId(), ImVec2(previewSize, previewSize));
	}
	else
	{
		// Type icon stand-in for assets with no preview image
		const ImVec2 tileMin= ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(previewSize, previewSize));

		const char* icon= assetRef ? assetRef->editorGetIcon() : "";
		ImFont* font= ImGui::GetFont();
		const float iconFontSize= ImGui::GetFontSize() * 3.f;
		const ImVec2 iconSize= font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.f, icon);
		const ImVec2 iconPos(tileMin.x + (previewSize - iconSize.x) * 0.5f,
							 tileMin.y + (previewSize - iconSize.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon);
	}

	// The name row, clipped to the preview width so it stays inside the frame
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + previewInset);
	const std::string clippedName= truncateTextWithEllipsis(displayName, previewSize);
	ImGui::TextUnformatted(clippedName.c_str());
	if (ImGui::IsItemHovered() && clippedName != displayName)
	{
		ImGui::SetTooltip("%s", displayName.c_str());
	}

	ImGui::EndGroup();

	// Tiles flow left to right and wrap when the next one would not fit
	ImGui::SameLine();
	if (ImGui::GetContentRegionAvail().x < k_tileWidth * uiScale + k_tileGap * uiScale)
	{
		ImGui::NewLine();
		ImGui::NewLine();
	}
}

bool beginList(const char* id)
{
	const float columnWidth= k_listColumnWidth * MkGui::getUiScale();
	const int columnCount= std::max(1, (int)(ImGui::GetContentRegionAvail().x / columnWidth));

	return ImGui::BeginTable(id, columnCount, ImGuiTableFlags_SizingStretchSame);
}

ItemResult drawListItem(const std::string& idStr, const char* glyph, const std::string& displayName, bool bSelected)
{
	ItemResult result;

	ImGui::TableNextColumn();

	// The glyph takes the lead of the row; the name is clipped to what is left
	const std::string glyphText= std::string(glyph != nullptr ? glyph : "") + " ";
	const float nameWidth= ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(glyphText.c_str()).x
						   - ImGui::GetStyle().FramePadding.x * 2.f;
	const std::string clippedName= truncateTextWithEllipsis(displayName, std::max(nameWidth, 0.f));
	const std::string label= glyphText + clippedName + idStr;

	result.bClicked= ImGui::Selectable(label.c_str(), bSelected, ImGuiSelectableFlags_AllowDoubleClick);
	result.bHovered= ImGui::IsItemHovered();
	result.bDoubleClicked= result.bHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

	if (result.bHovered && clippedName != displayName)
	{
		ImGui::SetTooltip("%s", displayName.c_str());
	}

	return result;
}

void endList() { ImGui::EndTable(); }
} // namespace AssetTileGui
