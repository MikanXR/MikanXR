#include "GuiPanel_ProjectSettings.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "AppStage.h"
#include "AutomationServer.h"
#include "ComponentNaming.h"
#include "EditorObjectSystem.h"
#include "IEditorWindow.h"
#include "ProjectAssetCatalog.h"
#include "LocText.h"
#include "LocalizationManager.h"
#include "MainWindow.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "Project/AppStage_Project.h"
#include "Project/ProjectGuiPanelContext.h"
#include "Shared/GuiPanel_DMXObjectSystem.h"
#include "Shared/GuiPanel_MarkerObjectSystem.h"
#include "SpoutLogRelay.h"

#include "imgui.h"
#include <algorithm>
#include <cstring>

bool GuiPanel_ProjectSettings::init(ProjectGuiPanelContext* context)
{
	m_context= context;
	AppStage_Project* ownerAppStage= context->getOwnerAppStage();
	m_editorSystem= ownerAppStage->getObjectSystemOfType<EditorObjectSystem>();

	m_defaultGuiStyle= getGuiStyleManager()->getStyle("default_component_panel");

	LocalizationManager* locManager= ownerAppStage->getOwnerWindow()->getLocalizationManager();
	m_selectedLanguageId= locManager->getLanguage();

	m_languageIdList.clear();
	m_languageNameList.clear();
	for (const LocalizationManager::LanguageInfo& info : locManager->getSupportedLanguageInfos())
	{
		m_languageIdList.push_back(info.code);
		m_languageNameList.push_back(info.nativeName);
	}
	m_languageDataSource.setEntries(m_languageNameList);

	return true;
}

void GuiPanel_ProjectSettings::dispose() { GuiPanel::dispose(); }

void GuiPanel_ProjectSettings::onGui()
{
	auto editorSystem= m_editorSystem.lock();
	if (!editorSystem)
		return;

	const auto editorConfig= editorSystem->getEditorSystemConfigConst();

	// -- Localization ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionLocalization")))
	{
		AppStage_Project* ownerAppStage= m_context->getOwnerAppStage();
		LocalizationManager* locManager= ownerAppStage->getOwnerWindow()->getLocalizationManager();
		m_selectedLanguageId= locManager->getLanguage();

		m_languageDataSource.setEntries(m_languageNameList);
		const auto selectedIt= std::find(m_languageIdList.begin(), m_languageIdList.end(), m_selectedLanguageId);
		int selectedIndex= selectedIt != m_languageIdList.end() ? (int)(selectedIt - m_languageIdList.begin()) : -1;
		if (MkGui::drawComboBoxProperty(m_defaultGuiStyle, "projectLanguage", locText("projectSettings.language"),
										&m_languageDataSource, selectedIndex))
		{
			if (selectedIndex >= 0 && selectedIndex < (int)m_languageIdList.size())
			{
				const std::string lang= m_languageIdList[selectedIndex];
				addDeferredGuiEvent([locManager, lang]() { locManager->setLanguage(lang); });
			}
		}

		// Community translations start out machine generated. Say so, rather
		// than letting an unreviewed string read as the project's own wording.
		const LocalizationManager::LanguageInfo* languageInfo= locManager->getLanguageInfo(m_selectedLanguageId);
		if (languageInfo != nullptr && languageInfo->stringCount > 0
			&& languageInfo->reviewedCount < languageInfo->stringCount)
		{
			const int reviewedPercent= (languageInfo->reviewedCount * 100) / languageInfo->stringCount;
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
			ImGui::TextWrapped(locText("projectSettings.translationReviewNoticeFmt"), reviewedPercent);
			ImGui::PopStyleColor();
		}
	}

	// -- Rendering ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionRendering")))
	{
		bool renderOrigin= editorConfig->getRenderOriginFlag();
		if (ImGui::Checkbox(locLabel("projectSettings.renderOrigin"), &renderOrigin))
		{
			addDeferredGuiEvent([this, renderOrigin]()
								{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderOriginFlag(renderOrigin); });
		}

		bool renderAnchors= editorConfig->getRenderAnchorsFlag();
		if (ImGui::Checkbox(locLabel("projectSettings.renderAnchors"), &renderAnchors))
		{
			addDeferredGuiEvent(
				[this, renderAnchors]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderAnchorsFlag(renderAnchors); });
		}

		bool renderQuadStencils= editorConfig->getRenderQuadStencilsFlag();
		if (ImGui::Checkbox(locLabel("projectSettings.renderQuadStencils"), &renderQuadStencils))
		{
			addDeferredGuiEvent(
				[this, renderQuadStencils]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderQuadStencilsFlag(renderQuadStencils); });
		}

		bool renderBoxStencils= editorConfig->getRenderBoxStencilsFlag();
		if (ImGui::Checkbox(locLabel("projectSettings.renderBoxStencils"), &renderBoxStencils))
		{
			addDeferredGuiEvent(
				[this, renderBoxStencils]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderBoxStencilsFlag(renderBoxStencils); });
		}

		bool renderModelStencils= editorConfig->getRenderModelStencilsFlag();
		if (ImGui::Checkbox(locLabel("projectSettings.renderModelStencils"), &renderModelStencils))
		{
			addDeferredGuiEvent(
				[this, renderModelStencils]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderModelStencilsFlag(renderModelStencils); });
		}

		int stencilDisplayIndex= (int)editorConfig->getModelStencilDisplayMode();
		const char* k_stencilDisplayLabels[]= {locText("projectSettings.stencilDisplaySolid"),
											   locText("projectSettings.stencilDisplayWireframe"),
											   locText("projectSettings.stencilDisplayBoth")};
		if (ImGui::Combo(locLabel("projectSettings.modelStencilDisplay"), &stencilDisplayIndex, k_stencilDisplayLabels,
						 IM_ARRAYSIZE(k_stencilDisplayLabels)))
		{
			addDeferredGuiEvent(
				[this, stencilDisplayIndex]()
				{
					m_editorSystem.lock()->getEditorSystemConfig()->setModelStencilDisplayMode(
						(eStencilDisplayMode)stencilDisplayIndex);
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.stencilDisplayTooltip"));
		}

		bool debugRenderInCompositor= editorConfig->getDebugRenderInCompositor();
		if (ImGui::Checkbox(locLabel("projectSettings.debugRenderInCompositor"), &debugRenderInCompositor))
		{
			addDeferredGuiEvent(
				[this, debugRenderInCompositor]()
				{
					m_editorSystem.lock()->getEditorSystemConfig()->setDebugRenderInCompositor(debugRenderInCompositor);
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.debugRenderInCompositorTooltip"));
		}

		bool renderFrameRate= editorConfig->getRenderFrameRate();
		if (ImGui::Checkbox(locLabel("projectSettings.renderFrameRate"), &renderFrameRate))
		{
			addDeferredGuiEvent(
				[this, renderFrameRate]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderFrameRate(renderFrameRate); });
		}

		bool renderComponentNames= editorConfig->getRenderComponentNames();
		if (ImGui::Checkbox(locLabel("projectSettings.renderComponentNames"), &renderComponentNames))
		{
			addDeferredGuiEvent(
				[this, renderComponentNames]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setRenderComponentNames(renderComponentNames); });
		}
	}

	// -- Marker ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionMarker")))
	{
		m_context->getMarkerSystemPanel()->onGui();
	}

	// -- Grid & Measurement ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.gridAndMeasurement")))
	{
		float gridExtent= editorConfig->getGridExtent();
		if (ImGui::InputFloat(locLabel("projectSettings.gridExtent"), &gridExtent, 0.5f, 1.f, "%.2f"))
		{
			if (gridExtent < 0.1f)
				gridExtent= 0.1f;
			addDeferredGuiEvent([this, gridExtent]()
								{ m_editorSystem.lock()->getEditorSystemConfig()->setGridExtent(gridExtent); });
		}

		float gridCellSize= editorConfig->getGridCellSize();
		if (ImGui::InputFloat(locLabel("projectSettings.gridCellSize"), &gridCellSize, 0.05f, 0.1f, "%.3f"))
		{
			if (gridCellSize < 0.001f)
				gridCellSize= 0.001f;
			addDeferredGuiEvent([this, gridCellSize]()
								{ m_editorSystem.lock()->getEditorSystemConfig()->setGridCellSize(gridCellSize); });
		}

		float snapIncrement= editorConfig->getSnapIncrement();
		if (ImGui::InputFloat(locLabel("projectSettings.snapIncrement"), &snapIncrement, 0.01f, 0.1f, "%.3f"))
		{
			if (snapIncrement < 0.f)
				snapIncrement= 0.f;
			addDeferredGuiEvent([this, snapIncrement]()
								{ m_editorSystem.lock()->getEditorSystemConfig()->setSnapIncrement(snapIncrement); });
		}

		bool snapEnabled= editorConfig->getSnapEnabled();
		if (ImGui::Checkbox(locLabel("projectSettings.rulerSnapToGridByDefault"), &snapEnabled))
		{
			addDeferredGuiEvent([this, snapEnabled]()
								{ m_editorSystem.lock()->getEditorSystemConfig()->setSnapEnabled(snapEnabled); });
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.rulerSnapTooltip"));
		}

		int rulerUnitsIndex= (int)editorConfig->getRulerDisplayUnits();
		const char* k_rulerUnitLabels[]= {locText("projectSettings.rulerUnitMeters"),
										  locText("projectSettings.rulerUnitCentimeters"),
										  locText("projectSettings.rulerUnitMillimeters")};
		if (ImGui::Combo(locLabel("projectSettings.rulerUnits"), &rulerUnitsIndex, k_rulerUnitLabels,
						 IM_ARRAYSIZE(k_rulerUnitLabels)))
		{
			addDeferredGuiEvent(
				[this, rulerUnitsIndex]()
				{
					m_editorSystem.lock()->getEditorSystemConfig()->setRulerDisplayUnits(
						(eRulerDisplayUnits)rulerUnitsIndex);
				});
		}

		bool debugCameraAlignment= editorConfig->getDebugCameraAlignment();
		if (ImGui::Checkbox(locLabel("projectSettings.debugCameraAlignment"), &debugCameraAlignment))
		{
			addDeferredGuiEvent(
				[this, debugCameraAlignment]()
				{ m_editorSystem.lock()->getEditorSystemConfig()->setDebugCameraAlignment(debugCameraAlignment); });
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.debugCameraAlignmentTooltip"));
		}
	}

	// -- Logging ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionLogging")))
	{
		// Spout log relay (Spout's own diagnostics, folded into the editor log)
		auto appSettings= App::getInstance()->getAppSettings();
		bool spoutLogEnabled= appSettings->getSpoutLogEnabled();
		if (ImGui::Checkbox(locLabel("projectSettings.spoutLogging"), &spoutLogEnabled))
		{
			addDeferredGuiEvent(
				[appSettings, spoutLogEnabled]()
				{
					appSettings->setSpoutLogEnabled(spoutLogEnabled);
					App::getInstance()->getSpoutLogRelay()->setEnabled(spoutLogEnabled);
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.spoutLoggingTooltip"));
		}
	}

	// -- Assets ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionAssets")))
	{
		// The bundled resources show through behind the project as read-only
		// assets; this developer switch lets the editors write them in place
		auto appSettings= App::getInstance()->getAppSettings();
		bool bEditBundled= appSettings->getEditBundledResources();
		if (ImGui::Checkbox(locLabel("projectSettings.editBundledResources"), &bEditBundled))
		{
			IEditorWindow* ownerWindow= m_ownerAppStage->getOwnerWindow();
			addDeferredGuiEvent(
				[appSettings, ownerWindow, bEditBundled]()
				{
					appSettings->setEditBundledResources(bEditBundled);
					if (ProjectAssetCatalog* catalog= ownerWindow ? ownerWindow->getAssetCatalog() : nullptr)
					{
						catalog->setBundledResourcesEditable(bEditBundled);
					}
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.editBundledResourcesTooltip"));
		}
	}

	// -- Scripting ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionScripting")))
	{
		// Script editor command (e.g. "code --reuse-window", "devenv /edit", "" = OS default)
		auto appSettings= App::getInstance()->getAppSettings();
		char editorBuf[256];
		strncpy_s(editorBuf, appSettings->getScriptEditorCommand().c_str(), sizeof(editorBuf) - 1);
		if (ImGui::InputText(locLabel("projectSettings.scriptEditorCommand"), editorBuf, sizeof(editorBuf)))
		{
			const std::string newCmd(editorBuf);
			addDeferredGuiEvent([appSettings, newCmd]() { appSettings->setScriptEditorCommand(newCmd); });
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.scriptEditorCommandTooltip"));
		}
	}

	// -- Automation ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionAutomation")))
	{
		// The loopback command channel that test scripts and AI tooling drive
		// the editor through. Off by default, and toggling it opens or closes
		// the listener without a restart.
		auto appSettings= App::getInstance()->getAppSettings();
		MainWindow* mainWindow= App::getInstance()->getMainWindow();
		AutomationServer* automationServer= mainWindow ? mainWindow->getAutomationServer() : nullptr;
		const bool bLockedOff= mainWindow != nullptr && mainWindow->getIsAutomationServerLockedOff();

		ImGui::BeginDisabled(bLockedOff);

		bool bAutomationEnabled= appSettings->getAutomationServerEnabled();
		if (ImGui::Checkbox(locLabel("projectSettings.automationServer"), &bAutomationEnabled))
		{
			addDeferredGuiEvent(
				[appSettings, mainWindow, bAutomationEnabled]()
				{
					appSettings->setAutomationServerEnabled(bAutomationEnabled);
					if (mainWindow != nullptr)
					{
						mainWindow->setAutomationServerEnabled(bAutomationEnabled);
					}
				});
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText(bLockedOff ? "projectSettings.automationServerLockedOffTooltip"
													   : "projectSettings.automationServerTooltip"));
		}

		// Changing the port rebinds an open listener. The edit commits on Enter
		// rather than per keystroke, so typing 21200 does not bind its way
		// through ports 2, 21, 212 and 2120 on the way there.
		int automationPort= appSettings->getAutomationServerPort();
		if (ImGui::InputInt(locLabel("projectSettings.automationServerPort"), &automationPort, 1, 100,
							ImGuiInputTextFlags_EnterReturnsTrue))
		{
			if (automationPort < 1)
				automationPort= 1;
			if (automationPort > 65535)
				automationPort= 65535;

			addDeferredGuiEvent(
				[appSettings, mainWindow, automationPort]()
				{
					appSettings->setAutomationServerPort(automationPort);
					if (mainWindow != nullptr)
					{
						mainWindow->restartAutomationServerListener();
					}
				});
		}

		ImGui::EndDisabled();

		// The listener state rather than the setting, so a failed bind and a
		// -automationPort override are both visible
		if (automationServer != nullptr && automationServer->isListening())
		{
			ImGui::Text(locText("projectSettings.automationServerListeningFmt"),
						(int)automationServer->getListenPort());
		}
		else
		{
			ImGui::TextUnformatted(locText("projectSettings.automationServerNotListening"));
		}
	}

	// -- Component Naming ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionComponentNaming")))
	{
		// One prefix per creatable component class. The class name is the row
		// label as well as the key in AppSettingsConfig.json, so it reads raw.
		auto appSettings= App::getInstance()->getAppSettings();
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", locText("projectSettings.componentNamingTooltip"));
		}

		for (const ComponentNamePrefixEntry& entry : getComponentNamePrefixEntries())
		{
			const std::string& className= entry.componentClassName;
			char prefixBuf[32];
			strncpy_s(prefixBuf, appSettings->getComponentNamePrefix(className).c_str(), sizeof(prefixBuf) - 1);
			if (ImGui::InputText(className.c_str(), prefixBuf, sizeof(prefixBuf)))
			{
				const std::string newPrefix(prefixBuf);
				addDeferredGuiEvent([appSettings, className, newPrefix]()
									{ appSettings->setComponentNamePrefix(className, newPrefix); });
			}
		}

		if (ImGui::Button(locLabel("projectSettings.resetComponentNamePrefixes")))
		{
			addDeferredGuiEvent([appSettings]() { appSettings->resetComponentNamePrefixes(); });
		}
	}

	// -- DMX ----
	if (MkGui::drawPropertySheetHeader(m_defaultGuiStyle, locText("projectSettings.sectionDMX")))
	{
		m_context->getDMXSystemPanel()->onGui();
	}
}
