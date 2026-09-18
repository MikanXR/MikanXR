//-- inludes -----
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedState.h"
#include "MkStateStack.h"
#include "MkStateModifiers.h"
#include "IMkState.h"
#include "IMkTriangulatedMesh.h"
#include "Project/AppStage_Project.h"
#include "MainMenu/AppStage_MainMenu.h"
#include "ModalConfirm/ModalDialog_Confirm.h"
#include "MkGuiContext.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"
#include "ProjectFileDialogs.h"
#include "ProjectManager.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "CrashHandler.h"
#include "LocText.h"
#include "MainWindow.h"
#include "OSUtils.h"
#include "PathUtils.h"
#include "Logger.h"

#include "imgui.h"
#include "tinyfiledialogs.h"

#include <algorithm>

//-- statics ----
const char* AppStage_MainMenu::APP_STAGE_NAME= "MainMenu";

//-- public methods -----
AppStage_MainMenu::AppStage_MainMenu(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_MainMenu::APP_STAGE_NAME)
{
}

void AppStage_MainMenu::enter()
{
	AppStage::enter();

	App* app= App::getInstance();
	IEditorWindow* ownerWindow= getOwnerWindow();

	m_appSettingsConfig= app->getAppSettings();
	m_projectManager= ownerWindow->getProjectManager();

	// Create the background quad
	if (!m_fullscreenRGBQuad)
	{
		MkMaterialConstPtr backgroundMaterial=
			getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(
				INTERNAL_MATERIAL_PT_PM5544_TEST_CARD);

		m_fullscreenRGBQuad=
			createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), backgroundMaterial, false);
	}

	// Language selector
	LocalizationManager* locManager= ownerWindow->getLocalizationManager();
	m_selectedLanguageId= locManager->getLanguage();

	m_languageIdList.clear();
	m_languageNameList.clear();
	for (const LocalizationManager::LanguageInfo& info : locManager->getSupportedLanguageInfos())
	{
		m_languageIdList.push_back(info.code);
		m_languageNameList.push_back(info.nativeName);
	}
	m_languageDataSource.setEntries(m_languageNameList);

	m_bCheckPendingCrashReport= true;
}

void AppStage_MainMenu::showPendingCrashReport()
{
	const std::filesystem::path reportDirectory= App::getInstance()->getCrashReportDirectory();
	if (CrashHandler::findPendingReport(reportDirectory).empty())
		return;

	const std::string question= locFormat("crashReport.questionFmt", reportDirectory.string().c_str());
	ModalDialog_Confirm::confirmQuestion(
		this, locText("crashReport.title"), question,
		[reportDirectory]()
		{
			OSUtils::openFileWithDefaultApplication(reportDirectory);
			CrashHandler::clearPendingReport(reportDirectory);
		},
		[reportDirectory]() { CrashHandler::clearPendingReport(reportDirectory); });
}

void AppStage_MainMenu::onResumeProject()
{
	std::vector<std::string> outResults;
	handleResumeProjectCommand(outResults);
}

void AppStage_MainMenu::onOpenProject()
{
	const std::filesystem::path projectFilePath= ProjectFileDialogs::pickProjectToOpen();
	if (projectFilePath.empty())
		return;

	std::vector<std::string> parameters= {projectFilePath.string()};
	std::vector<std::string> outResults;
	handleOpenProjectCommand(parameters, outResults);
}

void AppStage_MainMenu::onNewProject()
{
	const std::filesystem::path projectFilePath= ProjectFileDialogs::pickNewProjectPath();
	if (projectFilePath.empty())
		return;

	std::vector<std::string> parameters= {projectFilePath.string()};
	std::vector<std::string> outResults;
	handleNewProjectCommand(parameters, outResults);
}

void AppStage_MainMenu::onExit()
{
	std::vector<std::string> outResults;
	handleExitCommand(outResults);
}

void AppStage_MainMenu::onGui()
{
	AppStage::onGui();

	if (m_bCheckPendingCrashReport)
	{
		m_bCheckPendingCrashReport= false;
		showPendingCrashReport();
	}

	const float panelWidth= 300.f * MkGui::getUiScale();
	const ImVec2 center= ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(panelWidth, 0), ImGuiCond_Always);

	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

	MkGuiScopedWindow panel("##MainMenu", nullptr, k_flags);
	if (!panel)
		return;

	const float buttonWidth= panelWidth - 40.f * MkGui::getUiScale();
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	ImGui::TextUnformatted(locText("mainMenu.title"));
	ImGui::Separator();
	ImGui::Spacing();

	if (m_appSettingsConfig->hasLastProjectPath())
	{
		ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
		if (ImGui::Button(locLabel("mainMenu.resumeProject"), ImVec2(buttonWidth, 0)))
			onResumeProject();
	}
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	if (ImGui::Button(locLabel("mainMenu.openProject"), ImVec2(buttonWidth, 0)))
		onOpenProject();
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	if (ImGui::Button(locLabel("mainMenu.newProject"), ImVec2(buttonWidth, 0)))
		onNewProject();
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	if (ImGui::Button(locLabel("mainMenu.exit"), ImVec2(buttonWidth, 0)))
		onExit();

	// Language selector. No label: the property-row helpers put the label at x=0
	// and the value at the style's label width, which is sized for the wide
	// component panels and would overrun this narrow centered menu. The combo
	// shows the language's own native name, so it reads without one.
	ImGui::Spacing();

	LocalizationManager* locManager= getOwnerWindow()->getLocalizationManager();
	m_selectedLanguageId= locManager->getLanguage();

	const auto selectedIt= std::find(m_languageIdList.begin(), m_languageIdList.end(), m_selectedLanguageId);
	int selectedIndex= selectedIt != m_languageIdList.end() ? (int)(selectedIt - m_languageIdList.begin()) : -1;

	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	ImGui::SetNextItemWidth(buttonWidth);
	if (ImGui::Combo("##mainMenuLanguage", &selectedIndex, &MkGui::ComboBoxDataSource::itemGetter,
					 &m_languageDataSource, m_languageDataSource.getEntryCount()))
	{
		if (selectedIndex >= 0 && selectedIndex < (int)m_languageIdList.size())
		{
			locManager->setLanguage(m_languageIdList[selectedIndex]);
		}
	}

	// UI scale, reachable before a project is open so a display the editor reads
	// too small on can be fixed from the first screen. Same reasoning as the
	// language combo above for drawing it without a property row.
	ImGui::Spacing();
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	ImGui::TextUnformatted(locText("mainMenu.uiScale"));

	float uiScalePercent= m_appSettingsConfig->getUiScale() * 100.f;
	ImGui::SetCursorPosX((panelWidth - buttonWidth) * 0.5f);
	ImGui::SetNextItemWidth(buttonWidth);
	if (ImGui::SliderFloat("##mainMenuUiScale", &uiScalePercent, AppSettingsConfig::k_minUiScale * 100.f,
						   AppSettingsConfig::k_maxUiScale * 100.f, "%.0f%%"))
	{
		m_appSettingsConfig->setUiScale(uiScalePercent / 100.f);
		MkGuiContext::setUserUiScale(m_appSettingsConfig->getUiScale());
	}
}

void AppStage_MainMenu::render(IMkViewportPtr targetViewport)
{
	AppStage::render(targetViewport);

	if (m_fullscreenRGBQuad)
	{
		MkMaterialInstancePtr materialInstance= m_fullscreenRGBQuad->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();

		if (auto materialBinding= material->bindMaterial())
		{
			// TODO: "Time" and "ScreenSize" are uniforms that all materials
			//  should have available by default in the graphics context
			const double currentTimeSeconds= m_ownerWindow->getOwnerApp()->getSecondsSinceAppStart();
			const float shaderTime= (float)fmodf(currentTimeSeconds, 1000.0);
			const glm::vec2 screenSize(m_ownerWindow->getWidth(), m_ownerWindow->getHeight());
			materialInstance->setVec2BySemantic(eUniformSemantic::screenSize, screenSize);
			materialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, shaderTime);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				MkScopedState scopedState= getOwnerWindow()->getGraphicsContext()->getMkStateStack().createScopedState(
					"MainTargetDepthRender");
				IMkState* mkState= scopedState.getStackState();

				mkState->disableFlag(eMkStateFlagType::depthTest);
				mkState->disableFlag(eMkStateFlagType::cullFace);

				m_fullscreenRGBQuad->drawElements();
			}
		}
	}
}

// -- IRemoteControllableAppStage Interface -- //
bool AppStage_MainMenu::handleRemoteControlCommand(const std::string& command,
												   const std::vector<std::string>& parameters,
												   std::vector<std::string>& outResults)
{
	if (command == "resume_project")
	{
		return handleResumeProjectCommand(outResults);
	}
	else if (command == "open_project")
	{
		return handleOpenProjectCommand(parameters, outResults);
	}
	else if (command == "new_project")
	{
		return handleNewProjectCommand(parameters, outResults);
	}
	else if (command == "exit")
	{
		return handleExitCommand(outResults);
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}

bool AppStage_MainMenu::handleResumeProjectCommand(std::vector<std::string>& outResults)
{
	if (m_projectManager->hasLoadedProject())
	{
		m_ownerWindow->pushAppStageOfType<AppStage_Project>();
		outResults.push_back(IRemoteControllable::k_success);
		return true;
	}
	else if (m_appSettingsConfig->hasLastProjectPath())
	{
		std::filesystem::path projectFilePath= m_appSettingsConfig->getLastProjectPath();

		if (m_projectManager->loadProject(projectFilePath.string()))
		{
			m_ownerWindow->pushAppStageOfType<AppStage_Project>();
			outResults.push_back(IRemoteControllable::k_success);
			return true;
		}
	}

	outResults.push_back(IRemoteControllable::k_failure);
	return true;
}

bool AppStage_MainMenu::handleOpenProjectCommand(const std::vector<std::string>& parameters,
												 std::vector<std::string>& outResults)
{
	std::string projectFilePathStr= !parameters.empty() ? parameters[0] : "";

	if (!projectFilePathStr.empty())
	{
		if (m_projectManager->loadProject(projectFilePathStr))
		{
			// Remember the last opened project path
			m_appSettingsConfig->setLastProjectPath(projectFilePathStr);

			m_ownerWindow->pushAppStageOfType<AppStage_Project>();

			outResults.push_back(IRemoteControllable::k_success);
			return true;
		}
		else
		{
			outResults.push_back(IRemoteControllable::k_failure);
			return true;
		}
	}

	// Missing path parameter: the command is recognized, so report the failure
	// through the results rather than an unrecognized-command error
	outResults.push_back(IRemoteControllable::k_failure);
	return true;
}

bool AppStage_MainMenu::handleNewProjectCommand(const std::vector<std::string>& parameters,
												std::vector<std::string>& outResults)
{
	if (!parameters.empty())
	{
		const std::string& projectFilePathStr= parameters[0];

		if (m_projectManager->newProject(projectFilePathStr))
		{
			// Remember the last opened project path
			m_appSettingsConfig->setLastProjectPath(projectFilePathStr);

			m_ownerWindow->pushAppStageOfType<AppStage_Project>();
			outResults.push_back(IRemoteControllable::k_success);
		}
		else
		{
			outResults.push_back(IRemoteControllable::k_failure);
		}

		return true;
	}

	outResults.push_back(IRemoteControllable::k_failure);
	return true;
}

bool AppStage_MainMenu::handleExitCommand(std::vector<std::string>& outResults)
{
	App::getInstance()->requestShutdown();
	outResults.push_back(IRemoteControllable::k_success);

	return true;
}