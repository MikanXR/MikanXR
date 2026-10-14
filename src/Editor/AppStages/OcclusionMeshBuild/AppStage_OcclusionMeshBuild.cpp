#include "OcclusionMeshBuild/AppStage_OcclusionMeshBuild.h"
#include "OcclusionMeshBuild/GuiPanel_OcclusionMeshBuild.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"

#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "DepthTrackReader.h"
#include "FileVideoSourceComponent.h"
#include "GlbMeshWriter.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "IMkViewport.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanCamera.h"
#include "MikanViewport.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"
#include "ProjectManager.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "StageComponent.h"
#include "Transform.h"
#include "TransformComponent.h"

#include "glm/glm.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <numeric>

const char* AppStage_OcclusionMeshBuild::APP_STAGE_NAME= "OcclusionMeshBuild";

namespace OcclusionMeshBuildStage
{
const char* k_refinedTrackSuffix= ".refined.pose.json";

// A take's refined track sits beside the media with the media's stem, like
// the recorded track, so the depth sidecars still resolve from the media
std::filesystem::path refinedTrackPathForMedia(const std::filesystem::path& mediaPath)
{
	return mediaPath.parent_path() / (mediaPath.stem().string() + k_refinedTrackSuffix);
}

// A take recorded at the finer preset resolves small objects at arm's
// length; the coarser one is what a room sweep needs
void applyVoxelPreset(eOcclusionMeshVoxelPreset preset, OcclusionMeshBuildSettings& settings)
{
	switch (preset)
	{
	case eOcclusionMeshVoxelPreset::tabletop:
		settings.volume.voxelSize= 0.008f;
		settings.volume.truncation= 0.032f;
		settings.minComponentTriangles= 1500;
		break;
	case eOcclusionMeshVoxelPreset::room:
	default:
		settings.volume.voxelSize= 0.015f;
		settings.volume.truncation= 0.06f;
		settings.minComponentTriangles= 500;
		break;
	}
}

bool parseBool(const std::string& text, bool& outValue)
{
	if (text == "true" || text == "1" || text == "on")
	{
		outValue= true;
		return true;
	}
	if (text == "false" || text == "0" || text == "off")
	{
		outValue= false;
		return true;
	}
	return false;
}
} // namespace OcclusionMeshBuildStage

// -- BuildObserver -----
bool AppStage_OcclusionMeshBuild::BuildObserver::isCancelRequested() const
{
	return m_stage->m_bCancelRequested.load();
}

void AppStage_OcclusionMeshBuild::BuildObserver::onProgress(eOcclusionMeshBuildPhase phase, int done, int total)
{
	m_stage->m_buildPhase= (int)phase;
	m_stage->m_framesDone= done;
	m_stage->m_frameCount= total;
}

// -- AppStage_OcclusionMeshBuild -----
AppStage_OcclusionMeshBuild::AppStage_OcclusionMeshBuild(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, APP_STAGE_NAME)
{
}

AppStage_OcclusionMeshBuild::~AppStage_OcclusionMeshBuild() {}

bool AppStage_OcclusionMeshBuild::tryEnterBuild(AppStage* fromAppStage, FileVideoSourceComponentPtr videoSource)
{
	if (!fromAppStage || !videoSource)
		return false;

	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();
	FileVideoSourceDefinitionPtr definition= videoSource->getFileVideoSourceDefinition();

	if (PathUtils::getProjectDirectory().empty())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.noProjectLoaded"));
		return false;
	}

	const std::filesystem::path mediaPath= PathUtils::resolveProjectResource(definition->getMediaPath());
	if (mediaPath.empty())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.noMediaPath"));
		return false;
	}

	const std::filesystem::path trackPath= PathUtils::resolveProjectResource(definition->getPoseTrackPath());
	PoseTrack track;
	if (trackPath.empty() || !track.loadFromFile(trackPath))
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.poseTrackMissing"));
		return false;
	}

	if (!track.hasDepthTrack())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.noDepthTrack"));
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(DepthTrackReader::depthSidecarPathForMedia(mediaPath), ec)
		|| !std::filesystem::is_regular_file(DepthTrackReader::confidenceSidecarPathForMedia(mediaPath), ec))
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.depthSidecarsMissing"));
		return false;
	}

	if (!definition->hasPoseOffset())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.notAligned"));
		return false;
	}

	auto cameraSystem= ownerWindow->getProjectManager()->getSystemOfType<CameraObjectSystem>();
	CameraComponentPtr camera=
		cameraSystem ? cameraSystem->findCameraForVideoSource(videoSource->getVideoSourceId()) : CameraComponentPtr();
	StageComponentConstPtr stage= camera ? camera->getOwnerStageComponent() : StageComponentConstPtr();
	if (!camera || !stage)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.noCamera"));
		return false;
	}

	auto* buildStage= ownerWindow->pushAppStageOfType<AppStage_OcclusionMeshBuild>();
	buildStage->setSourceVideoSource(videoSource, camera, stage, track);
	return true;
}

void AppStage_OcclusionMeshBuild::setSourceVideoSource(FileVideoSourceComponentPtr videoSource,
													   CameraComponentPtr camera, StageComponentConstPtr stage,
													   const PoseTrack& track)
{
	m_videoSource= videoSource;
	m_camera= camera;
	m_stage= stage;
	m_track= track;
}

void AppStage_OcclusionMeshBuild::enter()
{
	AppStage::enter();

	// The preview is the fused mesh alone, so the viewport camera orbits it
	// rather than looking through the take's camera
	m_mkCamera= getFirstViewport()->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::orbit);

	m_buildPanel= addGuiPanel<GuiPanel_OcclusionMeshBuild>();
	m_buildPanel->OnBuildEvent= [this]() { onBuildEvent(); };
	m_buildPanel->OnCancelBuildEvent= [this]() { onCancelBuildEvent(); };
	m_buildPanel->OnApplyEvent= [this]() { onApplyEvent(); };
	m_buildPanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_buildPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_buildPanel->OnOkEvent= [this]() { onCancelEvent(); };

	const size_t frameCount= m_track.getFrameCount();
	const float durationSeconds=
		frameCount > 1 ? (float)(m_track.getFrame(frameCount - 1).timeUs - m_track.getFrame(0).timeUs) * 1e-6f : 0.f;
	m_buildPanel->setTakeSummary((int)frameCount, m_track.getDepthWidth(), m_track.getDepthHeight(), durationSeconds);

	m_bHasResult= false;
	startBuildWorker();

	setMenuState(eOcclusionMeshBuildMenuState::verifyTake);
}

void AppStage_OcclusionMeshBuild::exit()
{
	setMenuState(eOcclusionMeshBuildMenuState::inactive);

	stopBuildWorker();

	m_videoSource= nullptr;
	m_camera= nullptr;
	m_stage= nullptr;
	m_mkCamera= nullptr;

	AppStage::exit();
}

void AppStage_OcclusionMeshBuild::setMenuState(eOcclusionMeshBuildMenuState newState)
{
	if (m_buildPanel != nullptr)
		m_buildPanel->setMenuState(newState);
}

OcclusionMeshBuildSettings AppStage_OcclusionMeshBuild::gatherSettings() const
{
	OcclusionMeshBuildSettings settings;
	OcclusionMeshBuildStage::applyVoxelPreset(m_buildPanel->getVoxelPreset(), settings);
	settings.keyframeStride= std::max(m_buildPanel->getKeyframeStride(), 1);
	settings.bFrameToModel= m_buildPanel->getFrameToModel();
	return settings;
}

void AppStage_OcclusionMeshBuild::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	if (m_buildPanel->getMenuState() != eOcclusionMeshBuildMenuState::building)
		return;

	m_buildElapsedSeconds+= deltaSeconds;
	m_buildPanel->setBuildProgress((eOcclusionMeshBuildPhase)m_buildPhase.load(), m_framesDone.load(),
								   m_frameCount.load(), m_buildElapsedSeconds, m_bCancelRequested.load());

	bool bBuildFinished= false;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		bBuildFinished= m_bBuildFinished;
	}

	if (bBuildFinished)
		consumeBuildOutput();
}

void AppStage_OcclusionMeshBuild::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##OcclusionMeshBuild", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

// -- build worker -----
void AppStage_OcclusionMeshBuild::onBuildEvent()
{
	if (m_buildPanel->getMenuState() != eOcclusionMeshBuildMenuState::verifyTake)
		return;

	const std::filesystem::path mediaPath=
		PathUtils::resolveProjectResource(m_videoSource->getFileVideoSourceDefinition()->getMediaPath());

	BuildRequest request;
	request.track= m_track;
	request.depthPath= DepthTrackReader::depthSidecarPathForMedia(mediaPath);
	request.confidencePath= DepthTrackReader::confidenceSidecarPathForMedia(mediaPath);
	request.settings= gatherSettings();

	m_bCancelRequested= false;
	m_buildPhase= (int)eOcclusionMeshBuildPhase::loadingTake;
	m_framesDone= 0;
	m_frameCount= 0;
	m_buildElapsedSeconds= 0.f;
	m_bLastBuildRegistered= request.settings.bFrameToModel;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_pendingRequest= std::move(request);
		m_bBuildRequested= true;
		m_bBuildFinished= false;
	}
	m_workerSignal.notify_one();

	setMenuState(eOcclusionMeshBuildMenuState::building);
}

void AppStage_OcclusionMeshBuild::onCancelBuildEvent() { m_bCancelRequested= true; }

void AppStage_OcclusionMeshBuild::startBuildWorker()
{
	if (m_workerThread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_bWorkerShutdownRequested= false;
		m_bBuildRequested= false;
		m_bBuildFinished= false;
	}

	m_workerThread= std::thread([this]() { buildWorkerMain(); });
}

void AppStage_OcclusionMeshBuild::stopBuildWorker()
{
	if (!m_workerThread.joinable())
		return;

	// The builder polls the cancel flag between frames, so the join waits
	// out at most one frame's work
	m_bCancelRequested= true;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_bWorkerShutdownRequested= true;
	}
	m_workerSignal.notify_one();

	m_workerThread.join();
}

void AppStage_OcclusionMeshBuild::buildWorkerMain()
{
	for (;;)
	{
		BuildRequest request;
		{
			std::unique_lock<std::mutex> lock(m_workerMutex);
			m_workerSignal.wait(lock, [this]() { return m_bWorkerShutdownRequested || m_bBuildRequested; });

			if (m_bWorkerShutdownRequested)
				return;

			request= std::move(m_pendingRequest);
			m_bBuildRequested= false;
		}

		BuildOutput output;
		runBuildRequest(request, output);

		{
			std::lock_guard<std::mutex> lock(m_workerMutex);
			m_buildOutput= std::move(output);
			m_bBuildFinished= true;
		}
	}
}

void AppStage_OcclusionMeshBuild::runBuildRequest(const BuildRequest& request, BuildOutput& outOutput)
{
	const PoseTrack& track= request.track;

	DepthTrackReader reader;
	if (!reader.open(request.depthPath, request.confidencePath, track.getDepthWidth(), track.getDepthHeight(),
					 track.getDepthUnitMm(), track.getFrameCount()))
	{
		outOutput.result.failureReason= locText("occlusionMeshBuild.depthSidecarsUnreadable");
		return;
	}

	BuildObserver observer(this);
	OcclusionMeshBuilder::build(track, reader, request.settings, &observer, outOutput.result);
}

void AppStage_OcclusionMeshBuild::consumeBuildOutput()
{
	BuildOutput output;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		output= std::move(m_buildOutput);
		m_buildOutput= BuildOutput();
		m_bBuildFinished= false;
	}

	m_buildPhase= (int)eOcclusionMeshBuildPhase::idle;

	if (output.result.bCancelled)
	{
		MIKAN_LOG_INFO("AppStage_OcclusionMeshBuild") << "Build cancelled";
		setMenuState(eOcclusionMeshBuildMenuState::verifyTake);
		return;
	}

	if (!output.result.bSucceeded)
	{
		m_buildPanel->setFailureReason(output.result.failureReason);
		setMenuState(eOcclusionMeshBuildMenuState::failed);
		return;
	}

	m_result= std::move(output.result);
	m_bHasResult= true;

	m_buildPanel->setBuildSummary(m_result.stats, m_result.bHasRefinedPoses);
	setMenuState(eOcclusionMeshBuildMenuState::verifyMesh);
}

// -- apply -----
bool AppStage_OcclusionMeshBuild::createStencilFromMesh()
{
	if (!m_bHasResult)
		return false;

	const std::filesystem::path projectDirectory= PathUtils::getProjectDirectory();
	if (projectDirectory.empty())
	{
		m_buildPanel->setFailureReason(locText("occlusionMeshBuild.noProjectLoaded"));
		setMenuState(eOcclusionMeshBuildMenuState::failed);
		return false;
	}

	// A stencil only accepts a scene as its parent, so there is no fallback
	// when the project has no active scene
	auto sceneSystem= getSystemOfType<SceneObjectSystem>();
	SceneComponentPtr currentScene= sceneSystem ? sceneSystem->getCurrentScene() : nullptr;
	if (!currentScene)
	{
		m_buildPanel->setFailureReason(locText("occlusionMeshBuild.noActiveScene"));
		setMenuState(eOcclusionMeshBuildMenuState::failed);
		return false;
	}

	// Unique per build: the model resource cache keys on the file path
	std::time_t now= std::time(nullptr);
	std::tm timeInfo;
	localtime_s(&timeInfo, &now);
	char stencilName[64];
	std::strftime(stencilName, sizeof(stencilName), "OcclusionMesh_%Y%m%d_%H%M%S", &timeInfo);

	const std::filesystem::path modelsDirectory= projectDirectory / "models";
	std::filesystem::create_directories(modelsDirectory);
	const std::filesystem::path meshPath= modelsDirectory / (std::string(stencilName) + ".glb");

	if (!GlbMeshWriter::write(meshPath, stencilName, m_result.mesh.vertices, m_result.mesh.normals, {},
							  m_result.mesh.indices))
	{
		m_buildPanel->setFailureReason(locFormat("occlusionMeshBuild.meshWriteFailedFmt", meshPath.string().c_str()));
		setMenuState(eOcclusionMeshBuildMenuState::failed);
		return false;
	}

	const MikanTransformID parentTransformId= currentScene->getSceneId();
	auto modelStencilSystem= getSystemOfType<ModelStencilSystem>();
	ModelStencilComponentPtr stencilComponent= modelStencilSystem->addNewObjectByTypedDefinition(
		[&](ModelStencilDefinitionPtr definition)
		{
			definition->setComponentName(stencilName);
			definition->setParentTransformId(parentTransformId);
			definition->setRelativeTransform(GlmTransform());
			definition->setIsDisabled(false);
			definition->setModelPath(meshPath);
			return true;
		});
	if (!stencilComponent)
	{
		m_buildPanel->setFailureReason(locText("occlusionMeshBuild.stencilCreateFailed"));
		setMenuState(eOcclusionMeshBuildMenuState::failed);
		return false;
	}

	// The vertices are in the take's ARKit world space. The same offset that
	// carries a recorded camera pose into the stage carries the whole mesh,
	// and the stage's own transform takes it on into the world.
	const glm::mat4 poseOffset= m_videoSource->getFileVideoSourceDefinition()->getPoseOffset();
	stencilComponent->setWorldTransform(m_stage->getWorldTransform() * poseOffset);

	std::string refinedTrackStoredPath;
	bool bReplacedSourceTrack= false;
	if (m_result.bHasRefinedPoses)
	{
		if (!writeRefinedPoseTrack(refinedTrackStoredPath, bReplacedSourceTrack))
		{
			// The stencil stands; only the track is missing
			MIKAN_LOG_WARNING("AppStage_OcclusionMeshBuild") << "The refined pose track was not written";
		}
	}

	if (ProjectAssetCatalog* catalog= getOwnerWindow()->getAssetCatalog())
		catalog->refresh();

	m_buildPanel->setCreatedStencilName(stencilName);
	m_buildPanel->setRefinedTrackPath(refinedTrackStoredPath, bReplacedSourceTrack);

	MIKAN_LOG_INFO("AppStage_OcclusionMeshBuild")
		<< "Created occlusion mesh stencil '" << stencilName << "' (" << m_result.mesh.getTriangleCount()
		<< " triangles, " << meshPath.string() << ")";

	return true;
}

bool AppStage_OcclusionMeshBuild::writeRefinedPoseTrack(std::string& outStoredPath, bool& outReplacedSourceTrack)
{
	outStoredPath.clear();
	outReplacedSourceTrack= false;

	FileVideoSourceDefinitionPtr definition= m_videoSource->getFileVideoSourceDefinition();
	const std::filesystem::path mediaPath= PathUtils::resolveProjectResource(definition->getMediaPath());
	if (mediaPath.empty())
		return false;

	// Frames go out in file order so record i of the sidecars still
	// describes frame i of the refined track
	std::vector<size_t> trackOrder(m_track.getFrameCount());
	std::iota(trackOrder.begin(), trackOrder.end(), (size_t)0);
	std::sort(trackOrder.begin(), trackOrder.end(),
			  [this](size_t a, size_t b) { return m_track.getFrame(a).fileIndex < m_track.getFrame(b).fileIndex; });

	PoseTrackWriter writer(m_track.getSessionId(), m_track.getImageWidth(), m_track.getImageHeight());
	writer.setDepthTrack(m_track.getDepthWidth(), m_track.getDepthHeight(), m_track.getDepthUnitMm());
	for (size_t trackIndex : trackOrder)
	{
		const PoseTrackFrame& frame= m_track.getFrame(trackIndex);
		const glm::mat4& cameraToWorld= frame.fileIndex < m_result.refinedCameraToWorld.size()
											? m_result.refinedCameraToWorld[frame.fileIndex]
											: frame.cameraToWorld;
		writer.append(frame.timeUs, frame.captureTimestampUs, cameraToWorld, frame.fx, frame.fy, frame.cx, frame.cy);
	}

	const std::filesystem::path refinedPath= OcclusionMeshBuildStage::refinedTrackPathForMedia(mediaPath);
	if (!writer.writeToFile(refinedPath))
	{
		m_buildPanel->setFailureReason(
			locFormat("occlusionMeshBuild.refinedTrackWriteFailedFmt", refinedPath.string().c_str()));
		return false;
	}

	outStoredPath= PathUtils::makeStoredProjectPath(refinedPath);
	if (m_buildPanel->getReplacePoseTrack())
	{
		definition->setPoseTrackPath(refinedPath);
		outReplacedSourceTrack= true;
	}

	MIKAN_LOG_INFO("AppStage_OcclusionMeshBuild") << "Wrote refined pose track " << refinedPath.string()
												  << (outReplacedSourceTrack ? " (now the source's track)" : "");
	return true;
}

void AppStage_OcclusionMeshBuild::onApplyEvent()
{
	if (createStencilFromMesh())
		setMenuState(eOcclusionMeshBuildMenuState::buildComplete);
}

void AppStage_OcclusionMeshBuild::onRedoEvent()
{
	m_bHasResult= false;
	setMenuState(eOcclusionMeshBuildMenuState::verifyTake);
}

void AppStage_OcclusionMeshBuild::onCancelEvent() { getOwnerWindow()->popAppState(); }

// -- render -----
void AppStage_OcclusionMeshBuild::renderMeshPreview()
{
	if (!m_bHasResult)
		return;

	IMkLineRenderer* lineRenderer= getGraphicsContext()->getLineRenderer();
	if (lineRenderer == nullptr)
		return;

	// Drawn where the stencil will land, as a point sample dense enough to
	// read the room but bounded so the preview stays interactive
	const glm::mat4 poseOffset= m_videoSource->getFileVideoSourceDefinition()->getPoseOffset();
	const glm::mat4 meshToWorld= m_stage->getWorldTransform() * poseOffset;
	constexpr size_t k_maxPreviewPoints= 60000;
	const size_t vertexCount= m_result.mesh.vertices.size();
	const size_t stride= std::max<size_t>(1, vertexCount / k_maxPreviewPoints);

	for (size_t vertexIndex= 0; vertexIndex < vertexCount; vertexIndex+= stride)
	{
		const glm::vec3& normal=
			vertexIndex < m_result.mesh.normals.size() ? m_result.mesh.normals[vertexIndex] : glm::vec3(0.f, 1.f, 0.f);
		// Shaded by the normal so surfaces read as surfaces
		const glm::vec3 color= glm::abs(normal) * 0.6f + glm::vec3(0.3f);
		lineRenderer->addPoint3d(meshToWorld, m_result.mesh.vertices[vertexIndex], color, 2.f);
	}
}

void AppStage_OcclusionMeshBuild::render(IMkViewportPtr targetViewport)
{
	switch (m_buildPanel->getMenuState())
	{
	case eOcclusionMeshBuildMenuState::verifyMesh:
	case eOcclusionMeshBuildMenuState::buildComplete:
		renderMeshPreview();
		break;

	default:
		break;
	}

	getGraphicsContext()->getLineRenderer()->render(true);
}

// -- remote control -----
bool AppStage_OcclusionMeshBuild::handleRemoteControlCommand(const std::string& command,
															 const std::vector<std::string>& parameters,
															 std::vector<std::string>& outResults)
{
	const eOcclusionMeshBuildMenuState menuState= m_buildPanel->getMenuState();

	if (command == "get_state")
	{
		outResults.push_back(k_OcclusionMeshBuildMenuStateStrings[(int)menuState]);
		return true;
	}
	else if (command == "get_take_info")
	{
		outResults.push_back("frames " + std::to_string(m_track.getFrameCount()));
		outResults.push_back("depth_width " + std::to_string(m_track.getDepthWidth()));
		outResults.push_back("depth_height " + std::to_string(m_track.getDepthHeight()));
		outResults.push_back("session_id " + m_track.getSessionId());
		return true;
	}
	else if (command == "set_option")
	{
		if (parameters.size() != 2)
		{
			outResults.push_back(
				"usage: set_option <voxel_preset|keyframe_stride|frame_to_model|replace_pose_track> <value>");
			return false;
		}
		if (menuState != eOcclusionMeshBuildMenuState::verifyTake)
		{
			outResults.push_back("options can only change before a build");
			return false;
		}

		const std::string& option= parameters[0];
		const std::string& value= parameters[1];
		bool bFlag= false;
		if (option == "voxel_preset")
		{
			for (int presetIndex= 0; presetIndex < (int)eOcclusionMeshVoxelPreset::COUNT; ++presetIndex)
			{
				if (k_OcclusionMeshVoxelPresetStrings[presetIndex] == value)
				{
					m_buildPanel->setVoxelPreset((eOcclusionMeshVoxelPreset)presetIndex);
					return true;
				}
			}
			outResults.push_back("unknown voxel preset " + value);
			return false;
		}
		else if (option == "keyframe_stride")
		{
			const int stride= std::atoi(value.c_str());
			if (stride < 1)
			{
				outResults.push_back("keyframe_stride must be at least 1");
				return false;
			}
			m_buildPanel->setKeyframeStride(stride);
			return true;
		}
		else if (option == "frame_to_model" && OcclusionMeshBuildStage::parseBool(value, bFlag))
		{
			m_buildPanel->setFrameToModel(bFlag);
			return true;
		}
		else if (option == "replace_pose_track" && OcclusionMeshBuildStage::parseBool(value, bFlag))
		{
			m_buildPanel->setReplacePoseTrack(bFlag);
			return true;
		}

		outResults.push_back("unknown option or value: " + option + " " + value);
		return false;
	}
	else if (command == "build")
	{
		if (menuState != eOcclusionMeshBuildMenuState::verifyTake)
		{
			outResults.push_back("build needs the verifyTake state");
			return false;
		}
		onBuildEvent();
		return true;
	}
	else if (command == "get_progress")
	{
		outResults.push_back("phase " + std::to_string(m_buildPhase.load()));
		outResults.push_back("frames " + std::to_string(m_framesDone.load()));
		outResults.push_back("total " + std::to_string(m_frameCount.load()));
		outResults.push_back("elapsed " + std::to_string(m_buildElapsedSeconds));
		return true;
	}
	else if (command == "get_summary")
	{
		if (!m_bHasResult)
		{
			outResults.push_back("no result");
			return false;
		}
		const OcclusionMeshBuildStats& stats= m_result.stats;
		outResults.push_back("triangles " + std::to_string(stats.trianglesClean));
		outResults.push_back("triangles_raw " + std::to_string(stats.trianglesRaw));
		outResults.push_back("islands_removed " + std::to_string(stats.trianglesRemovedAsIslands));
		outResults.push_back("frames_integrated " + std::to_string(stats.framesIntegrated));
		outResults.push_back("keyframes " + std::to_string(stats.keyframes));
		outResults.push_back("voxel_size " + std::to_string(stats.voxelSize));
		outResults.push_back("grid " + std::to_string(stats.gridDims.x) + " " + std::to_string(stats.gridDims.y) + " "
							 + std::to_string(stats.gridDims.z));
		outResults.push_back("registrations_accepted " + std::to_string(stats.registrationsAccepted));
		outResults.push_back("registrations_rejected " + std::to_string(stats.registrationsRejected));
		outResults.push_back("final_correction_mm " + std::to_string(stats.finalCorrectionMm));
		outResults.push_back("final_correction_deg " + std::to_string(stats.finalCorrectionDeg));
		outResults.push_back("seconds " + std::to_string(stats.totalSeconds));
		return true;
	}
	else if (command == "apply")
	{
		if (menuState != eOcclusionMeshBuildMenuState::verifyMesh)
		{
			outResults.push_back("apply needs the verifyMesh state");
			return false;
		}
		onApplyEvent();
		outResults.push_back(m_buildPanel->getMenuState() == eOcclusionMeshBuildMenuState::buildComplete ? "created"
																										 : "failed");
		return true;
	}
	else if (command == "cancel")
	{
		if (menuState == eOcclusionMeshBuildMenuState::building)
			onCancelBuildEvent();
		else
			onRedoEvent();
		return true;
	}
	else if (command == "return")
	{
		onCancelEvent();
		return true;
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}
