///-- includes -----
#include "App.h"
#include "CameraComponent.h"
#include "CameraMath.h"
#include "ClientSourceManager.h"
#include "Colors.h"
#include "CompositorComponent.h"
#include "EditorObjectSystem.h"
#include "InputManager.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "IMkState.h"
#include "IMkTextRenderer.h"
#include "IMkTexture.h"
#include "IMkWireframeMesh.h"
#include "LightEnvironmentComponent.h"
#include "LightEnvironmentSystem.h"
#include "LocDebugUI.h"
#include "LocText.h"
#include "MathGLM.h"
#include "MathMikan.h"
#include "AppSettingsConfig.h"
#include "MainWindow.h"
#include "ProjectFileDialogs.h"
#include "Shared/LogPanel.h"
#include "MarkerObjectSystem.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MikanCamera.h"
#include "MikanViewport.h"
#include "ObjectSystemRenderQueries.h"
#include "MikanObjectSystem.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanObject.h"
#include "MkGuiDrawUtils.h"
#include "MkScene.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "ModalMessageBox/ModalDialog_ScriptErrors.h"
#include "ProjectAssetCatalog.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "Project/AppStage_Project.h"
#include "Project/ProjectGuiPanelContext.h"
#include "Project/GuiPanel_ProjectOutliner.h"
#include "Project/GuiPanel_Assets.h"
#include "Project/GuiPanel_CompositorList.h"
#include "Project/GuiPanel_HttpTriggers.h"
#include "Project/GuiPanel_ProjectSettings.h"
#include "Project/GuiPanel_SceneList.h"
#include "AnchorObjectSystem.h"
#include "BoxShapeComponent.h"
#include "BoxShapeSystem.h"
#include "BoxStencilSystem.h"
#include "CameraObjectSystem.h"
#include "ModelStencilSystem.h"
#include "QuadStencilSystem.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "QuadShapeComponent.h"
#include "QuadShapeSystem.h"
#include "ShapeComponent.h"
#include "RGBSpotLightSystem.h"
#include "ScriptObjectSystem.h"
#include "RGBPixelGridSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"
#include "TextStyle.h"
#include "TrackingVolumeComponent.h"
#include "VRTrackingVolumeComponent.h"
#include "VRObjectSystem.h"
#include "VideoSourceComponent.h"

#include <easy/profiler.h>

#include "MkGuiScopedTabBar.h"
#include "MkGuiScopedTabItem.h"
#include "MkGuiDockspace.h"
#include "MkGuiScopedWindow.h"
#include "opencv2/opencv.hpp"

//-- statics ----
const char* AppStage_Project::APP_STAGE_NAME= "Compositor";

// How long the editor camera has to sit still before its pose is written back to
// the editor settings, matching the transaction system's coalescing window
static const float k_cameraStateIdleDelay= 0.75f;

//-- public methods -----
AppStage_Project::AppStage_Project(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_Project::APP_STAGE_NAME)
{
}

AppStage_Project::~AppStage_Project() { m_viewport= nullptr; }

void AppStage_Project::enter()
{
	AppStage::enter();

	// Create a mikan scene for 3d rendering
	m_mkScene= std::make_shared<MkScene>();
	m_mkScene->setLightColor(glm::vec4(1.f, 1.f, 1.f, 1.f));
	m_mkScene->setLightDirection(glm::vec3(glm::normalize(glm::vec3(0.5f, -1.f, 0.5f))));

	// Cache a ref to the project
	m_project= getProjectConfig();

	// Cache object systems we'll be accessing
	ProjectManagerPtr objectSystemManager= m_ownerWindow->getProjectManager();
	m_editorSystem= objectSystemManager->getSystemOfType<EditorObjectSystem>();
	m_sceneObjectSystem= objectSystemManager->getSystemOfType<SceneObjectSystem>();
	m_stageObjectSystem= objectSystemManager->getSystemOfType<StageObjectSystem>();
	m_anchorObjectSystem= objectSystemManager->getSystemOfType<AnchorObjectSystem>();
	m_cameraObjectSystem= objectSystemManager->getSystemOfType<CameraObjectSystem>();
	m_markerObjectSystem= objectSystemManager->getSystemOfType<MarkerObjectSystem>();
	m_quadStencilSystem= objectSystemManager->getSystemOfType<QuadStencilSystem>();
	m_boxStencilSystem= objectSystemManager->getSystemOfType<BoxStencilSystem>();
	m_modelStencilSystem= objectSystemManager->getSystemOfType<ModelStencilSystem>();
	m_quadShapeSystem= objectSystemManager->getSystemOfType<QuadShapeSystem>();
	m_boxShapeSystem= objectSystemManager->getSystemOfType<BoxShapeSystem>();
	m_modelShapeSystem= objectSystemManager->getSystemOfType<ModelShapeSystem>();
	m_pixelGridLightSystem= objectSystemManager->getSystemOfType<RGBPixelGridSystem>();
	m_spotLightSystem= objectSystemManager->getSystemOfType<RGBSpotLightSystem>();
	m_scriptObjectSystem= objectSystemManager->getSystemOfType<ScriptObjectSystem>();

	// Stage view collision set

	// Scene view collision set: the stage set plus the scene actors
	m_sceneObjectSystemFilter.insert(m_anchorObjectSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_quadStencilSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_boxStencilSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_modelStencilSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_quadShapeSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_boxShapeSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_modelShapeSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_editorSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_cameraObjectSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_pixelGridLightSystem.lock().get());
	m_sceneObjectSystemFilter.insert(m_spotLightSystem.lock().get());

	// Tracking view collision set is empty

	// Setup Scene viewport
	{
		const glm::i32vec2 viewportOrigin= {0, 0};
		const glm::i32vec2 viewportSize= {1280, 720};

		m_viewport= getFirstViewport();
		m_viewport->setViewport(viewportOrigin, viewportSize);

		MikanCameraPtr sceneCamera= m_viewport->getCurrentMikanCamera();
		sceneCamera->setCameraMovementMode(eCameraMovementMode::fly);
		sceneCamera->setName("scene camera");

		// Put the camera back where the project was left
		restoreCameraState();

		// Register the scene with the primary viewport
		m_editorSystem.lock()->bindViewport(m_viewport);
	}

	// Create ImGui GuiPanel context and project panels
	{
		m_projectGuiPanelContext= new ProjectGuiPanelContext(this);
		m_projectGuiPanelContext->init();

		m_projectSettingsPanel= addGuiPanel<GuiPanel_ProjectSettings>();
		m_projectSettingsPanel->init(m_projectGuiPanelContext);

		m_httpTriggersPanel= addGuiPanel<GuiPanel_HttpTriggers>();
		m_httpTriggersPanel->init(m_projectGuiPanelContext);

		m_projectOutlinerPanel= addGuiPanel<GuiPanel_ProjectOutliner>();
		m_projectOutlinerPanel->init(m_projectGuiPanelContext);

		// The list panels push their picks into the outliner, so it exists first
		m_sceneListPanel= addGuiPanel<GuiPanel_SceneList>();
		m_sceneListPanel->init(m_projectGuiPanelContext, m_projectOutlinerPanel);

		m_compositorListPanel= addGuiPanel<GuiPanel_CompositorList>();
		m_compositorListPanel->init(m_projectGuiPanelContext, m_projectOutlinerPanel);

		m_assetsPanel= addGuiPanel<GuiPanel_Assets>();
		m_assetsPanel->init(m_projectGuiPanelContext);
	}

	setViewMode(eProjectViewMode::scene);

	// The scripts loaded with the project, before this stage existed, so any
	// load error from then is surfaced now
	if (ScriptObjectSystemPtr scriptSystem= m_scriptObjectSystem.lock())
	{
		scriptSystem->OnScriptError+= MakeDelegate(this, &AppStage_Project::onScriptError);
		scriptSystem->OnScriptsReloaded+= MakeDelegate(this, &AppStage_Project::onScriptsReloaded);

		for (const ScriptError& error : scriptSystem->getLastLoadErrors())
		{
			onScriptError(error);
		}
	}
}

void AppStage_Project::exit()
{
	if (ScriptObjectSystemPtr scriptSystem= m_scriptObjectSystem.lock())
	{
		scriptSystem->OnScriptError-= MakeDelegate(this, &AppStage_Project::onScriptError);
		scriptSystem->OnScriptsReloaded-= MakeDelegate(this, &AppStage_Project::onScriptsReloaded);
	}
	// AppStage::exit pops the modal stack, which deletes the dialog
	m_scriptErrorDialog= nullptr;

	// Save where the camera was left before the project goes away, along with
	// any other editor setting still waiting out the autosave cooldown
	flushCameraState();
	if (m_project)
	{
		m_project->flushPendingAutoSave();
	}

	// Clean up the 3d scene
	m_mkScene= nullptr;

	// Clean up the GuiPanel Context (panels themselves are owned by AppStage::m_guiPanels)
	delete m_projectGuiPanelContext;
	m_projectGuiPanelContext= nullptr;
	m_projectOutlinerPanel= nullptr;
	m_projectSettingsPanel= nullptr;
	m_httpTriggersPanel= nullptr;
	m_sceneListPanel= nullptr;
	m_compositorListPanel= nullptr;
	m_assetsPanel= nullptr;

	// Unregister all viewports from the editor
	m_editorSystem.lock()->clearViewports();

	// GuiPanels are disposed automatically by AppStage::exit()
	AppStage::exit();
}

void AppStage_Project::onScriptError(const ScriptError& error)
{
	if (m_scriptErrorDialog != nullptr && getCurrentModalDialog() == m_scriptErrorDialog)
	{
		m_scriptErrorDialog->addError(error);
		return;
	}

	// Another modal on top would hide it, so wait for the stack to clear
	if (getCurrentModalDialog() != nullptr)
		return;

	m_scriptErrorDialog= ModalDialog_ScriptErrors::show(this, error, [this]() { onScriptErrorDialogDismissed(); });
}

void AppStage_Project::onScriptsReloaded(bool bSuccess)
{
	// The load errors it listed are gone, so the dialog goes with them
	if (bSuccess && m_scriptErrorDialog != nullptr && getCurrentModalDialog() == m_scriptErrorDialog
		&& m_scriptErrorDialog->holdsOnlyLoadErrors())
	{
		popModalDialog();
		m_scriptErrorDialog= nullptr;
	}
}

void AppStage_Project::onScriptErrorDialogDismissed() { m_scriptErrorDialog= nullptr; }

void AppStage_Project::pause() { AppStage::pause(); }

void AppStage_Project::resume() { AppStage::resume(); }

void AppStage_Project::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	// Update the timing dependent state for the project GuiPanels
	m_projectGuiPanelContext->update(deltaSeconds);

	// The outliner's selected node kind picks the viewport view and filter
	if (m_projectOutlinerPanel != nullptr)
	{
		switch (m_projectOutlinerPanel->getSelectedNodeKind())
		{
		case eOutlinerNodeKind::trackingVolume:
			setViewMode(eProjectViewMode::tracking);
			break;
		case eOutlinerNodeKind::trackingMount:
		case eOutlinerNodeKind::marker:
		case eOutlinerNodeKind::folderMarkers:
		case eOutlinerNodeKind::folderTrackingVolumes:
		case eOutlinerNodeKind::folderSources:
		case eOutlinerNodeKind::folderCameras:
		case eOutlinerNodeKind::folderLights:
		case eOutlinerNodeKind::stage:
		case eOutlinerNodeKind::camera:
		case eOutlinerNodeKind::stageLight:
		case eOutlinerNodeKind::videoSource:
		case eOutlinerNodeKind::textureSource:
		case eOutlinerNodeKind::folderScripts:
		case eOutlinerNodeKind::script:
		default:
			setViewMode(eProjectViewMode::scene);
			break;
		}
	}

	updateCameraStateCapture(deltaSeconds);

	applyPendingProjectActions();
}

void AppStage_Project::onGui()
{
	// Process deferred events emitted due to Gui interaction
	AppStage::onGui();

	// Viewport view-mode selector overlay (perspective / orthographic), editor camera only.
	// Compositor cameras (pool index > 0) use real lens intrinsics and must not be switched.
	if (m_viewport && m_viewport->getCurrentCameraIndex() == 0)
	{
		MikanCameraPtr camera= m_viewport->getMikanCameraByIndex(0);
		if (camera)
		{
			const glm::i32vec2 vpOrigin= m_viewport->getViewportOrigin();
			ImGui::SetNextWindowPos(
				ImVec2((float)vpOrigin.x + 8 * MkGui::getUiScale(), (float)vpOrigin.y + 8 * MkGui::getUiScale()),
				ImGuiCond_Always);

			constexpr ImGuiWindowFlags k_overlayFlags= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
													   | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar
													   | ImGuiWindowFlags_AlwaysAutoResize;

			MkGuiScopedWindow overlay("##ViewModeOverlay", nullptr, k_overlayFlags);
			if (overlay)
			{
				const char* k_viewLabels[]= {locText("project.viewPerspective"), locText("project.viewTop"),
											 locText("project.viewBottom"),      locText("project.viewFront"),
											 locText("project.viewBack"),        locText("project.viewLeft"),
											 locText("project.viewRight")};

				int currentIndex= getCurrentCameraViewIndex();

				ImGui::SetNextItemWidth(110.f * MkGui::getUiScale());
				if (ImGui::Combo("##ViewMode", &currentIndex, k_viewLabels, IM_ARRAYSIZE(k_viewLabels)))
				{
					applyCameraView(currentIndex);
				}
			}
		}
	}

	// MR camera-alignment debug readout window
	if (getEditorSettings().bDebugCameraAlignment)
	{
		renderCameraAlignmentGui();
	}

	// Keep the 3d viewport on the dockspace's empty central node, so the scene
	// fills whatever the docked panels leave behind
	{
		ImVec2 centralPos, centralSize;
		if (MkGui::getDockspaceCentralRect("MikanDockspace", centralPos, centralSize) && centralSize.x >= 1.f
			&& centralSize.y >= 1.f)
		{
			m_viewport->setViewport({(int)centralPos.x, (int)centralPos.y}, {(int)centralSize.x, (int)centralSize.y});
		}
	}

	// The unified project outliner window
	if (m_bOutlinerVisible && m_projectOutlinerPanel != nullptr)
	{
		MkGuiScopedWindow outlinerWindow(locWindowTitle("windows.project"), &m_bOutlinerVisible);
		if (outlinerWindow)
		{
			m_projectOutlinerPanel->onGui();
		}
	}

	// The settings window
	if (m_bSettingsPanelVisible && m_projectSettingsPanel != nullptr)
	{
		MkGuiScopedWindow settingsWindow(locWindowTitle("windows.projectSettings"), &m_bSettingsPanelVisible);
		if (settingsWindow)
		{
			m_projectSettingsPanel->onGui();
		}
	}

	// The HTTP trigger window
	if (m_bHttpTriggersPanelVisible && m_httpTriggersPanel != nullptr)
	{
		MkGuiScopedWindow httpTriggersWindow(locWindowTitle("windows.httpTriggers"), &m_bHttpTriggersPanelVisible);
		if (httpTriggersWindow)
		{
			m_httpTriggersPanel->onGui();
		}
	}

	// The scene and compositor list windows
	if (m_bSceneListVisible && m_sceneListPanel != nullptr)
	{
		MkGuiScopedWindow sceneListWindow(locWindowTitle("windows.scenes"), &m_bSceneListVisible);
		if (sceneListWindow)
		{
			m_sceneListPanel->onGui();
		}
	}
	if (m_bCompositorListVisible && m_compositorListPanel != nullptr)
	{
		MkGuiScopedWindow compositorListWindow(locWindowTitle("windows.compositors"), &m_bCompositorListVisible);
		if (compositorListWindow)
		{
			m_compositorListPanel->onGui();
		}
	}

	// The project asset browser
	if (m_bAssetsPanelVisible && m_assetsPanel != nullptr)
	{
		MkGuiScopedWindow assetsWindow(locWindowTitle("windows.assets"), &m_bAssetsPanelVisible);
		if (assetsWindow)
		{
			m_assetsPanel->onGui();
		}
	}

	if (m_bShowLogPanel)
	{
		LogPanel::getInstance().draw(&m_bShowLogPanel);
	}

	LocDebugUI::drawWindows(m_bShowIdStackTool);
	LocDebugUI::handleShortcuts();
}

void AppStage_Project::onMenuBarGui()
{
	if (ImGui::BeginMenu(locLabel("mainWindow.fileMenu")))
	{
		if (ImGui::MenuItem(locLabel("mainWindow.newProject")))
		{
			m_pendingNewProjectPath= ProjectFileDialogs::pickNewProjectPath();
		}
		if (ImGui::MenuItem(locLabel("mainWindow.saveProject")))
		{
			// The app settings track the open project's path (set on open/new)
			AppSettingsConfigPtr appSettings= App::getInstance()->getAppSettings();
			if (appSettings && appSettings->hasLastProjectPath())
			{
				m_ownerWindow->getProjectManager()->saveProject(appSettings->getLastProjectPath().string());
			}
		}
		if (ImGui::MenuItem(locLabel("mainWindow.loadProject")))
		{
			m_pendingLoadProjectPath= ProjectFileDialogs::pickProjectToOpen();
		}
		if (ImGui::MenuItem(locLabel("mainWindow.closeProject")))
		{
			m_bPendingCloseProject= true;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(locLabel("mainWindow.exit")))
		{
			m_bPendingExit= true;
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(locLabel("mainWindow.viewMenu")))
	{
		ImGui::MenuItem(locLabel("project.panelProject"), nullptr, &m_bOutlinerVisible);
		ImGui::MenuItem(locLabel("project.panelSettings"), nullptr, &m_bSettingsPanelVisible);
		ImGui::MenuItem(locLabel("project.panelHttpTriggers"), nullptr, &m_bHttpTriggersPanelVisible);
		ImGui::MenuItem(locLabel("project.panelScenes"), nullptr, &m_bSceneListVisible);
		ImGui::MenuItem(locLabel("project.panelCompositors"), nullptr, &m_bCompositorListVisible);
		ImGui::MenuItem(locLabel("project.panelAssets"), nullptr, &m_bAssetsPanelVisible);
		ImGui::Separator();
		ImGui::MenuItem(locLabel("mainWindow.logPanel"), nullptr, &m_bShowLogPanel);
		ImGui::Separator();
		LocDebugUI::drawViewMenuItems(m_bShowIdStackTool);
		ImGui::Separator();
		if (ImGui::MenuItem(locLabel("mainWindow.resetLayout")))
		{
			m_bOutlinerVisible= true;
			m_bSettingsPanelVisible= true;
			m_bHttpTriggersPanelVisible= true;
			m_bSceneListVisible= true;
			m_bCompositorListVisible= true;
			m_bAssetsPanelVisible= true;
			m_bShowLogPanel= true;
			m_bDockLayoutResetRequested= true;
		}
		ImGui::EndMenu();
	}
}

void AppStage_Project::applyPendingProjectActions()
{
	// Applied outside the ImGui frame: swapping the project mid-frame would
	// destroy the systems the panels are drawing from
	if (m_bPendingExit)
	{
		m_bPendingExit= false;
		App::getInstance()->requestShutdown();
		return;
	}

	if (m_bPendingCloseProject)
	{
		m_bPendingCloseProject= false;
		m_ownerWindow->popAppState();
		return;
	}

	// New and load both pop back to the main menu stage, which owns the load and
	// re-push of the project stage
	if (!m_pendingNewProjectPath.empty())
	{
		const std::filesystem::path path= m_pendingNewProjectPath;
		m_pendingNewProjectPath.clear();
		App::getInstance()->getMainWindow()->requestNewProject(path);
		m_ownerWindow->popAppState();
		return;
	}

	if (!m_pendingLoadProjectPath.empty())
	{
		const std::filesystem::path path= m_pendingLoadProjectPath;
		m_pendingLoadProjectPath.clear();
		App::getInstance()->getMainWindow()->requestOpenProject(path);
		m_ownerWindow->popAppState();
	}
}

void AppStage_Project::onBuildDefaultDockLayout(unsigned int dockspaceId)
{
	// Project and Settings tabbed on the right. Along the bottom, left to right:
	// the scene list, the compositor list, the assets browser, and the log. The
	// central node is left empty for the scene.
	ImGuiID remaining= (ImGuiID)dockspaceId;
	const ImGuiID rightId= MkGui::dockBuilderSplit(remaining, ImGuiDir_Right, 0.28f, remaining);
	// The bottom row is tall enough for a row of asset tiles, and the assets
	// browser takes half of what the two lists leave, so its folder list and
	// tile grid both fit at a 720p window
	ImGuiID bottomId= MkGui::dockBuilderSplit(remaining, ImGuiDir_Down, 0.32f, remaining);
	const ImGuiID scenesId= MkGui::dockBuilderSplit(bottomId, ImGuiDir_Left, 0.15f, bottomId);
	const ImGuiID compositorsId= MkGui::dockBuilderSplit(bottomId, ImGuiDir_Left, 0.2f, bottomId);
	const ImGuiID assetsId= MkGui::dockBuilderSplit(bottomId, ImGuiDir_Left, 0.55f, bottomId);

	MkGui::dockBuilderDockWindow(locWindowTitle("windows.project"), rightId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.projectSettings"), rightId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.httpTriggers"), rightId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.scenes"), scenesId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.compositors"), compositorsId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.assets"), assetsId);
	MkGui::dockBuilderDockWindow(locWindowTitle("windows.log"), bottomId);
}

// Viewport view mode
void AppStage_Project::setViewMode(eProjectViewMode newViewMode)
{
	if (newViewMode != m_viewMode)
	{
		m_viewMode= newViewMode;
		onViewModeChanged();
	}
}

void AppStage_Project::onViewModeChanged()
{
	// Update the collision filter on the editor system
	switch (m_viewMode)
	{
	case eProjectViewMode::scene:
		m_editorSystem.lock()->setObjectSystemSelectionFilter(m_sceneObjectSystemFilter);
		break;
	default:
		m_editorSystem.lock()->setObjectSystemSelectionFilter(m_emptyObjectSystemFilter);
	}
}

// Editor camera views
void AppStage_Project::applyCameraView(int viewIndex)
{
	MikanCameraPtr camera= (m_viewport != nullptr) ? m_viewport->getMikanCameraByIndex(0) : MikanCameraPtr();
	if (!camera)
		return;

	if (viewIndex >= 1 && viewIndex <= k_orthoViewpointCount)
	{
		camera->setOrthographicViewpoint((eCameraViewpoint)(viewIndex - 1));
	}
	else
	{
		camera->setProjectionMode(eCameraProjectionMode::perspective);
		camera->setCameraMovementMode(eCameraMovementMode::fly);
	}
}

int AppStage_Project::getCurrentCameraViewIndex() const
{
	MikanCameraPtr camera= (m_viewport != nullptr) ? m_viewport->getMikanCameraByIndex(0) : MikanCameraPtr();
	if (!camera || camera->getProjectionMode() != eCameraProjectionMode::orthographic)
		return 0;

	return (int)camera->getOrthoViewpoint() + 1;
}

bool AppStage_Project::captureCameraState(EditorCameraState& outCameraState) const
{
	MikanCameraPtr camera= (m_viewport != nullptr) ? m_viewport->getMikanCameraByIndex(0) : MikanCameraPtr();
	if (!camera)
		return false;

	outCameraState.perspectivePosition= camera->getFlyPosition();
	outCameraState.perspectiveYawDegrees= camera->getFlyYawDegrees();
	outCameraState.perspectivePitchDegrees= camera->getFlyPitchDegrees();

	for (int viewIndex= 0; viewIndex < EditorCameraState::k_orthoViewCount; ++viewIndex)
	{
		const MikanOrthoViewState& orthoViewState= camera->getOrthoViewState((eCameraViewpoint)viewIndex);

		outCameraState.orthoTargets[viewIndex]= orthoViewState.target;
		outCameraState.orthoExtents[viewIndex]= orthoViewState.extent;
	}

	// The saved active view is an eCameraViewpoint index, or -1 for the perspective view
	outCameraState.activeView= getCurrentCameraViewIndex() - 1;

	return true;
}

void AppStage_Project::restoreCameraState()
{
	MikanCameraPtr camera= (m_viewport != nullptr) ? m_viewport->getMikanCameraByIndex(0) : MikanCameraPtr();
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	if (!camera || !editorSystem)
		return;

	const EditorCameraState& cameraState= editorSystem->getEditorSystemConfig()->getEditorCameraState();

	camera->setFlyPose(cameraState.perspectivePosition, cameraState.perspectiveYawDegrees,
					   cameraState.perspectivePitchDegrees);

	for (int viewIndex= 0; viewIndex < EditorCameraState::k_orthoViewCount; ++viewIndex)
	{
		camera->setOrthoViewState((eCameraViewpoint)viewIndex, cameraState.orthoTargets[viewIndex],
								  cameraState.orthoExtents[viewIndex]);
	}

	applyCameraView(cameraState.activeView + 1);

	m_pendingCameraState= cameraState;
	m_cameraStateIdleTimer= -1.f;
}

void AppStage_Project::updateCameraStateCapture(float deltaSeconds)
{
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	EditorCameraState liveCameraState;
	if (!editorSystem || !captureCameraState(liveCameraState))
		return;

	// Still moving: restart the idle window rather than write a pose per frame
	if (liveCameraState != m_pendingCameraState)
	{
		m_pendingCameraState= liveCameraState;
		m_cameraStateIdleTimer= k_cameraStateIdleDelay;
		return;
	}

	if (m_cameraStateIdleTimer >= 0.f)
	{
		m_cameraStateIdleTimer-= deltaSeconds;
		if (m_cameraStateIdleTimer < 0.f)
		{
			editorSystem->getEditorSystemConfig()->setEditorCameraState(liveCameraState);
		}
	}
}

void AppStage_Project::flushCameraState()
{
	EditorObjectSystemPtr editorSystem= m_editorSystem.lock();
	EditorCameraState liveCameraState;
	if (!editorSystem || !captureCameraState(liveCameraState))
		return;

	EditorObjectSystemDefinitionPtr editorConfig= editorSystem->getEditorSystemConfig();
	if (liveCameraState == editorConfig->getEditorCameraState())
		return;

	editorConfig->setEditorCameraState(liveCameraState);

	// Unloading a project doesn't save it and the autosave cooldown may not have
	// fired yet, so write the pose out rather than lose it on close
	if (m_project)
	{
		m_project->save();
	}
}

// Compositor Model UI Events
void AppStage_Project::onReturnEvent() { m_ownerWindow->popAppState(); }

void AppStage_Project::render(IMkViewportPtr targetViewport)
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	MikanCameraPtr viewportCamera= m_viewport->getCurrentMikanCamera();

	MkStateStack& stageStack= graphicsContext->getMkStateStack();
	MkScopedState scopedState= stageStack.createScopedState("AppStage_Project::render");
	IMkState* mkState= scopedState.getStackState();

	// Enable Depth Test while drawing the scene
	mkState->enableFlag(eMkStateFlagType::depthTest);

	// Clear the scene of any previously rendered instances
	m_mkScene->removeAllInstances();

	// View-mode specific rendering
	std::vector<ShapeComponentPtr> deferredShapeGraphs;
	switch (m_viewMode)
	{
	case eProjectViewMode::scene:
		renderProjectScene(graphicsContext, viewportCamera, deferredShapeGraphs);
		break;
	case eProjectViewMode::tracking:
		renderProjectTracking(graphicsContext, viewportCamera, StageComponentConstPtr());
		break;
	}

	// Render the 3d scene
	m_mkScene->render(viewportCamera, graphicsContext->getMkStateStack());

	// Shapes that draw through a shape node graph go last, since the pass above is what actually
	// issues the scene geometry they have to sort against. A shape graph draws with the caller's
	// state: the scene's depth test from above, plus normal alpha blending so a textured quad with
	// transparency reads the same here as under the compositor's default Draw Shapes blend.
	if (!deferredShapeGraphs.empty())
	{
		MkScopedState shapeGraphScope= stageStack.createScopedState("AppStage_Project::shapeGraphs");
		IMkState* shapeGraphState= shapeGraphScope.getStackState();
		shapeGraphState->enableFlag(eMkStateFlagType::blend);
		mkStateSetBlendFunc(shapeGraphState, eMkBlendFunction::SRC_ALPHA, eMkBlendFunction::ONE_MINUS_SRC_ALPHA);
		mkStateSetBlendEquation(shapeGraphState, eMkBlendEquation::ADD);

		const glm::mat4 viewportVpMatrix= viewportCamera->getViewProjectionMatrix();

		for (const ShapeComponentPtr& shape : deferredShapeGraphs)
		{
			shape->renderShapeGraph(viewportVpMatrix, graphicsContext);
		}
	}

	// Draw the floor grid using the configured extent / cell size
	const EditorSettings& editorSettings= getEditorSettings();
	const float gridExtent= (editorSettings.gridExtent > 0.f) ? editorSettings.gridExtent : 10.f;
	const float gridCellSize= (editorSettings.gridCellSize > 0.f) ? editorSettings.gridCellSize : 0.5f;
	int gridSubDiv= (int)((gridExtent / gridCellSize) + 0.5f);
	if (gridSubDiv < 1)
		gridSubDiv= 1;
	drawGrid(graphicsContext, glm::mat4(1.f), gridExtent, gridExtent, gridSubDiv, gridSubDiv, Colors::GhostWhite);

	// Translucent spot light cones last: they write no depth, so any opaque
	// geometry drawn after them (the environment probe sphere encloses the
	// whole scene in perspective) would paint over them
	if (m_viewMode == eProjectViewMode::scene)
	{
		if (auto spotLightSystem= m_spotLightSystem.lock())
			spotLightSystem->renderConeVolumes(graphicsContext, viewportCamera);
	}

	// The transform gizmo goes after every piece of scene geometry, so its
	// occluded pass can read the finished depth buffer. Both the scene and the
	// stage view can hold a selection (stage lights and cameras are pickable
	// in either).
	if (auto editorSystem= m_editorSystem.lock())
	{
		if (m_viewMode == eProjectViewMode::scene)
		{
			editorSystem->renderGizmo(graphicsContext, viewportCamera);
		}

		// Draw the orthographic ruler overlay (no-op unless measuring in an ortho view)
		editorSystem->renderRuler(graphicsContext, m_viewport);
	}

	if (editorSettings.bRenderOrigin)
	{
		debugRenderOrigin();
	}

	if (editorSettings.bDebugCameraAlignment)
	{
		renderCameraAlignmentDebug(graphicsContext, viewportCamera);
	}
}

SceneComponentConstPtr AppStage_Project::getCurrentSceneConst() const
{
	return getSystemOfTypeConst<const SceneObjectSystem>()->getCurrentScene();
}

StageComponentConstPtr AppStage_Project::getCurrentStageConst() const
{
	SceneComponentConstPtr currentScene= getCurrentSceneConst();
	if (currentScene)
	{
		return currentScene->getParentStage();
	}
	return nullptr;
}

TrackingVolumeComponentConstPtr AppStage_Project::getCurrentTrackingVolumeConst() const
{
	StageComponentConstPtr currentStage= getCurrentStageConst();
	if (currentStage)
	{
		return currentStage->getTrackingVolumeConst();
	}
	return nullptr;
}

void AppStage_Project::renderProjectScene(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera,
										  std::vector<ShapeComponentPtr>& outDeferredShapeGraphs) const
{
	SceneComponentConstPtr currentScene= getCurrentSceneConst();
	if (currentScene)
	{
		auto editorObjectSystem= getObjectSystemOfType<EditorObjectSystem>();
		const EditorSettings& editorSettings= editorObjectSystem->getEditorSettings();

		// Render the stage
		renderProjectStages(graphicsContext, viewportCamera);

		// Render anchors if enabled
		if (editorSettings.bDebugRenderAnchors)
		{
			AnchorObjectSystemPtr anchorSystem= m_anchorObjectSystem.lock();

			addAllRenderablesToMkScene(anchorSystem, m_mkScene);
			anchorSystem->customRender(graphicsContext, viewportCamera);
		}

		// Render the stencils if enabled
		if (editorSettings.bDebugRenderBoxStencils)
		{
			BoxStencilSystemPtr boxStencilSystem= m_boxStencilSystem.lock();

			addAllRenderablesToMkScene(boxStencilSystem, m_mkScene);
			boxStencilSystem->customRender(graphicsContext, viewportCamera);
		}
		if (editorSettings.bDebugRenderModelStencils)
		{
			ModelStencilSystemPtr modelStencilSystem= m_modelStencilSystem.lock();

			addAllRenderablesToMkScene(modelStencilSystem, m_mkScene);
			modelStencilSystem->customRender(graphicsContext, viewportCamera);
		}
		if (editorSettings.bDebugRenderQuadStencils)
		{
			QuadStencilSystemPtr quadStencilSystem= m_quadStencilSystem.lock();

			addAllRenderablesToMkScene(quadStencilSystem, m_mkScene);
			quadStencilSystem->customRender(graphicsContext, viewportCamera);
		}

		// Render the shapes if enabled
		if (editorSettings.bDebugRenderQuadShapes)
		{
			QuadShapeSystemPtr quadShapeSystem= m_quadShapeSystem.lock();

			std::vector<QuadShapeComponentPtr> quadShapes;
			quadShapeSystem->getQuadShapeComponentList(quadShapes);

			bool bAnyLegacyShapes= false;
			for (auto& shape : quadShapes)
			{
				if (shape->hasValidShapeGraph())
					outDeferredShapeGraphs.push_back(shape);
				else
					bAnyLegacyShapes= true;
			}
			if (bAnyLegacyShapes)
			{
				addAllRenderablesToMkScene(quadShapeSystem, m_mkScene);
				quadShapeSystem->customRender(graphicsContext, viewportCamera);
			}
		}
		if (editorSettings.bDebugRenderBoxShapes)
		{
			BoxShapeSystemPtr boxShapeSystem= m_boxShapeSystem.lock();

			std::vector<BoxShapeComponentPtr> boxShapes;
			boxShapeSystem->getBoxShapeComponentList(boxShapes);

			bool bAnyLegacyShapes= false;
			for (auto& shape : boxShapes)
			{
				if (shape->hasValidShapeGraph())
					outDeferredShapeGraphs.push_back(shape);
				else
					bAnyLegacyShapes= true;
			}
			if (bAnyLegacyShapes)
			{
				addAllRenderablesToMkScene(boxShapeSystem, m_mkScene);
				boxShapeSystem->customRender(graphicsContext, viewportCamera);
			}
		}
		if (editorSettings.bDebugRenderModelShapes)
		{
			ModelShapeSystemPtr modelShapeSystem= m_modelShapeSystem.lock();

			std::vector<ModelShapeComponentPtr> modelShapes;
			modelShapeSystem->getModelShapeComponentList(modelShapes);

			bool bAnyLegacyShapes= false;
			for (auto& shape : modelShapes)
			{
				if (shape->hasValidShapeGraph())
					outDeferredShapeGraphs.push_back(shape);
				else
					bAnyLegacyShapes= true;
			}
			if (bAnyLegacyShapes)
			{
				addAllRenderablesToMkScene(modelShapeSystem, m_mkScene);
				modelShapeSystem->customRender(graphicsContext, viewportCamera);
			}
		}
	}
}

void AppStage_Project::renderProjectStages(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	StageObjectSystemPtr stageObjectSystem= m_stageObjectSystem.lock();
	if (!stageObjectSystem)
		return;

	// The camera and light meshes belong to the project rather than to any one
	// stage, so they are submitted once no matter how many stages draw below
	addAllRenderablesToMkScene(m_cameraObjectSystem.lock(), m_mkScene);
	addAllRenderablesToMkScene(m_pixelGridLightSystem.lock(), m_mkScene);
	addAllRenderablesToMkScene(m_spotLightSystem.lock(), m_mkScene);

	// Light axes and labels; the additive cones come after the opaque pass
	if (auto spotLightSystem= m_spotLightSystem.lock())
		spotLightSystem->customRender(graphicsContext, viewportCamera);

	// Pixel grid boxes are opaque and write depth, so they belong here
	if (auto pixelGridSystem= m_pixelGridLightSystem.lock())
		pixelGridSystem->customRender(graphicsContext, viewportCamera);

	for (auto& [id, wp] : stageObjectSystem->getComponentMap())
	{
		StageComponentConstPtr stageComponent= wp.lock();
		if (stageComponent)
		{
			// Draw all the environment lights in the stage
			renderEnvironmentLightComponents(graphicsContext, viewportCamera, stageComponent);

			// Draw the cameras on the stage
			renderCameraComponents(graphicsContext, viewportCamera, stageComponent);

			// Draw the tracking volume for the stage
			renderProjectTracking(graphicsContext, viewportCamera, stageComponent);

			// Draw the state bounds
			stageComponent->renderStageBounds(graphicsContext, stageComponent->getWorldTransform());
		}
	}
}

void AppStage_Project::renderProjectTracking(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera,
											 StageComponentConstPtr stageComponent) const
{
	glm::mat4 stageToWorldSpace= stageComponent ? stageComponent->getWorldTransform() : glm::mat4(1.f);

	TrackingVolumeComponentConstPtr trackingVolume= getCurrentTrackingVolumeConst();
	if (trackingVolume)
	{
		switch (trackingVolume->getTrackingVolumeType())
		{
		case eTrackingVolumeType::vr:
			renderVRTrackingVolume(graphicsContext, viewportCamera,
								   std::dynamic_pointer_cast<const VRTrackingVolumeComponent>(trackingVolume),
								   stageToWorldSpace);
			break;
		case eTrackingVolumeType::marker:
			renderMarkerTrackingVolume(graphicsContext, viewportCamera,
									   std::dynamic_pointer_cast<const MarkerTrackingVolumeComponent>(trackingVolume),
									   stageToWorldSpace);
			break;
		}
	}
}

void AppStage_Project::renderVRTrackingVolume(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera,
											  VRTrackingVolumeComponentConstPtr vrTrackingVolume,
											  glm::mat4 stageToWorldSpace) const
{
	VRObjectSystemPtr vrObjectSystem= getObjectSystemOfType<VRObjectSystem>();

	// Resolve VRSpace -> StageSpace offset from the VR tracking volume
	glm::mat4 vrSpaceToStageSpace= vrTrackingVolume->getVRSpaceToStageSpace();
	glm::mat4 vrSpaceToWorldSpace= glm_composite_xform(vrSpaceToStageSpace, stageToWorldSpace);

	// Get the tracking volume origin marker (if it exists) so we can render it in the correct space
	const MikanMarkerID originMarkerId= vrTrackingVolume->getOriginMarkerId();
	MarkerComponentPtr markerComp;
	if (originMarkerId != INVALID_MIKAN_ID)
	{
		MarkerObjectSystemPtr markerSystem= getObjectSystemOfType<MarkerObjectSystem>();
		markerComp= markerSystem->getMarkerById(originMarkerId);
	}

	if (m_viewMode == eProjectViewMode::tracking)
	{

		switch (vrTrackingVolume->getDisplayTrackingSpace())
		{
		case MikanTrackingSpace_VR:
		{
			// Render the VR devices in VR space
			addAllVRDevicesToMkScene(vrObjectSystem, m_mkScene, glm::mat4(1.f));

			// Render the VR Device info in VR space
			renderAllVRDeviceInfo(vrObjectSystem, graphicsContext, viewportCamera, glm::mat4(1.f));

			// Render the origin marker as a textured quad at VRSpace Origin
			if (markerComp)
			{
				glm::mat4 stageSpaceToVRSpace= glm::inverse(vrSpaceToStageSpace);

				markerComp->renderArucoMarker(graphicsContext, viewportCamera, stageSpaceToVRSpace);
			}
		}
		break;
		case MikanTrackingSpace_Stage:
		{
			// Render the VR devices in stage space
			addAllVRDevicesToMkScene(vrObjectSystem, m_mkScene, vrSpaceToWorldSpace);

			// Render the VR Device info in stage space
			renderAllVRDeviceInfo(vrObjectSystem, graphicsContext, viewportCamera, vrSpaceToWorldSpace);

			// Render the origin marker as a textured quad at world origin
			if (markerComp)
			{
				markerComp->renderArucoMarker(graphicsContext, viewportCamera, stageToWorldSpace);
			}
		}
		break;
		}
	}
	else
	{
		// Render the VR devices in scene space
		addAllVRDevicesToMkScene(vrObjectSystem, m_mkScene, vrSpaceToWorldSpace);

		// Render the origin marker as a textured quad at world origin
		if (markerComp)
		{
			markerComp->renderArucoMarker(graphicsContext, viewportCamera, stageToWorldSpace);
		}
	}
}

void AppStage_Project::renderMarkerTrackingVolume(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera,
												  MarkerTrackingVolumeComponentConstPtr markerTrackingVolume,
												  glm::mat4 stageToWorldSpace) const
{
	const MikanMarkerID originMarkerId= markerTrackingVolume->getOriginMarkerId();
	if (originMarkerId != INVALID_MIKAN_ID)
	{
		MarkerObjectSystemPtr markerSystem= getObjectSystemOfType<MarkerObjectSystem>();
		MarkerComponentPtr markerComp= markerSystem->getMarkerById(originMarkerId);
		if (markerComp)
		{
			markerComp->renderArucoMarker(graphicsContext, viewportCamera, stageToWorldSpace);
		}
	}
}

void AppStage_Project::renderEnvironmentLightComponents(IMkGraphicsContext* graphicsContext,
														MikanCameraPtr viewportCamera,
														StageComponentConstPtr stageComponent) const
{
	if (viewportCamera->getProjectionMode() == eCameraProjectionMode::perspective)
	{
		getObjectSystemOfType<LightEnvironmentSystem>()->customRender(
			graphicsContext, viewportCamera, [stageComponent](LightEnvironmentComponentPtr lightEnv)
			{ return lightEnv->getOwnerStageComponent() == stageComponent; });
	}
}

void AppStage_Project::renderCameraComponents(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera,
											  StageComponentConstPtr stageComponent) const
{
	getObjectSystemOfType<CameraObjectSystem>()->customRender(
		graphicsContext, viewportCamera,
		[stageComponent](CameraComponentPtr camera) { return camera->getOwnerStageComponent() == stageComponent; });
}

void AppStage_Project::debugRenderOrigin() const
{
	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	TextStyle style= getDefaultTextStyle();

	drawTransformedAxes(graphicsContext, glm::mat4(1.f), 1.f, 1.f, 1.f);
	drawTextAtWorldPosition(graphicsContext, style, glm::vec3(0.f, 0.f, 0.f), L"(0,0,0)");
}

void AppStage_Project::renderCameraAlignmentDebug(IMkGraphicsContext* graphicsContext,
												  MikanCameraPtr viewportCamera) const
{
	TextStyle style= getDefaultTextStyle();

	// Stage origin (identity) — the frame every calibrated transform is expressed relative to
	drawTransformedAxes(graphicsContext, glm::mat4(1.f), 0.25f, true);
	drawTextAtWorldPosition(graphicsContext, style, glm::vec3(0.f), L"stage origin");

	// For each compositor on the current scene, draw its tracking puck + camera aperture in stage
	// space so the pose chain (VR->stage, puck->aperture lever arm) is directly inspectable in 3D.
	SceneComponentConstPtr currentScene= getCurrentSceneConst();
	if (!currentScene)
		return;

	for (const CompositorComponentPtr& compositor : currentScene->getOutputCompositors())
	{
		if (!compositor)
			continue;

		CameraComponentPtr cameraComponent= compositor->getCameraComponent();
		if (!cameraComponent)
			continue;

		// World space: these axes are drawn into the world-space project viewport
		glm::mat4 aperturePose(1.f);
		if (!cameraComponent->getWorldSpaceAperturePose(aperturePose))
			continue;

		const glm::vec3 aperturePos= glm::vec3(aperturePose[3]);
		drawTransformedAxes(graphicsContext, aperturePose, 0.1f, true);
		drawTextAtWorldPosition(graphicsContext, style, aperturePos, L"aperture");

		// aperturePose = puckStagePose * puckToApertureOffset  =>  puckStagePose = aperturePose * inverse(offset)
		glm::mat4 offsetXform(1.f);
		if (cameraComponent->hasValidApertureOffsetXform() && cameraComponent->getApertureOffsetXform(offsetXform))
		{
			const glm::mat4 puckPose= aperturePose * glm::inverse(offsetXform);
			const glm::vec3 puckPos= glm::vec3(puckPose[3]);

			drawTransformedAxes(graphicsContext, puckPose, 0.1f, true);
			drawTextAtWorldPosition(graphicsContext, style, puckPos, L"puck");

			// Lever arm from tracking puck to camera aperture
			drawSegment(graphicsContext, glm::mat4(1.f), puckPos, aperturePos, glm::vec3(1.f, 0.5f, 0.f));
		}
	}
}

void AppStage_Project::renderCameraAlignmentGui()
{
	const glm::i32vec2 vpOrigin= m_viewport ? m_viewport->getViewportOrigin() : glm::i32vec2(0, 45);
	ImGui::SetNextWindowPos(
		ImVec2((float)vpOrigin.x + 8 * MkGui::getUiScale(), (float)vpOrigin.y + 44 * MkGui::getUiScale()),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(340.f * MkGui::getUiScale(), 0.f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(locWindowTitle("windows.cameraAlignmentDebug")))
	{
		const ImVec4 k_warnColor(1.f, 0.3f, 0.3f, 1.f);
		int compositorCount= 0;

		SceneComponentConstPtr currentScene= getCurrentSceneConst();
		const std::vector<CompositorComponentPtr> compositors=
			currentScene ? currentScene->getOutputCompositors() : std::vector<CompositorComponentPtr>();

		for (const CompositorComponentPtr& compositor : compositors)
		{
			if (!compositor)
				continue;

			CameraComponentPtr cameraComponent= compositor->getCameraComponent();
			VideoSourceComponentPtr videoSource= compositor->getVideoSourceComponent();
			if (!cameraComponent || !videoSource)
				continue;

			ImGui::PushID(compositorCount++);
			ImGui::Separator();
			ImGui::Text("%s", compositor->getName().c_str());

			// Resolution: live video mode vs the resolution the intrinsics were calibrated at.
			// A mismatch silently invalidates the projection (no rescaling is done anywhere).
			int liveWidth= 0, liveHeight= 0;
			videoSource->getVideoPixelDimensions(liveWidth, liveHeight);

			MikanVideoSourceIntrinsics intrinsics;
			videoSource->getCameraIntrinsics(intrinsics);
			if (intrinsics.intrinsics_type == MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
			{
				const MikanMonoIntrinsics& mono= intrinsics.getMonoIntrinsics();
				const int calibWidth= (int)mono.pixel_width;
				const int calibHeight= (int)mono.pixel_height;

				ImGui::Text(locText("project.liveResolutionFmt"), liveWidth, liveHeight);
				ImGui::Text(locText("project.calibResolutionFmt"), calibWidth, calibHeight);
				if (liveWidth != calibWidth || liveHeight != calibHeight)
					ImGui::TextColored(k_warnColor, "%s", locText("project.resolutionMismatch"));

				float fx= 0.f, fy= 0.f, cx= 0.f, cy= 0.f, skew= 0.f;
				extractCameraIntrinsicMatrixParameters(mono.undistorted_camera_matrix, fx, fy, cx, cy, skew);
				ImGui::Text(locText("project.fxFyFmt"), fx, fy);
				ImGui::Text(locText("project.cxCyFmt"), cx, cy);
				ImGui::Text(locText("project.hfovVfovFmt"), (float)mono.hfov, (float)mono.vfov);
				ImGui::Text(locText("project.znearZfarFmt"), (float)mono.znear, (float)mono.zfar);
			}
			else
			{
				ImGui::TextColored(k_warnColor, "%s", locText("project.noMonoIntrinsics"));
			}

			// Tracking + calibrated offsets
			CameraDefinitionPtr cameraDef= cameraComponent->getCameraDefinition();
			ImGui::Text(locText("project.trackingFrameDelayFmt"), cameraDef->getTrackingFrameDelay());
			ImGui::Text(locText("project.puckPoseValidFmt"), cameraComponent->hasValidTrackingMountPoseView()
																 ? locText("project.yes")
																 : locText("project.no"));

			const bool offsetValid= cameraDef->hasValidApertureOffset();
			ImGui::Text(locText("project.apertureOffsetValidFmt"),
						offsetValid ? locText("project.yes") : locText("project.no"));
			if (offsetValid)
			{
				const MikanVector3d p= cameraDef->getApertureOffsetPosition();
				const MikanQuatd q= cameraDef->getApertureOffsetOrientation();
				ImGui::Text(locText("project.aperturePosFmt"), p.x * 1000.0, p.y * 1000.0, p.z * 1000.0);
				ImGui::Text(locText("project.apertureQuatFmt"), q.w, q.x, q.y, q.z);
			}

			ImGui::PopID();
		}

		if (compositorCount == 0)
			ImGui::TextDisabled("%s", locText("project.noActiveCompositorCameras"));
	}
	ImGui::End();
}

// -- IRemoteControllable Interface -- //
bool AppStage_Project::handleRemoteControlCommand(const std::string& command,
												  const std::vector<std::string>& parameters,
												  std::vector<std::string>& outResults)
{
	if (command == "add_new_script")
	{
		// TODO: Implement script removal - need to determine component ID parameter handling
		return true;
	}
	else if (command == "remove_script")
	{
		// TODO: Implement script removal - need to determine component ID parameter handling
		return true;
	}
	else if (command == "select_asset")
	{
		if (parameters.size() < 2)
			return false;

		return m_assetsPanel != nullptr && m_assetsPanel->selectEntry(parameters[0], parameters[1]);
	}
	else if (command == "selected_asset")
	{
		if (m_assetsPanel != nullptr)
		{
			if (const ProjectAssetEntry* entry= m_assetsPanel->getSelectedEntry())
			{
				outResults.push_back(entry->folderId);
				outResults.push_back(entry->storedPath);
			}
		}

		return true;
	}
	else if (command == "current_asset_folder")
	{
		if (m_assetsPanel != nullptr)
		{
			outResults.push_back(m_assetsPanel->getCurrentFolderId());
		}

		return true;
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}