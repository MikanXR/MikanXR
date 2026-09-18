#include "GuiPanel_Assets.h"
#include "App.h"
#include "AppStage.h"
#include "AssetReference.h"
#include "AssetTileGui.h"
#include "IconsForkAwesome.h"
#include "IEditorWindow.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedChild.h"
#include "MkGuiScopedDragDropSource.h"
#include "MkGuiScopedPopup.h"
#include "MkGuiScopedStyle.h"
#include "MkGuiStyleManager.h"
#include "ModalConfirm/ModalDialog_Confirm.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ProjectAssetCatalog.h"
#include "Project/ProjectGuiPanelContext.h"
#include "StringUtils.h"
#include "Windows/MaterialNodeEditorWindow.h"

#include "imgui.h"
#include "tinyfiledialogs.h"

#include <algorithm>
#include <sstream>

// Width of the folder list beside the tile grid
static constexpr float k_folderListWidth= 200.f;
// How many referrers the refused-delete message names before it summarizes the rest
static constexpr int k_maxReferrerLines= 12;

// The message shown when an asset is still in use: the refusal, then one line
// per referrer that named it
static std::string buildDeleteRefusedMessage(const std::string& displayName,
											 const std::vector<ProjectAssetReferrer>& referrers)
{
	std::string message= locFormat("assets.deleteRefusedFmt", displayName.c_str());

	const int referrerCount= (int)referrers.size();
	const int lineCount= std::min(referrerCount, k_maxReferrerLines);
	for (int lineIndex= 0; lineIndex < lineCount; ++lineIndex)
	{
		const ProjectAssetReferrer& referrer= referrers[lineIndex];

		message+= "\n";
		switch (referrer.kind)
		{
		case ProjectAssetReferrer::Kind::graph:
			message+= locFormat("assets.referrerGraphFmt", referrer.name.c_str());
			break;
		case ProjectAssetReferrer::Kind::material:
			message+= locFormat("assets.referrerMaterialFmt", referrer.name.c_str());
			break;
		case ProjectAssetReferrer::Kind::component:
			message+= locFormat("assets.referrerComponentFmt", referrer.name.c_str(), referrer.detail.c_str());
			break;
		}
	}

	if (referrerCount > lineCount)
	{
		message+= "\n" + locFormat("assets.referrerMoreFmt", referrerCount - lineCount);
	}

	return message;
}

GuiPanel_Assets::~GuiPanel_Assets()
{
	if (ProjectAssetCatalog* catalog= getCatalog())
	{
		catalog->OnCatalogChanged-= MakeDelegate(this, &GuiPanel_Assets::onCatalogChanged);
	}
}

bool GuiPanel_Assets::init(ProjectGuiPanelContext* context)
{
	m_context= context;
	m_contextMenuStyle= getGuiStyleManager()->getStyle("node_editor_context_menu");

	const std::vector<ProjectAssetFolderDesc>& folderDescs= ProjectAssetCatalog::getFolderDescs();
	if (!folderDescs.empty())
	{
		m_currentFolderId= folderDescs[0].id;
	}

	if (ProjectAssetCatalog* catalog= getCatalog())
	{
		catalog->OnCatalogChanged+= MakeDelegate(this, &GuiPanel_Assets::onCatalogChanged);
	}

	return true;
}

ProjectAssetCatalog* GuiPanel_Assets::getCatalog() const
{
	IEditorWindow* ownerWindow= m_ownerAppStage != nullptr ? m_ownerAppStage->getOwnerWindow() : nullptr;

	return ownerWindow != nullptr ? ownerWindow->getAssetCatalog() : nullptr;
}

void GuiPanel_Assets::onCatalogChanged()
{
	// A rescan can drop the selected entry, leaving the key pointing at nothing
	if (!m_selectedKey.empty() && getSelectedEntry() == nullptr)
	{
		m_selectedKey.clear();
		m_bScrollToSelection= false;
	}
}

const ProjectAssetEntry* GuiPanel_Assets::getSelectedEntry() const
{
	ProjectAssetCatalog* catalog= getCatalog();
	if (catalog == nullptr || m_selectedKey.empty())
		return nullptr;

	const size_t separatorIndex= m_selectedKey.find('|');
	if (separatorIndex == std::string::npos)
		return nullptr;

	return catalog->findEntry(m_selectedKey.substr(0, separatorIndex), m_selectedKey.substr(separatorIndex + 1));
}

bool GuiPanel_Assets::selectEntry(const std::string& folderId, const std::string& storedPath)
{
	ProjectAssetCatalog* catalog= getCatalog();
	if (catalog == nullptr)
		return false;

	const ProjectAssetEntry* entry= catalog->findEntry(folderId, storedPath);
	if (entry == nullptr)
		return false;

	m_currentFolderId= folderId;
	m_selectedKey= entry->key();
	m_bScrollToSelection= true;

	return true;
}

void GuiPanel_Assets::setCurrentFolder(const std::string& folderId)
{
	if (m_currentFolderId == folderId)
		return;

	m_currentFolderId= folderId;
	m_selectedKey.clear();
	m_bScrollToSelection= false;
}

void GuiPanel_Assets::onGui()
{
	if (getCatalog() == nullptr)
		return;

	const std::vector<ProjectAssetFolderDesc>& folderDescs= ProjectAssetCatalog::getFolderDescs();
	if (folderDescs.empty())
		return;

	// The folder descs are static, so the current desc stays valid for the frame
	// even when the folder list below changes which folder is current
	const ProjectAssetFolderDesc* currentDesc= ProjectAssetCatalog::findFolderDesc(m_currentFolderId);
	if (currentDesc == nullptr)
	{
		m_currentFolderId= folderDescs[0].id;
		currentDesc= &folderDescs[0];
	}

	renderFolderList();

	ImGui::SameLine();

	{
		MkGuiScopedChild folderContents("AssetFolderContents");

		renderToolbar(*currentDesc);
		ImGui::Separator();
		renderTileGrid(*currentDesc);
	}
}

void GuiPanel_Assets::renderFolderList()
{
	MkGuiScopedChild folderList("AssetFolderList", ImVec2(k_folderListWidth * MkGui::getUiScale(), 0),
								ImGuiChildFlags_Borders);

	for (const ProjectAssetFolderDesc& desc : ProjectAssetCatalog::getFolderDescs())
	{
		const std::string label= std::string(ICON_FK_FOLDER) + " " + locText(desc.locKey) + "##" + desc.id;

		if (ImGui::Selectable(label.c_str(), desc.id == m_currentFolderId))
		{
			setCurrentFolder(desc.id);
		}
	}
}

void GuiPanel_Assets::renderToolbar(const ProjectAssetFolderDesc& desc)
{
	if (desc.bReadOnly)
	{
		ImGui::TextDisabled("%s", locText("assets.readOnlyFolder"));
	}
	else
	{
		// The buttons flow: one that no longer fits the panel width wraps to the next row
		const auto smallButtonWidth= [](const std::string& label)
		{ return ImGui::CalcTextSize(label.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.f; };

		bool bFirstButton= true;
		for (const AssetReferenceFactoryPtr& factory : desc.factories)
		{
			const std::string buttonLabel= StringUtils::stringify(
				ICON_FK_PLUS_CIRCLE "  ", locFormat("assets.addAssetFmt", factory->getAssetTypeName().c_str()), "##add",
				factory->getAssetRefClassName());
			if (!bFirstButton)
			{
				MkGui::sameLineIfFits(smallButtonWidth(buttonLabel));
			}
			bFirstButton= false;

			if (ImGui::SmallButton(buttonLabel.c_str()))
			{
				importFromFileDialog(desc, factory);
			}

			// A material folder also authors: the material editor opens on a fresh
			// graph of the folder's domain, and its save rescans the catalog
			if (desc.bMaterialFolder)
			{
				const std::string newLabel= StringUtils::stringify(
					ICON_FK_PLUS_CIRCLE "  ", locFormat("assets.newAssetFmt", factory->getAssetTypeName().c_str()),
					"##new", factory->getAssetRefClassName());
				MkGui::sameLineIfFits(smallButtonWidth(newLabel));
				if (ImGui::SmallButton(newLabel.c_str()))
				{
					// Deferred: opening pushes an editor window while the toolbar is drawing
					const eMaterialDomain domain= (eMaterialDomain)desc.materialDomain;
					addDeferredGuiEvent([domain]()
										{ MaterialNodeEditorWindow::openOnNewMaterial(App::getInstance(), domain); });
				}
			}
		}
	}

	const std::string refreshLabel=
		StringUtils::stringify(ICON_FK_REFRESH "  ", locText("assets.refresh"), "##refreshAssets");
	MkGui::sameLineIfFits(ImGui::CalcTextSize(refreshLabel.c_str(), nullptr, true).x
						  + ImGui::GetStyle().FramePadding.x * 2.f);
	if (ImGui::SmallButton(refreshLabel.c_str()))
	{
		// Deferred: a rescan replaces the entries the grid is drawing from
		addDeferredGuiEvent(
			[this]()
			{
				if (ProjectAssetCatalog* catalog= getCatalog())
					catalog->refresh();
			});
	}
}

void GuiPanel_Assets::renderTileGrid(const ProjectAssetFolderDesc& desc)
{
	ProjectAssetCatalog* catalog= getCatalog();
	if (catalog == nullptr)
		return;

	const std::vector<ProjectAssetEntry>& entries= catalog->getEntries(desc.id);
	if (entries.empty())
	{
		ImGui::TextDisabled("%s", locText("assets.empty"));
		return;
	}

	MkGuiScopedChild tileGrid("AssetTileGrid");

	if (desc.bPreviewTiles)
	{
		ImGui::Dummy(ImVec2(1, 10 * MkGui::getUiScale()));
		for (const ProjectAssetEntry& entry : entries)
		{
			const std::string entryKey= entry.key();
			const bool bSelected= entryKey == m_selectedKey;

			const AssetTileGui::ItemResult tile= AssetTileGui::beginTile("##asset" + entryKey, bSelected);
			handleEntryInteraction(desc, entry, tile.bClicked, tile.bDoubleClicked);
			AssetTileGui::endTile(catalog->getAssetReference(entry), entry.displayName);
		}
	}
	else if (AssetTileGui::beginList("AssetList"))
	{
		for (const ProjectAssetEntry& entry : entries)
		{
			const std::string entryKey= entry.key();
			const bool bSelected= entryKey == m_selectedKey;

			AssetReferencePtr assetRef= catalog->getAssetReference(entry);
			const char* glyph= assetRef ? assetRef->editorGetIcon() : "";
			const AssetTileGui::ItemResult row=
				AssetTileGui::drawListItem("##asset" + entryKey, glyph, entry.displayName, bSelected);
			handleEntryInteraction(desc, entry, row.bClicked, row.bDoubleClicked);
		}
		AssetTileGui::endList();
	}
}

void GuiPanel_Assets::handleEntryInteraction(const ProjectAssetFolderDesc& desc, const ProjectAssetEntry& entry,
											 bool bClicked, bool bDoubleClicked)
{
	ProjectAssetCatalog* catalog= getCatalog();
	const std::string entryKey= entry.key();
	const bool bSelected= entryKey == m_selectedKey;

	if (bSelected && m_bScrollToSelection)
	{
		ImGui::SetScrollHereY(0.5f);
		m_bScrollToSelection= false;
	}

	{
		MkGuiScopedDragDropSource dds(ImGuiDragDropFlags_None);
		if (dds)
		{
			ProjectAssetDragPayload payload(entry);

			ImGui::SetDragDropPayload(ProjectAssetCatalog::k_dragPayloadType, &payload,
									  sizeof(ProjectAssetDragPayload));
			ImGui::TextUnformatted(entry.displayName.c_str());
		}
	}

	{
		MkGuiScopedStyle contextStyle(m_contextMenuStyle);
		MkGuiScopedPopupContextItem contextMenu;
		if (contextMenu)
		{
			m_selectedKey= entryKey;

			// A bundled asset becomes editable by copying it into the project,
			// where the copy shadows it by stored path
			if (entry.bBundled && !desc.bReadOnly)
			{
				if (ImGui::MenuItem(locLabel("assets.copyToProject"), ICON_FK_FILES_O))
				{
					requestCopyToProject(entry);
				}
			}

			if (ImGui::MenuItem(locLabel("nodeEditor.delete"), ICON_FK_TRASH, false, !entry.bReadOnly))
			{
				requestDeleteEntry(entry);
			}

			if (entry.bReadOnly)
			{
				ImGui::TextDisabled("%s", locText("assets.bundledAsset"));
			}
		}
	}

	if (bClicked)
	{
		m_selectedKey= entryKey;
	}

	AssetReferencePtr assetRef= catalog ? catalog->getAssetReference(entry) : AssetReferencePtr();
	if (bDoubleClicked && assetRef && assetRef->editorCanOpen())
	{
		// Deferred: opening pushes an editor window while the grid is drawing
		addDeferredGuiEvent([assetRef]() { assetRef->editorOpen(); });
	}
}

void GuiPanel_Assets::importFromFileDialog(const ProjectAssetFolderDesc& desc, AssetReferenceFactoryPtr factory)
{
	ProjectAssetCatalog* catalog= getCatalog();
	if (catalog == nullptr || !factory)
		return;

	// The dialog opens in the folder the import lands in. tinyfd reads the last
	// element of the path as a file name, so a directory needs a trailing separator.
	const std::filesystem::path folderDirectory= ProjectAssetCatalog::getFolderDirectory(desc.id);
	const std::string defaultDir= !folderDirectory.empty() ? (folderDirectory / "").string() : std::string();
	const char* defaultPath= !defaultDir.empty() ? defaultDir.c_str() : factory->getDefaultPath();

	const char* pickedPaths=
		tinyfd_openFileDialog(factory->getFileDialogTitle(), defaultPath, factory->getFilterPatternCount(),
							  factory->getFilterPatterns(), factory->getFilterDescription(), 1);
	if (pickedPaths == nullptr || pickedPaths[0] == '\0')
		return;

	// A multi-select hands back the paths joined by '|'
	std::stringstream pathStream(pickedPaths);
	std::string sourcePath;
	std::string lastStoredPath;
	while (std::getline(pathStream, sourcePath, '|'))
	{
		if (sourcePath.empty())
			continue;

		std::string storedPath;
		std::string error;
		if (!catalog->importAsset(desc.id, sourcePath, storedPath, error))
		{
			ModalDialog_MessageBox::showMessageBox(m_ownerAppStage, locFormat("assets.importFailedFmt", error.c_str()));
			return;
		}

		lastStoredPath= storedPath;
	}

	if (!lastStoredPath.empty())
	{
		selectEntry(desc.id, lastStoredPath);
	}
}

void GuiPanel_Assets::requestCopyToProject(const ProjectAssetEntry& entry)
{
	const std::string folderId= entry.folderId;
	const std::filesystem::path sourcePath= entry.absolutePath;

	addDeferredGuiEvent(
		[this, folderId, sourcePath]()
		{
			ProjectAssetCatalog* catalog= getCatalog();
			if (catalog == nullptr)
				return;

			std::string storedPath;
			std::string error;
			if (!catalog->importAsset(folderId, sourcePath, storedPath, error))
			{
				ModalDialog_MessageBox::showMessageBox(m_ownerAppStage,
													   locFormat("assets.importFailedFmt", error.c_str()));
				return;
			}

			selectEntry(folderId, storedPath);
		});
}

void GuiPanel_Assets::requestDeleteEntry(const ProjectAssetEntry& entry)
{
	// The entry lives in the catalog's scan, which every refresh replaces, so the
	// deferred work carries the ids and finds the entry again
	const std::string folderId= entry.folderId;
	const std::string storedPath= entry.storedPath;

	addDeferredGuiEvent(
		[this, folderId, storedPath]()
		{
			ProjectAssetCatalog* catalog= getCatalog();
			if (catalog == nullptr)
				return;

			const ProjectAssetEntry* targetEntry= catalog->findEntry(folderId, storedPath);
			if (targetEntry == nullptr)
				return;

			const std::string displayName= targetEntry->displayName;
			const std::vector<ProjectAssetReferrer> referrers= catalog->findReferences(storedPath);
			if (!referrers.empty())
			{
				ModalDialog_MessageBox::showMessageBox(m_ownerAppStage,
													   buildDeleteRefusedMessage(displayName, referrers));
				return;
			}

			ModalDialog_Confirm::confirmQuestion(
				m_ownerAppStage, locText("assets.deleteConfirmTitle"),
				locFormat("assets.deleteConfirmFmt", displayName.c_str()),
				[this, folderId, storedPath]()
				{
					addDeferredGuiEvent(
						[this, folderId, storedPath]()
						{
							ProjectAssetCatalog* deleteCatalog= getCatalog();
							if (deleteCatalog == nullptr)
								return;

							const ProjectAssetEntry* deleteTarget= deleteCatalog->findEntry(folderId, storedPath);
							if (deleteTarget == nullptr)
								return;

							std::string error;
							if (!deleteCatalog->deleteAsset(*deleteTarget, error))
							{
								ModalDialog_MessageBox::showMessageBox(m_ownerAppStage, error);
							}
						});
				});
		});
}
