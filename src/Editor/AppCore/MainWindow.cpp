//-- includes -----
#include "MainWindow.h"
#include "Logger.h"
#include "Version.h"

#include "App.h"
#include "AppSettingsConfig.h"
#include "AppStage.h"
#include "AnchorObjectSystem.h"
#include "ARKitDebugChannel.h"
#include "AutomationServer.h"
#include "TransactionHistory.h"
#include "ProjectAssetCatalog.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "ClientSourceManager.h"
#include "VideoSourceComponent.h"
#include "EditorObjectSystem.h"
#include "InputManager.h"
#include "IMkGraphicsContext.h"
#include "IMkWindowContext.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTextRenderer.h"
#include "IMkLineRenderer.h"
#include "MathUtility.h"
#include "MathGLM.h"
#include "MikanCamera.h"
#include "IMkFontManager.h"
#include "MikanServer.h"
#include "MikanViewport.h"
#include "MikanModelResourceManager.h"
#include "MkGuiContext.h"
#include "MkGuiDockspace.h"
#include "MkGuiScopedUpdate.h"
#include "MkGuiStyleManager.h"
#include "MkStateStack.h"
#include "MkStateModifiers.h"
#include "MikanTextRenderer.h"
#include "MkWindowEvent.h"
#include "PathUtils.h"
#include "ProjectManager.h"
#include "OpenCVManager.h"
#include "LuaDebugServer.h"
#include "ScriptFolderWatcher.h"
#include "StencilUtils.h"
#include "StringUtils.h"
#include "TextStyle.h"

#include <chrono>
#include <cstdlib>

// App Stages
#include "AlignmentCalibration/AppStage_AlignmentCalibration.h"
#include "AlignCameraByUtilityMarker/AppStage_AlignCameraByUtilityMarker.h"
#include "AlignCameraByOriginMarker/AppStage_AlignCameraByOriginMarker.h"
#include "AnchorTriangulation/AppStage_AnchorTriangulation.h"
#include "LightFixtureCalibration/AppStage_LightFixtureCalibration.h"
#include "DepthMeshCapture/AppStage_DepthMeshCapture.h"
#include "SceneLightingCapture/AppStage_SceneLightingCapture.h"
#include "MainMenu/AppStage_MainMenu.h"
#include "MonoLensCalibration/AppStage_MonoLensCalibration.h"
#include "Project/AppStage_Project.h"
#include "StencilAlignment/AppStage_StencilAlignment.h"
#include "TextureSourceSettings/AppStage_TextureSourceSettings.h"
#include "VideoSourceSettings/AppStage_VideoSourceSettings.h"
#include "VRTrackingRecenter/AppStage_VRTrackingRecenter.h"

#include <algorithm>

#include <easy/profiler.h>

//-- constants -----
static const int k_window_pixel_width= 1280;
static const int k_window_pixel_height= 720;

static const glm::vec4 k_clear_color= glm::vec4(0.45f, 0.45f, 0.5f, 1.f);

static const glm::vec3 k_frustum_color= glm::vec3(0.1f, 0.7f, 0.3f);

//-- public methods -----
MainWindow::MainWindow(App* ownerApp)
	: EditorWindow(ownerApp)
	, m_mikanServer(new MikanServer())
	, m_automationServer(new AutomationServer())
	, m_arkitDebugChannel(new ARKitDebugChannel())
	, m_transactionHistory(new TransactionHistory())
	, m_clientSourceManager(new ClientSourceManager(DEFAULT_VIDEO_FRAME_QUEUE_SIZE))
	, m_inputManager(new InputManager(this))
	, m_projectManager(std::make_shared<ProjectManager>(this))
	, m_assetCatalog(new ProjectAssetCatalog())
	, m_scriptFolderWatcher(new ScriptFolderWatcher())
	, m_openCVManager(new OpenCVManager())
	, m_fontManager(createMkFontManager())
	, m_appStageFactory(this)
	, m_isRenderingStage(false)
	, m_isRenderingUI(false)
	, m_bIsMainWindowGuiHidden(ownerApp->hasCommandLineFlag("hideMainWindowGUI"))
{
	m_graphicsContext= createMkGraphicsContext(eGraphicsAPI::OpenGL, m_fontManager.get());
	m_mkWindowContext= createMkWindowContext(m_ownerApp->getWindowManager(), m_graphicsContext);
	m_modelResourceManager=
		MikanModelResourceManagerUniquePtr(new MikanModelResourceManager(getGraphicsContext().get()));

	m_appStageFactory.addAppStageConstructor<AppStage_AlignmentCalibration>();
	m_appStageFactory.addAppStageConstructor<AppStage_AlignCameraByUtilityMarker>();
	m_appStageFactory.addAppStageConstructor<AppStage_AlignCameraByOriginMarker>();
	m_appStageFactory.addAppStageConstructor<AppStage_AnchorTriangulation>();
	m_appStageFactory.addAppStageConstructor<AppStage_DepthMeshCapture>();
	m_appStageFactory.addAppStageConstructor<AppStage_LightFixtureCalibration>();
	m_appStageFactory.addAppStageConstructor<AppStage_SceneLightingCapture>();
	m_appStageFactory.addAppStageConstructor<AppStage_MainMenu>();
	m_appStageFactory.addAppStageConstructor<AppStage_MonoLensCalibration>();
	m_appStageFactory.addAppStageConstructor<AppStage_Project>();
	m_appStageFactory.addAppStageConstructor<AppStage_StencilAlignment>();
	m_appStageFactory.addAppStageConstructor<AppStage_TextureSourceSettings>();
	m_appStageFactory.addAppStageConstructor<AppStage_VideoSourceSettings>();
	m_appStageFactory.addAppStageConstructor<AppStage_VRTrackingRecenter>();

	m_clientSourceManager->setTextureQueueSizeResolver(
		[this](MikanCameraID cameraId) -> int
		{
			ProjectManagerPtr projectManager= getProjectManager();
			CameraObjectSystemPtr cameraSystem=
				projectManager ? projectManager->getSystemOfType<CameraObjectSystem>() : nullptr;
			CameraComponentPtr camera= cameraSystem ? cameraSystem->getCameraById(cameraId) : nullptr;
			VideoSourceComponentPtr videoSource= camera ? camera->getVideoSourceComponent() : nullptr;

			return videoSource ? videoSource->getVideoSourceDefinition()->getVideoFrameQueueSize() : 0;
		});
}

MainWindow::~MainWindow()
{
	delete m_scriptFolderWatcher;
	delete m_assetCatalog;
	m_projectManager= nullptr;
	delete m_openCVManager;
	delete m_inputManager;
	delete m_transactionHistory;
	delete m_arkitDebugChannel;
	delete m_automationServer;
	delete m_mikanServer;
	delete m_clientSourceManager;
}

EventBus* MainWindow::getEventBus() const { return m_ownerApp->getEventBus(); }

LocalizationManager* MainWindow::getLocalizationManager() const { return m_ownerApp->getLocalizationManager(); }

IMkViewportPtr MainWindow::getRenderingViewport() const { return m_graphicsContext->getRenderingViewport(); }

bool MainWindow::startup()
{
	EASY_FUNCTION();

	bool success= true;

	MIKAN_LOG_INFO("MainWindow::init()") << "Initializing MainWindow";

#define MIKAN_TIMED_STARTUP(label, expr)                                                                               \
	do                                                                                                                 \
	{                                                                                                                  \
		auto _t0= std::chrono::high_resolution_clock::now();                                                           \
		expr;                                                                                                          \
		auto _t1= std::chrono::high_resolution_clock::now();                                                           \
		MIKAN_LOG_INFO("MainWindow::startup")                                                                          \
			<< label ": " << std::chrono::duration_cast<std::chrono::milliseconds>(_t1 - _t0).count() << "ms";         \
	} while (0)

	auto windowTitle= StringUtils::stringify("MikanXR v", MIKAN_RELEASE_VERSION_STRING);
	if (success && !startupWindow(windowTitle, k_window_pixel_width, k_window_pixel_height))
	{
		success= false;
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("startupGuiContext", ok= startupGuiContext("main", /*bEnableDocking=*/true));
		if (!ok)
			success= false;
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("startupStyleManager", ok= startupStyleManager());
		if (!ok)
			success= false;
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("openCVManager::startup", ok= m_openCVManager->startup());
		if (!ok)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize OpenCV manager!";
			success= false;
		}
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("startupTextureCache", ok= startupTextureCache());
		if (!ok)
			success= false;
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("startupModelResourceManager", ok= startupModelResourceManager());
		if (!ok)
			success= false;
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("fontManager::startup", ok= m_fontManager->startup());
		if (!ok)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize baked text cache!";
			success= false;
		}
	}

	if (success)
	{
		// Start the Lua remote debug server (non-blocking) before the first
		// project loads, since the project script context attaches on load
		LuaDebugServer::getInstance()->startListening();
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("projectManager::startup", ok= m_projectManager->startup(this));
		if (!ok)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize the object system manager";
			success= false;
		}
	}

	if (success)
	{
		// Scans the initial project's asset folders and follows project switches
		m_assetCatalog->setBundledResourcesEditable(m_ownerApp->getAppSettings()->getEditBundledResources());
		m_assetCatalog->startup(this);
		m_scriptFolderWatcher->startup(this);
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("clientSourceManager::startup", ok= m_clientSourceManager->startup());
		if (!ok)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize the client source manager";
			success= false;
		}
	}

	if (success)
	{
		bool ok= false;
		MIKAN_TIMED_STARTUP("mikanServer::startup", ok= m_mikanServer->startup(this));
		if (!ok)
		{
			MIKAN_LOG_ERROR("App::init") << "Failed to initialize the MikanXR server";
			success= false;
		}
	}

	if (success)
	{
		// The automation text command server (loopback only) can drive and
		// script the editor, so it opens only when asked for: the app setting,
		// -automationServer, or -automationPort, which implies the flag since a
		// port with nothing listening is a trap. -noAutomationServer holds it
		// closed for the session whatever the setting says.
		// Its namespaces register either way, so the command registry survives
		// the listener being toggled off and on from the settings panel.
		m_automationServer->initialize(this);

		bool bAutomationEnabled= m_ownerApp->getAppSettings()->getAutomationServerEnabled();
		if (m_ownerApp->hasCommandLineFlag("automationServer")
			|| !m_ownerApp->getCommandLineStringArg("automationPort").empty())
		{
			bAutomationEnabled= true;
		}
		if (getIsAutomationServerLockedOff())
		{
			bAutomationEnabled= false;
		}

		if (bAutomationEnabled)
		{
			// A failed bind is logged and tolerated
			m_automationServer->startListener(resolveAutomationServerPort());
		}
	}

	if (success)
	{
		// Start transaction recording (binds to the already-loaded initial
		// project) and expose the history commands over the automation channel
		m_transactionHistory->startup(this);
		m_transactionHistory->registerAutomationCommands(m_automationServer);
	}

	if (success)
	{
		// The ARKit debug channel binds every interface rather than loopback
		// only, so it is opt-in as well. Its `arkit` namespace registers either
		// way, so `arkit status` can report that it is off.
		bool bARKitDebugEnabled= m_ownerApp->getAppSettings()->getARKitDebugChannelEnabled();
		if (m_ownerApp->hasCommandLineFlag("arkitDebugChannel"))
			bARKitDebugEnabled= true;

		if (bARKitDebugEnabled)
		{
			int arkitDebugPort= m_ownerApp->getAppSettings()->getARKitDebugChannelPort();
			const std::string portOverride= m_ownerApp->getCommandLineStringArg("arkitDebugPort");
			if (!portOverride.empty())
				arkitDebugPort= atoi(portOverride.c_str());

			// A failed bind is logged and tolerated, matching the automation server
			m_arkitDebugChannel->startup((uint16_t)arkitDebugPort);
		}

		m_arkitDebugChannel->registerAutomationCommands(m_automationServer);
	}

#undef MIKAN_TIMED_STARTUP

	if (success)
	{
		// Create the base GL state for the window
		IMkState* mkState= m_graphicsContext->getMkStateStack().pushState("MainWindow Root");
		assert(mkState->getStackDepth() == 0);

		// Set default state flags at the base of the stack
		mkState->disableFlag(eMkStateFlagType::cullFace);

		// Set the default clear color
		mkStateSetClearColor(mkState, k_clear_color);

		// Default to the full window viewport
		mkStateSetViewport(mkState, 0, 0, m_mkWindowContext->getWidth(), m_mkWindowContext->getHeight());

		// Create a fullscreen viewport for the UI (which creates it's own camera).
		// It sizes itself from the window, so it starts correct even when the OS
		// hands back something other than the requested dimensions.
		m_uiViewport= std::make_shared<MikanViewport>(this);
	}

	if (success)
	{
		pushAppStageOfType<AppStage_MainMenu>();
	}

	return success;
}

void MainWindow::update(float deltaSeconds)
{
	MkGuiScopedUpdate mkScopedCtx(*m_guiContext);

	// Poll rendered frames from client connections
	m_mikanServer->update();

	// Service Lua debugger socket I/O (between Lua script updates)
	LuaDebugServer::getInstance()->poll();

	// Reload project scripts and refresh the asset catalog when a .lua file changes on disk
	m_scriptFolderWatcher->update(deltaSeconds);

	// Service automation command socket I/O (dispatches commands inline)
	m_automationServer->poll();

	// Service the ARKit debug channel after the automation server, so a command
	// dispatched this frame can have its reply picked up on the next one
	m_arkitDebugChannel->poll(deltaSeconds);

	// Seal any transaction whose coalescing window lapsed
	m_transactionHistory->update(deltaSeconds);

	// Garbage collect stale baked text
	m_fontManager->garbageCollect();

	// Process any pending app stage operations queued by pushAppStage/popAppStage from last frame
	processPendingAppStageOps();

	// Apply a project switch requested from a project stage's File menu, now
	// that the requesting stage has popped
	processPendingProjectRequest();

	// Process most recent SDL events (keyboard, mouse, etc)
	m_mkWindowContext->handleEvents(this);

	// Update objects in the object system
	m_projectManager->update(deltaSeconds);

	// Update the current app stage last
	AppStage* appStage= getCurrentAppStage();
	if (appStage != nullptr && appStage->getIsUpdateActive())
	{
		// Process input events in the Debug UI and render the UI
		if (!m_bIsMainWindowGuiHidden && !m_projectManager->isAnySystemLoading())
		{
			EASY_BLOCK("appStage onGui");
			beginDockspaceHost(appStage);
			appStage->onGui();
			endDockspaceHost();
		}

		// Update the simulation of the current app stage
		{
			EASY_BLOCK("appStage Update");
			appStage->update(deltaSeconds);
		}
	}
}

void MainWindow::setAutomationServerEnabled(bool bEnabled)
{
	assert(m_automationServer != nullptr);

	if (getIsAutomationServerLockedOff())
		return;

	if (bEnabled)
	{
		if (!m_automationServer->isListening())
		{
			m_automationServer->startListener(resolveAutomationServerPort());
		}
	}
	else
	{
		m_automationServer->stopListener();
	}
}

void MainWindow::restartAutomationServerListener()
{
	assert(m_automationServer != nullptr);

	if (m_automationServer->isListening())
	{
		m_automationServer->stopListener();
		m_automationServer->startListener(resolveAutomationServerPort());
	}
}

bool MainWindow::getIsAutomationServerLockedOff() const { return m_ownerApp->hasCommandLineFlag("noAutomationServer"); }

uint16_t MainWindow::resolveAutomationServerPort() const
{
	int port= m_ownerApp->getAppSettings()->getAutomationServerPort();

	const std::string portOverride= m_ownerApp->getCommandLineStringArg("automationPort");
	if (!portOverride.empty())
		port= atoi(portOverride.c_str());

	return (uint16_t)port;
}

void MainWindow::requestOpenProject(const std::filesystem::path& projectFilePath)
{
	m_pendingProjectRequest= ePendingProjectRequest::open;
	m_pendingProjectRequestPath= projectFilePath;
}

void MainWindow::requestNewProject(const std::filesystem::path& projectFilePath)
{
	m_pendingProjectRequest= ePendingProjectRequest::create;
	m_pendingProjectRequestPath= projectFilePath;
}

void MainWindow::processPendingProjectRequest()
{
	if (m_pendingProjectRequest == ePendingProjectRequest::none)
		return;

	// Wait for the requesting stage to finish popping: the main menu stage is
	// the one that knows how to load a project and push the project stage
	AppStage* appStage= getCurrentAppStage();
	if (appStage == nullptr || appStage->getUsesDockspace())
		return;

	const char* command= m_pendingProjectRequest == ePendingProjectRequest::open ? "open_project" : "new_project";
	const std::vector<std::string> parameters= {m_pendingProjectRequestPath.string()};
	std::vector<std::string> results;

	m_pendingProjectRequest= ePendingProjectRequest::none;
	m_pendingProjectRequestPath.clear();

	appStage->handleRemoteControlCommand(command, parameters, results);
}

void MainWindow::beginDockspaceHost(AppStage* appStage)
{
	m_bDockspaceHostOpen= false;

	if (appStage == nullptr || !appStage->getUsesDockspace())
		return;

	bool bNeedsDefaultLayout= false;
	const ImGuiID dockspaceId= MkGui::beginDockspaceHost("##MikanDockHost", "MikanDockspace", bNeedsDefaultLayout,
														 appStage->consumeDockLayoutResetRequest());
	m_bDockspaceHostOpen= true;

	if (bNeedsDefaultLayout)
	{
		appStage->onBuildDefaultDockLayout((unsigned int)dockspaceId);
		MkGui::dockBuilderFinish(dockspaceId);
	}

	if (ImGui::BeginMenuBar())
	{
		appStage->onMenuBarGui();
		ImGui::EndMenuBar();
	}
}

void MainWindow::endDockspaceHost()
{
	if (m_bDockspaceHostOpen)
	{
		MkGui::endDockspaceHost();
		m_bDockspaceHostOpen= false;
	}
}

void MainWindow::render()
{
	AppStage* appStage= getCurrentAppStage();

	if (appStage != nullptr)
	{
		// Clear the window
		m_graphicsContext->renderBegin();

		// Render all enabled 3d viewports for the app state
		for (MikanViewportPtr viewpoint : appStage->getViewportList())
		{
			if (viewpoint->getIsRenderingEnabled())
			{
				renderStageViewport(appStage, viewpoint);
			}
		}

		// Render the UI on top
		renderStageUI(appStage);

		// Finalize rendering
		m_graphicsContext->renderEnd();

		// Capture the finished frame for any pending automation screenshot, then present the
		// rendered frame (may block on vsync or SteamVR overlay DWM handshake)
		presentFrame();
	}
}

void MainWindow::renderStageViewport(AppStage* appStage, IMkViewportPtr targetViewport)
{
	EASY_FUNCTION();

	MkScopedState scopedState= m_graphicsContext->getMkStateStack().createScopedState("appStage viewport render");
	IMkState* glState= scopedState.getStackState();

	// Registers this viewport with the graphics context for the duration of the
	// scoped state. Deregisters automatically via onRenderingViewportRevert when
	// the scoped state pops at end of this function.
	targetViewport->applyRenderingViewport(glState);

	// Set window state flag that we are in the middle of rendering a stage
	// Used for safety checks in the render functions
	m_isRenderingStage= true;

	// Render the 3d geometry of the AppStage
	appStage->render(targetViewport);

	// Render any 3D line segments emitted by the AppStage
	m_graphicsContext->getLineRenderer()->render(false);

	// Render any glyphs emitted by the AppStage
	m_graphicsContext->getTextRenderer()->render();

	// Rendering the state is done
	m_isRenderingStage= false;
}

void MainWindow::renderStageUI(AppStage* appStage)
{
	EASY_FUNCTION();

	MkScopedState scopedState= m_graphicsContext->getMkStateStack().createScopedState("appStage renderUI");
	IMkState* glState= scopedState.getStackState();

	// Registers the UI viewport with the graphics context for the duration of the
	// scoped state. Deregisters automatically via onRenderingViewportRevert when
	// the scoped state pops at end of this function.
	m_uiViewport->applyRenderingViewport(glState);

	m_isRenderingUI= true;

	// Submit the MkGui draw call
	m_guiContext->submitDrawData();

	// Frame rate readout, in the corner of the stage's scene view rather than the
	// window, so it does not sit under the docked panels. Drawn unless the editor
	// settings turn it off; with no project loaded there are no settings to read
	// and it stays on.
	bool bRenderFrameRate= true;
	if (EditorObjectSystemPtr editorSystem= m_projectManager->getSystemOfType<EditorObjectSystem>())
	{
		bRenderFrameRate= editorSystem->getEditorSettings().bRenderFrameRate;
	}

	if (bRenderFrameRate)
	{
		// Stages without a viewport (the main menu) fall back to the window corner
		glm::vec2 frameRateAnchor((float)getWidth() - 1.f, (float)getHeight() - 1.f);
		const MikanViewportList& viewports= appStage->getViewportList();
		if (!viewports.empty() && viewports[0])
		{
			const glm::i32vec2 viewportOrigin= viewports[0]->getViewportOrigin();
			const glm::i32vec2 viewportSize= viewports[0]->getViewportSize();
			if (viewportSize.x > 0 && viewportSize.y > 0)
			{
				frameRateAnchor= glm::vec2((float)(viewportOrigin.x + viewportSize.x - 1),
										   (float)(viewportOrigin.y + viewportSize.y - 1));
			}
		}

		TextStyle style= getDefaultTextStyle();
		style.horizontalAlignment= eHorizontalTextAlignment::Right;
		style.verticalAlignment= eVerticalTextAlignment::Bottom;
		drawTextAtScreenPosition(m_graphicsContext.get(), style, frameRateAnchor, L"%.1ffps",
								 App::getInstance()->getFPS());
	}

	// Show "Loading..." centered on screen while background systems are initializing
	if (m_projectManager->isAnySystemLoading())
	{
		TextStyle loadingStyle= getDefaultTextStyle();
		loadingStyle.horizontalAlignment= eHorizontalTextAlignment::Middle;
		loadingStyle.verticalAlignment= eVerticalTextAlignment::Middle;
		loadingStyle.pointSize= 96;
		loadingStyle.hasShadow= true;
		loadingStyle.shadowColor= {0.f, 0.f, 0.f};
		loadingStyle.shadowOffset= {3, 3};
		loadingStyle.shadowOpacity= 0.8f;
		drawTextAtScreenPosition(m_graphicsContext.get(), loadingStyle,
								 glm::vec2(getWidth() * 0.5f, getHeight() * 0.5f), L"Loading...");
	}

	// Render any 2D line segments emitted by the AppStage renderUI phase
	m_graphicsContext->getLineRenderer()->render(true);

	// Render any glyphs emitted by the AppStage renderUI phase
	m_graphicsContext->getTextRenderer()->render();

	m_isRenderingUI= false;
}

void MainWindow::shutdown()
{
	m_uiViewport= nullptr;

	// Tear down all active app stages
	while (getCurrentAppStage() != nullptr)
	{
		popAppState();
	}
	processPendingAppStageOps();

	assert(m_transactionHistory != nullptr);
	m_transactionHistory->shutdown();

	assert(m_assetCatalog != nullptr);
	m_assetCatalog->shutdown();

	assert(m_scriptFolderWatcher != nullptr);
	m_scriptFolderWatcher->shutdown();

	// Before the automation server, so an in-flight `arkit send` still gets its
	// error reply out while the automation socket is alive to carry it
	assert(m_arkitDebugChannel != nullptr);
	m_arkitDebugChannel->shutdown();

	assert(m_automationServer != nullptr);
	m_automationServer->shutdown();

	assert(m_mikanServer != nullptr);
	m_mikanServer->shutdown();

	assert(m_clientSourceManager != nullptr);
	m_clientSourceManager->shutdown();

	// Dispose all ObjectSystems
	assert(m_projectManager != nullptr);
	m_projectManager->shutdown();

	assert(m_fontManager != nullptr);
	m_fontManager->shutdown();

	shutdownModelResourceManager();
	shutdownTextureCache();
	shutdownStyleManager();
	shutdownGuiContext();
	shutdownWindow();
}

bool MainWindow::onWindowEvent(const MkWindowEvent& event)
{
	bool bHandled= false;

	const auto eventType= event.getEventType();
	const auto keySym= event.getKeySym();

	// First see if we got an app shutdown request. Escape inside a text field
	// belongs to the field (it cancels the edit), not to the app.
	const bool bEscapeOutsideTextField= eventType == eMkWindowEventType::KeyDown && keySym == MkKey::ESCAPE
										&& !(m_guiContext && m_guiContext->wantsTextInput());
	if (eventType == eMkWindowEventType::Quit || bEscapeOutsideTextField)
	{
		MIKAN_LOG_INFO("App::exec") << "QUIT message received";
		App::getInstance()->requestShutdown();
		bHandled= true;
	}
	// Toggle debug UI with F11
	else if (eventType == eMkWindowEventType::KeyUp && keySym == MkKey::F11)
	{
		m_bIsMainWindowGuiHidden= !m_bIsMainWindowGuiHidden;
	}

	// Then see if the UI wants to handle the event
	if (!bHandled && !m_bIsMainWindowGuiHidden)
	{
		bHandled= m_guiContext->onWindowEvent(event);
	}

	// Then see if the current app stage wants to handle the event
	AppStage* appStage= getCurrentAppStage();
	if (appStage != nullptr)
	{
		appStage->onWindowEvent(event);
	}

	// Then see if the main window object simulation wants to handle the event
	if (!bHandled)
	{
		if (m_mkWindowContext->hasMouseFocus() || m_mkWindowContext->hasKeyboardFocus())
		{
			bHandled= m_inputManager->onWindowEvent(event);
		}
	}

	return bHandled;
}

inline AppStage* MainWindow::getCurrentAppStage() const
{
	return (m_appStageStack.size() > 0) ? m_appStageStack[m_appStageStack.size() - 1].get() : nullptr;
}

inline AppStage* MainWindow::getParentAppStage() const
{
	return (m_appStageStack.size() > 1) ? m_appStageStack[m_appStageStack.size() - 2].get() : nullptr;
}

AppStage* MainWindow::pushAppStage(const std::string& appStageName)
{
	assert(bAppStackOperationAllowed);

	AppStagePtr newAppStage= m_appStageFactory.allocateAppStage(appStageName);
	if (newAppStage)
	{
		AppStagePtr parentAppStage=
			m_appStageStack.size() > 0 ? m_appStageStack[m_appStageStack.size() - 1] : AppStagePtr();

		m_appStageStack.push_back(newAppStage);
		m_pendingAppStageOps.push_back({parentAppStage, newAppStage, AppStageOperation::enter});

		return newAppStage.get();
	}

	return nullptr;
}

void MainWindow::popAppState()
{
	assert(bAppStackOperationAllowed);
	AppStagePtr appStage= m_appStageStack.size() > 0 ? m_appStageStack[m_appStageStack.size() - 1] : AppStagePtr();
	if (appStage)
	{
		m_appStageStack.pop_back();

		AppStagePtr parentAppStage=
			m_appStageStack.size() > 0 ? m_appStageStack[m_appStageStack.size() - 1] : AppStagePtr();

		m_pendingAppStageOps.push_back({parentAppStage, appStage, AppStageOperation::exit});
	}
}

void MainWindow::processPendingAppStageOps()
{
	// Disallow app stack operations during enter or exit
	bAppStackOperationAllowed= false;

	for (auto& pendingAppStageOp : m_pendingAppStageOps)
	{
		switch (pendingAppStageOp.op)
		{
		case AppStageOperation::enter:
		{
			EASY_BLOCK("appStage Enter");

			// Pause the parent app stage
			if (pendingAppStageOp.parentAppStage)
				pendingAppStageOp.parentAppStage->pause();

			// Create a new input event set for the app state
			m_inputManager->pushEventBindingSet();

			// Enter the new app stage
			pendingAppStageOp.appStage->enter();

			// Notify any object systems that care about app stage transitions
			if (OnAppStageEntered)
				OnAppStageEntered(pendingAppStageOp.parentAppStage.get(), pendingAppStageOp.appStage.get());
		}
		break;
		case AppStageOperation::exit:
		{
			EASY_BLOCK("appStage Exit");

			// Notify any object systems that care about app stage transitions
			if (OnAppStageExited)
				OnAppStageExited(pendingAppStageOp.appStage.get(), pendingAppStageOp.parentAppStage.get());

			// Exit the app stage we are leaving
			pendingAppStageOp.appStage->exit();

			// Clean up the input event set for the deactivated app stage
			m_inputManager->popEventBindingSet();

			// Resume the parent app stage we are restoring (if any)
			if (pendingAppStageOp.parentAppStage != nullptr)
				pendingAppStageOp.parentAppStage->resume();

			// Free the app state
			pendingAppStageOp.appStage= nullptr;
		}
		break;
		}
	}
	m_pendingAppStageOps.clear();

	// App stack operations allowed during update
	bAppStackOperationAllowed= true;
}