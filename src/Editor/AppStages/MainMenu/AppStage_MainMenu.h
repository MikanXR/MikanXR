#pragma once

//-- includes -----
#include "AppStage.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "Shared/GuiDataSource_StringList.h"

//-- definitions -----
class AppStage_MainMenu : public AppStage
{
public:
	AppStage_MainMenu(class IEditorWindow* ownerWindow);

	virtual void enter() override;

	static const char* APP_STAGE_NAME;

protected:
	void onResumeProject();
	void onOpenProject();
	void onNewProject();
	void onExit();
	void showPendingCrashReport();

	// -- AppStage --
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport);

	// -- IRemoteControllableAppStage Interface -- //
	virtual bool handleRemoteControlCommand(const std::string& command, const std::vector<std::string>& parameters,
											std::vector<std::string>& outResults);
	bool handleResumeProjectCommand(std::vector<std::string>& outResults);
	bool handleOpenProjectCommand(const std::vector<std::string>& parameters, std::vector<std::string>& outResults);
	bool handleNewProjectCommand(const std::vector<std::string>& parameters, std::vector<std::string>& outResults);
	bool handleExitCommand(std::vector<std::string>& outResults);

private:
	AppSettingsConfigPtr m_appSettingsConfig;
	ProjectManagerPtr m_projectManager;
	IMkTriangulatedMeshPtr m_fullscreenRGBQuad;

	std::string m_selectedLanguageId;
	// Parallel lists: the combo displays native names, the selection maps back
	// through the matching code
	std::vector<std::string> m_languageIdList;
	std::vector<std::string> m_languageNameList;
	GuiDataSource_StringList m_languageDataSource;

	// Set on enter, consumed by the first onGui so the notice pushes from the GUI pass
	bool m_bCheckPendingCrashReport= false;
};
