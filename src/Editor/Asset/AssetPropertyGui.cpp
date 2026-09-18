#include "AssetPropertyGui.h"
#include "AssetReference.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedDragDropTarget.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"

#include "imgui.h"

#include <algorithm>
#include <filesystem>

namespace AssetPropertyGui
{
bool drawAssetReferenceProperty(MkGuiStyleConstPtr style, const std::string& fieldName, const std::string& label,
								const AssetReferenceFactory& factory, const std::string& storedPath,
								std::string& outStoredPath)
{
	ImGui::TextUnformatted(label.c_str());

	// An asset name earns more of the row than the label column would leave it,
	// so the field starts right after the label text. These labels are much
	// shorter than the column offset and the reclaimed space is often a whole name.
	const float labelGap= 8.f * MkGui::getUiScale();
	ImGui::SameLine(0.f, labelGap);

	AssetReferencePtr defaultAssetRef= factory.getDefaultAssetReference();
	const std::string glyph= (defaultAssetRef != nullptr) ? defaultAssetRef->editorGetIcon() : "";
	const std::string shortName=
		!storedPath.empty() ? std::filesystem::path(storedPath).filename().string() : locText("componentPanel.noAsset");

	// The value is only ever set by a drop, so the item is inert: it is here to
	// read as a field and to carry the drop target. Floored so a narrow panel
	// still leaves a usable field.
	const float fieldWidth= std::max(ImGui::GetContentRegionAvail().x, (float)style->getValueWidth());
	const std::string itemLabel= glyph + " " + shortName + "##" + fieldName;

	const ImVec4 frameColor= ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
	ImGui::PushStyleColor(ImGuiCol_Button, frameColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, frameColor);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, frameColor);
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
	ImGui::Button(itemLabel.c_str(), ImVec2(fieldWidth, 0.f));
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("%s", !storedPath.empty() ? storedPath.c_str() : locText("componentPanel.dropAssetHint"));
	}

	bool bAssetDropped= false;

	// A tile dragged from the project Assets panel. The payload is inspected
	// before it is accepted, so a file of the wrong type never highlights the row.
	{
		MkGuiScopedDragDropTarget dropTarget;
		if (dropTarget)
		{
			const ImGuiPayload* pendingPayload= ImGui::GetDragDropPayload();
			if (pendingPayload != nullptr && pendingPayload->IsDataType(ProjectAssetCatalog::k_dragPayloadType)
				&& pendingPayload->Data != nullptr)
			{
				const auto& assetPayload= *static_cast<const ProjectAssetDragPayload*>(pendingPayload->Data);

				if (factory.matchesFilterPatterns(std::filesystem::path(assetPayload.storedPath)))
				{
					if (ImGui::AcceptDragDropPayload(ProjectAssetCatalog::k_dragPayloadType) != nullptr)
					{
						// The panel drags the stored form, which is the form kept here
						outStoredPath= assetPayload.storedPath;
						bAssetDropped= true;
					}
				}
			}
		}
	}

	// A tile dragged from a node editor window's Graph Assets panel, whose payload
	// is typed by asset class, so only a matching asset can be accepted at all
	if (!bAssetDropped && defaultAssetRef != nullptr)
	{
		auto assetRef= MkGui::receiveTypedDragDropPayload<AssetReference>(factory.getAssetRefClassName());
		if (assetRef)
		{
			outStoredPath= PathUtils::makeStoredProjectPath(assetRef->getInternalAssetPath());
			bAssetDropped= true;
		}
	}

	return bAssetDropped;
}
} // namespace AssetPropertyGui
