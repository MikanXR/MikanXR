#pragma once

//-- includes -----
#include "AppStage.h"
#include "EditorWindow.h"
#include "IMkFontManager.h"
#include "MikanRendererFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <assert.h>

//-- definitions -----
class MainWindow : public EditorWindow
{
public:
	MainWindow(class App* ownerApp);
	~MainWindow();

	// -- IMkWindowEventListener ----
	virtual bool onWindowEvent(const MkWindowEvent& event) override;

	// -- IEditorWindow ----
	virtual bool startup() override;
	virtual void update(float deltaSeconds) override;
	virtual void render() override;
	virtual void shutdown() override;

	virtual bool getIsRenderingStage() const override { return m_isRenderingStage; }
	virtual IMkViewportPtr getRenderingViewport() const override;

	virtual class EventBus* getEventBus() const override;
	virtual class LocalizationManager* getLocalizationManager() const override;
	virtual class MikanServer* getMikanServer() const override { return m_mikanServer; }
	virtual class TransactionHistory* getTransactionHistory() const override { return m_transactionHistory; }
	virtual class ClientSourceManager* getClientSourceManager() const override { return m_clientSourceManager; }
	virtual class InputManager* getInputManager() const override { return m_inputManager; }
	virtual ProjectManagerPtr getProjectManager() const override { return m_projectManager; }
	virtual class ProjectAssetCatalog* getAssetCatalog() const override { return m_assetCatalog; }
	virtual class OpenCVManager* getOpenCVManager() const override { return m_openCVManager; }
	virtual class IMkFontManager* getFontManager() const override { return m_fontManager.get(); }
	class AutomationServer* getAutomationServer() const { return m_automationServer; }

	// The automation channel can drive and script the editor, so it is opt-in
	// (AppSettingsConfig, or -automationServer / -automationPort on the command
	// line). These apply a settings change without a restart.
	void setAutomationServerEnabled(bool bEnabled);
	void restartAutomationServerListener();
	// True when -noAutomationServer was passed, which holds the channel closed
	// for the whole session no matter what the setting says
	bool getIsAutomationServerLockedOff() const;

	virtual AppStage* getCurrentAppStage() const override;
	virtual AppStage* getParentAppStage() const override;
	virtual AppStage* pushAppStage(const std::string& appStageName) override;
	virtual void popAppState() override;

	void processPendingAppStageOps();

protected:
	void renderStageViewport(AppStage* appStage, IMkViewportPtr targetViewport);
	void renderStageUI(AppStage* appStage);
	// Full-viewport host window carrying the menu bar and the dockspace the
	// stage's panels dock into. Returns the central node's screen rect, which is
	// the area left for the 3d scene.
	void beginDockspaceHost(AppStage* appStage);
	void endDockspaceHost();

public:
	// Project switches requested from inside a project stage (the File menu).
	// The stage pops itself, and the request is applied once the main menu stage
	// is current again, through the same commands the automation server uses.
	// Swapping the project under a live stage would pull its systems out from
	// under the panels drawing this frame.
	void requestOpenProject(const std::filesystem::path& projectFilePath);
	void requestNewProject(const std::filesystem::path& projectFilePath);

private:
	void processPendingProjectRequest();
	// The -automationPort override wins over the setting, so a toggle rebinds
	// the port the launch asked for rather than the stored one
	uint16_t resolveAutomationServerPort() const;

private:
	// Mikan API Server
	class MikanServer* m_mikanServer= nullptr;

	// Automation text command server
	class AutomationServer* m_automationServer= nullptr;
	class ARKitDebugChannel* m_arkitDebugChannel= nullptr;

	// Editor transaction recording and undo/redo
	class TransactionHistory* m_transactionHistory= nullptr;

	// Client Source Manager
	class ClientSourceManager* m_clientSourceManager= nullptr;

	// Input Manager
	class InputManager* m_inputManager= nullptr;

	// Object System manager
	ProjectManagerPtr m_projectManager;

	// The scanned project asset folders the Assets panels browse
	class ProjectAssetCatalog* m_assetCatalog= nullptr;

	// Reloads project scripts and refreshes the asset catalog when a .lua file
	// under the project or bundled scripts folder changes on disk
	class ScriptFolderWatcher* m_scriptFolderWatcher= nullptr;

	// OpenCV management
	class OpenCVManager* m_openCVManager;

	// OpenGL/SDL font/baked text string texture cache
	IMkFontManagerPtr m_fontManager;

	// App Stages
	AppStageFactory m_appStageFactory;
	int m_appStageStackIndex= -1;
	std::vector<AppStagePtr> m_appStageStack;

	IMkViewportPtr m_uiViewport;

	enum class AppStageOperation : int
	{
		enter,
		exit
	};

	struct PendingAppStageOperation
	{
		AppStagePtr parentAppStage;
		AppStagePtr appStage;
		AppStageOperation op;
	};
	std::vector<PendingAppStageOperation> m_pendingAppStageOps;
	bool bAppStackOperationAllowed= true;

	bool m_isRenderingStage;
	bool m_isRenderingUI;
	bool m_bIsMainWindowGuiHidden= false;
	bool m_bDockspaceHostOpen= false;

	enum class ePendingProjectRequest
	{
		none,
		open,
		create
	};
	ePendingProjectRequest m_pendingProjectRequest= ePendingProjectRequest::none;
	std::filesystem::path m_pendingProjectRequestPath;
};
