#include "OcclusionMeshBuild/AppStage_OcclusionMeshBuild.h"
#include "OcclusionMeshBuild/GuiPanel_OcclusionMeshBuild.h"
#include "OcclusionMeshBuild/OcclusionMeshPreview.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"

#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "Capture.h"
#include "DepthTrackReader.h"
#include "FileVideoSourceComponent.h"
#include "FileVideoSourceSystem.h"
#include "GlbMeshWriter.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "IMkViewport.h"
#include "LocText.h"
#include "Logger.h"
#include "MathGLM.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanViewport.h"
#include "MkWindowEvent.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "MovieDecoder.h"
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

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <limits>
#include <numeric>

const char* AppStage_OcclusionMeshBuild::APP_STAGE_NAME= "OcclusionMeshBuild";

namespace OcclusionMeshBuildStage
{
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

// A dragged face stops this far short of the opposite face
const float k_minBoxExtent= 0.05f;
// The framed view never looks flatter than this, so the tops of things read
const float k_minFramePitchDegrees= 20.f;

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
	, m_preview(std::make_unique<OcclusionMeshPreview>())
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

	// Only a trackcamera carries a per-frame track and the depth it describes
	Capture capture;
	std::string captureError;
	if (CaptureKindUtils::kindFromExtension(mediaPath) != eCaptureKind::trackcamera
		|| !capture.open(mediaPath, captureError))
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.notTrackCamera"));
		return false;
	}

	// The build fuses from the track the source plays: the override, when one
	// was set by hand, or the capture's own track. An override from
	// another session is a leftover of a different take and is skipped the
	// way playback skips it, since its frames index nothing in this capture.
	PoseTrack track;
	const std::filesystem::path trackPath= PathUtils::resolveProjectResource(definition->getPoseTrackPath());
	if (!definition->getPoseTrackPath().empty())
	{
		if (trackPath.empty() || !track.loadFromFile(trackPath))
		{
			ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.poseTrackMissing"));
			return false;
		}
		if (track.getSessionId() != capture.getManifest().sessionId)
		{
			MIKAN_LOG_WARNING("AppStage_OcclusionMeshBuild")
				<< "The pose track override " << trackPath << " was recorded in session " << track.getSessionId()
				<< ", not the capture's " << capture.getManifest().sessionId << ", so the capture's own track is used";
			track.clear();
		}
	}
	if (!track.isLoaded())
	{
		track= capture.getPoseTrack(eCaptureMemberRole::movie_pose);
	}

	const CaptureArchiveMember* depthMember= capture.findMember(eCaptureMemberRole::depth);
	const CaptureArchiveMember* confidenceMember= capture.findMember(eCaptureMemberRole::confidence);
	const CaptureArchiveMember* movieMember= capture.findMember(eCaptureMemberRole::movie);
	if (!track.hasDepthTrack() || depthMember == nullptr || confidenceMember == nullptr || movieMember == nullptr)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMeshBuild.noDepthTrack"));
		return false;
	}

	if (!videoSource->hasPoseOffset())
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
	buildStage->setSourceVideoSource(videoSource, camera, stage, track, mediaPath, *depthMember, *confidenceMember,
									 *movieMember);
	return true;
}

void AppStage_OcclusionMeshBuild::setSourceVideoSource(FileVideoSourceComponentPtr videoSource,
													   CameraComponentPtr camera, StageComponentConstPtr stage,
													   const PoseTrack& track, const std::filesystem::path& capturePath,
													   const CaptureArchiveMember& depthMember,
													   const CaptureArchiveMember& confidenceMember,
													   const CaptureArchiveMember& movieMember)
{
	m_videoSource= videoSource;
	m_camera= camera;
	m_stage= stage;
	m_track= track;
	m_capturePath= capturePath;
	m_depthMember= depthMember;
	m_confidenceMember= confidenceMember;
	m_movieMember= movieMember;
}

void AppStage_OcclusionMeshBuild::enter()
{
	AppStage::enter();

	// The preview is the fused mesh alone, so the viewport camera orbits it
	// rather than looking through the take's camera
	MikanViewportPtr viewport= getFirstViewport();
	m_mkCamera= viewport->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::orbit);

	// The left button drags the bounds box; the right orbits the view
	viewport->OnMouseRayChanged+= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayChanged);
	viewport->OnMouseRayButtonDown+= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayButtonDown);
	viewport->OnMouseRayButtonUp+= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayButtonUp);

	m_buildPanel= addGuiPanel<GuiPanel_OcclusionMeshBuild>();
	m_buildPanel->OnBuildEvent= [this]() { onBuildEvent(); };
	m_buildPanel->OnCancelBuildEvent= [this]() { onCancelBuildEvent(); };
	m_buildPanel->OnApplyEvent= [this]() { onApplyEvent(); };
	m_buildPanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_buildPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_buildPanel->OnOkEvent= [this]() { onCancelEvent(); };
	m_buildPanel->OnPreviewViewChanged= [this]() { setPreviewView(m_buildPanel->getPreviewView()); };
	m_buildPanel->OnFrameMeshEvent= [this]() { frameMesh(); };
	m_buildPanel->OnRebuildInBoxEvent= [this]() { onRebuildInBoxEvent(); };

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

	MikanViewportPtr viewport= getFirstViewport();
	viewport->OnMouseRayChanged-= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayChanged);
	viewport->OnMouseRayButtonDown-= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayButtonDown);
	viewport->OnMouseRayButtonUp-= MakeDelegate(this, &AppStage_OcclusionMeshBuild::onMouseRayButtonUp);

	releasePreview();
	m_result= OcclusionMeshBuildResult();
	m_bHasResult= false;

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
	settings.cameraPathRadius= std::max(m_buildPanel->getPathRadius(), 0.f);
	settings.bTexture= m_buildPanel->getTexture();
	if (m_buildPanel->hasBoundsBox())
	{
		settings.bBoundsOverride= true;
		settings.boundsOverrideLo= m_buildPanel->getBoundsBoxLo();
		settings.boundsOverrideHi= m_buildPanel->getBoundsBoxHi();
	}
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

	BuildRequest request;
	request.track= m_track;
	request.capturePath= m_capturePath;
	request.depthMember= m_depthMember;
	request.confidenceMember= m_confidenceMember;
	request.movieMember= m_movieMember;
	request.settings= gatherSettings();

	m_bCancelRequested= false;
	m_buildPhase= (int)eOcclusionMeshBuildPhase::loadingTake;
	m_framesDone= 0;
	m_frameCount= 0;
	m_buildElapsedSeconds= 0.f;

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
	if (!reader.open(request.capturePath, request.depthMember, request.confidenceMember, track.getDepthWidth(),
					 track.getDepthHeight(), track.getDepthUnitMm(), track.getFrameCount()))
	{
		outOutput.result.failureReason= locText("occlusionMeshBuild.depthTracksUnreadable");
		return;
	}

	// The movie plays in place inside the capture, decoded here on the worker
	// with no GL in sight. A movie that will not open costs the texture, not
	// the mesh.
	MovieDecoder decoder;
	if (request.settings.bTexture
		&& !decoder.openMovieUrl(Capture::makeSubfileUrl(request.capturePath, request.movieMember)))
	{
		MIKAN_LOG_WARNING("AppStage_OcclusionMeshBuild") << "The take's movie could not be opened for texturing";
	}

	BuildObserver observer(this);
	OcclusionMeshBuilder::build(track, reader, request.settings, &observer, outOutput.result,
								decoder.isOpen() ? &decoder : nullptr);
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

	releasePreview();
	m_result= std::move(output.result);
	m_bHasResult= true;

	m_buildPanel->setBuildSummary(m_result.stats);
	buildPreview();
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

	// The atlas goes beside the model under the model's own stem, which is
	// where the model's material and a client's placement texture both look.
	// Written before the glb so the reference never dangles.
	std::string textureFileName;
	if (m_result.mesh.hasTexCoords() && !m_result.atlas.empty())
	{
		textureFileName= std::string(stencilName) + ".png";
		const std::filesystem::path texturePath= modelsDirectory / textureFileName;
		if (!cv::imwrite(texturePath.string(), m_result.atlas))
		{
			MIKAN_LOG_WARNING("AppStage_OcclusionMeshBuild")
				<< "Failed to write the atlas " << texturePath.string() << ", the stencil will be untextured";
			textureFileName.clear();
		}
	}

	if (!GlbMeshWriter::write(meshPath, stencilName, m_result.mesh.vertices, m_result.mesh.normals,
							  textureFileName.empty() ? std::vector<glm::vec2>() : m_result.mesh.texCoords,
							  m_result.mesh.indices, textureFileName))
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

	// The vertices are in the take's ARKit world space. The stencil remembers
	// the take, so a later alignment of it moves the stencil along.
	stencilComponent->setWorldTransform(getMeshToWorld());
	stencilComponent->getModelStencilDefinition()->setAlignedTake(m_videoSource->getVideoSourceId(),
																  m_videoSource->getPoseSessionId());

	if (ProjectAssetCatalog* catalog= getOwnerWindow()->getAssetCatalog())
		catalog->refresh();

	m_buildPanel->setCreatedStencilName(stencilName);

	MIKAN_LOG_INFO("AppStage_OcclusionMeshBuild")
		<< "Created occlusion mesh stencil '" << stencilName << "' (" << m_result.mesh.getTriangleCount()
		<< " triangles, " << meshPath.string() << ")";

	return true;
}

void AppStage_OcclusionMeshBuild::onApplyEvent()
{
	if (createStencilFromMesh())
		setMenuState(eOcclusionMeshBuildMenuState::buildComplete);
}

void AppStage_OcclusionMeshBuild::onRedoEvent()
{
	releasePreview();
	m_bHasResult= false;
	m_draggedBoxHandle= -1;
	m_hoveredBoxHandle= -1;
	setMenuState(eOcclusionMeshBuildMenuState::verifyTake);
}

void AppStage_OcclusionMeshBuild::onRebuildInBoxEvent()
{
	if (!m_bHasResult)
		return;

	m_buildPanel->setBoundsBox(m_boxLo, m_boxHi);
	onRedoEvent();
}

void AppStage_OcclusionMeshBuild::onCancelEvent() { getOwnerWindow()->popAppState(); }

// -- preview -----
glm::mat4 AppStage_OcclusionMeshBuild::getMeshToWorld() const
{
	// Where the stencil will land
	return FileVideoSourceSystem::computeTakeToWorldTransform(*m_stage, *m_videoSource);
}

void AppStage_OcclusionMeshBuild::buildPreview()
{
	if (!m_bHasResult)
		return;

	if (!m_preview->build(getGraphicsContext(), m_result, OcclusionMeshPreview::getDefaultView(m_result)))
	{
		MIKAN_LOG_WARNING("AppStage_OcclusionMeshBuild") << "The mesh preview could not be built";
		return;
	}
	m_preview->setModelMatrix(getMeshToWorld());

	uint32_t viewMask= 0;
	for (int viewIndex= 0; viewIndex < (int)eOcclusionMeshPreviewView::COUNT; ++viewIndex)
	{
		if (m_preview->isViewAvailable((eOcclusionMeshPreviewView)viewIndex))
			viewMask|= 1u << viewIndex;
	}
	m_buildPanel->setAvailablePreviewViews(viewMask);
	m_buildPanel->setPreviewView(m_preview->getView());

	// The box starts on the bounds this build fused inside
	m_boxLo= m_result.stats.boundsLo;
	m_boxHi= m_result.stats.boundsHi;
	m_hoveredBoxHandle= -1;
	m_draggedBoxHandle= -1;

	frameMesh();
}

void AppStage_OcclusionMeshBuild::releasePreview() { m_preview->release(); }

bool AppStage_OcclusionMeshBuild::setPreviewView(eOcclusionMeshPreviewView view)
{
	// The panel shows whatever the preview ended up on, so a refused view
	// puts the combo back
	const bool bChanged= m_preview->setView(view);
	m_buildPanel->setPreviewView(m_preview->getView());
	return bChanged;
}

void AppStage_OcclusionMeshBuild::frameMesh()
{
	if (!m_preview->hasMesh() || !m_mkCamera)
		return;

	const glm::mat4 meshToWorld= getMeshToWorld();
	const glm::vec3 centre=
		glm::vec3(meshToWorld * glm::vec4(0.5f * (m_preview->getBoundsLo() + m_preview->getBoundsHi()), 1.f));
	const glm::vec3 diagonal= glm::mat3(meshToWorld) * (m_preview->getBoundsHi() - m_preview->getBoundsLo());
	const float radius= std::max(0.5f * glm::length(diagonal), 0.1f);

	// The fused cameras' mean forward, in the world: the orbit eye sits
	// against it so the mesh first shows from where the operator stood.
	// Recorded poses look down their -Z.
	glm::vec3 meanForward(0.f);
	for (size_t trackIndex : m_result.fusedKeyframes)
	{
		meanForward-= glm::vec3(m_track.getFrame(trackIndex).cameraToWorld[2]);
	}
	meanForward= glm::mat3(meshToWorld) * meanForward;
	const glm::vec3 toEye= glm::length(meanForward) > 1e-6f ? -glm::normalize(meanForward) : glm::vec3(0.f, 0.5f, 1.f);
	const float yawDegrees= glm::degrees(std::atan2(toEye.x, toEye.z));
	const float pitchDegrees= std::max(glm::degrees(std::asin(std::clamp(toEye.y, -1.f, 1.f))),
									   OcclusionMeshBuildStage::k_minFramePitchDegrees);

	// Far enough that the bounding sphere fits the narrower field of view
	const float halfFov=
		glm::radians(0.5f * std::min(m_mkCamera->getHorizontalFOVDegrees(), m_mkCamera->getVerticalFOVDegrees()));
	const float distance= radius / std::max(std::sin(halfFov), 0.1f);

	m_mkCamera->setOrbitTargetPosition(centre);
	m_mkCamera->setOrbitLocation(yawDegrees, pitchDegrees, distance);
}

void AppStage_OcclusionMeshBuild::renderMeshPreview()
{
	if (!m_bHasResult || !m_preview->hasMesh())
		return;

	// The stage can move while the preview is up
	m_preview->setModelMatrix(getMeshToWorld());
	m_preview->render(m_mkCamera, getGraphicsContext());
}

void AppStage_OcclusionMeshBuild::renderCameraPath()
{
	if (!m_bHasResult || !m_buildPanel->getShowCameraPath())
		return;

	const glm::mat4 meshToWorld= getMeshToWorld();
	const glm::vec3 pathColor(0.2f, 0.6f, 1.f);
	bool bHasPrevious= false;
	glm::vec3 previous(0.f);
	for (size_t trackIndex : m_result.fusedKeyframes)
	{
		const glm::vec3 position(m_track.getFrame(trackIndex).cameraToWorld[3]);
		if (bHasPrevious)
			drawSegment(getGraphicsContext(), meshToWorld, previous, position, pathColor);
		previous= position;
		bHasPrevious= true;
	}
}

void AppStage_OcclusionMeshBuild::renderBoundsBox()
{
	if (!m_bHasResult || m_buildPanel->getMenuState() != eOcclusionMeshBuildMenuState::verifyMesh)
		return;

	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	const glm::mat4 meshToWorld= getMeshToWorld();
	drawTransformedBox(graphicsContext, meshToWorld, m_boxLo, m_boxHi, glm::vec3(1.f, 0.6f, 0.1f));
	for (int handle= 0; handle < 6; ++handle)
	{
		const bool bActive= handle == m_draggedBoxHandle || (m_draggedBoxHandle < 0 && handle == m_hoveredBoxHandle);
		drawPoint(graphicsContext, meshToWorld, getBoxHandleCenter(handle),
				  bActive ? glm::vec3(1.f, 1.f, 0.2f) : glm::vec3(1.f, 0.6f, 0.1f), bActive ? 14.f : 9.f);
	}
}

void AppStage_OcclusionMeshBuild::render(IMkViewportPtr targetViewport)
{
	switch (m_buildPanel->getMenuState())
	{
	case eOcclusionMeshBuildMenuState::verifyMesh:
	case eOcclusionMeshBuildMenuState::buildComplete:
		renderMeshPreview();
		renderCameraPath();
		renderBoundsBox();
		break;

	default:
		break;
	}

	getGraphicsContext()->getLineRenderer()->render(true);
}

// -- bounds box -----
glm::vec3 AppStage_OcclusionMeshBuild::getBoxHandleCenter(int handle) const
{
	glm::vec3 centre= 0.5f * (m_boxLo + m_boxHi);
	const int axis= handle / 2;
	centre[axis]= (handle % 2) == 1 ? m_boxHi[axis] : m_boxLo[axis];
	return centre;
}

float AppStage_OcclusionMeshBuild::getBoxHandleRadius() const
{
	return std::max(0.04f * glm::length(m_boxHi - m_boxLo), 0.02f);
}

int AppStage_OcclusionMeshBuild::pickBoxHandle(const glm::vec3& rayOrigin, const glm::vec3& rayDir) const
{
	// Picked in the mesh's own space, where the box is axis aligned
	const glm::mat4 worldToMesh= glm::inverse(getMeshToWorld());
	const glm::vec3 origin= glm::vec3(worldToMesh * glm::vec4(rayOrigin, 1.f));
	const glm::vec3 direction= glm::normalize(glm::mat3(worldToMesh) * rayDir);
	const float radius= getBoxHandleRadius();

	int bestHandle= -1;
	float bestTime= std::numeric_limits<float>::max();
	for (int handle= 0; handle < 6; ++handle)
	{
		float time= 0.f;
		glm::vec3 closest;
		const glm::vec3 centre= getBoxHandleCenter(handle);
		if (glm_closest_point_on_ray_to_point(origin, direction, centre, time, closest)
			&& glm::length(closest - centre) <= radius && time < bestTime)
		{
			bestTime= time;
			bestHandle= handle;
		}
	}
	return bestHandle;
}

void AppStage_OcclusionMeshBuild::onMouseRayChanged(const glm::vec3& rayOrigin, const glm::vec3& rayDir)
{
	if (!m_bHasResult || m_buildPanel->getMenuState() != eOcclusionMeshBuildMenuState::verifyMesh)
		return;

	if (m_draggedBoxHandle < 0)
	{
		m_hoveredBoxHandle= pickBoxHandle(rayOrigin, rayDir);
		return;
	}

	// The dragged face follows the point on its axis nearest the ray
	const glm::mat4 worldToMesh= glm::inverse(getMeshToWorld());
	const glm::vec3 origin= glm::vec3(worldToMesh * glm::vec4(rayOrigin, 1.f));
	const glm::vec3 direction= glm::normalize(glm::mat3(worldToMesh) * rayDir);
	const int axis= m_draggedBoxHandle / 2;
	glm::vec3 axisDirection(0.f);
	axisDirection[axis]= 1.f;
	glm::vec3 axisStart= getBoxHandleCenter(m_draggedBoxHandle);
	axisStart[axis]= 0.f;

	float axisTime= 0.f;
	glm::vec3 closest;
	if (!glm_closest_point_on_ray_to_ray(axisStart, axisDirection, origin, direction, axisTime, closest))
		return;

	const float face= axisTime - m_dragGrabOffset;
	if ((m_draggedBoxHandle % 2) == 1)
		m_boxHi[axis]= std::max(face, m_boxLo[axis] + OcclusionMeshBuildStage::k_minBoxExtent);
	else
		m_boxLo[axis]= std::min(face, m_boxHi[axis] - OcclusionMeshBuildStage::k_minBoxExtent);
}

void AppStage_OcclusionMeshBuild::onMouseRayButtonDown(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	if (button != MkMouseButton::LEFT || !m_bHasResult
		|| m_buildPanel->getMenuState() != eOcclusionMeshBuildMenuState::verifyMesh)
	{
		return;
	}

	const int handle= pickBoxHandle(rayOrigin, rayDir);
	if (handle < 0)
		return;

	const glm::mat4 worldToMesh= glm::inverse(getMeshToWorld());
	const glm::vec3 origin= glm::vec3(worldToMesh * glm::vec4(rayOrigin, 1.f));
	const glm::vec3 direction= glm::normalize(glm::mat3(worldToMesh) * rayDir);
	const int axis= handle / 2;
	glm::vec3 axisDirection(0.f);
	axisDirection[axis]= 1.f;
	glm::vec3 axisStart= getBoxHandleCenter(handle);
	axisStart[axis]= 0.f;

	float axisTime= 0.f;
	glm::vec3 closest;
	if (!glm_closest_point_on_ray_to_ray(axisStart, axisDirection, origin, direction, axisTime, closest))
		return;

	m_draggedBoxHandle= handle;
	m_dragGrabOffset= axisTime - getBoxHandleCenter(handle)[axis];
}

void AppStage_OcclusionMeshBuild::onMouseRayButtonUp(const glm::vec3& rayOrigin, const glm::vec3& rayDir, int button)
{
	if (button == MkMouseButton::LEFT)
		m_draggedBoxHandle= -1;
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
	else if (command == "set_option" && parameters.size() == 2 && parameters[0] == "view")
	{
		// The one option that applies after a build, to the preview
		if (menuState != eOcclusionMeshBuildMenuState::verifyMesh
			&& menuState != eOcclusionMeshBuildMenuState::buildComplete)
		{
			outResults.push_back("view needs a built mesh");
			return false;
		}
		for (int viewIndex= 0; viewIndex < (int)eOcclusionMeshPreviewView::COUNT; ++viewIndex)
		{
			if (k_OcclusionMeshPreviewViewStrings[viewIndex] == parameters[1])
			{
				if (setPreviewView((eOcclusionMeshPreviewView)viewIndex))
					return true;
				outResults.push_back("the mesh cannot show the view " + parameters[1]);
				return false;
			}
		}
		outResults.push_back("unknown view " + parameters[1]);
		return false;
	}
	else if (command == "set_option" && !parameters.empty() && parameters[0] == "bounds")
	{
		if (menuState != eOcclusionMeshBuildMenuState::verifyTake)
		{
			outResults.push_back("options can only change before a build");
			return false;
		}
		if (parameters.size() == 2 && parameters[1] == "auto")
		{
			m_buildPanel->clearBoundsBox();
			return true;
		}
		if (parameters.size() != 7)
		{
			outResults.push_back("usage: set_option bounds <lo.x> <lo.y> <lo.z> <hi.x> <hi.y> <hi.z> | auto");
			return false;
		}
		float box[6];
		for (int index= 0; index < 6; ++index)
		{
			const std::string& text= parameters[index + 1];
			char* end= nullptr;
			box[index]= std::strtof(text.c_str(), &end);
			if (end == text.c_str())
			{
				outResults.push_back("bounds must be six metres");
				return false;
			}
		}
		const glm::vec3 lo(box[0], box[1], box[2]);
		const glm::vec3 hi(box[3], box[4], box[5]);
		if (!(lo.x < hi.x && lo.y < hi.y && lo.z < hi.z))
		{
			outResults.push_back("bounds need lo below hi on every axis");
			return false;
		}
		m_buildPanel->setBoundsBox(lo, hi);
		return true;
	}
	else if (command == "set_option")
	{
		if (parameters.size() != 2)
		{
			outResults.push_back("usage: set_option "
								 "<voxel_preset|keyframe_stride|path_radius|texture|"
								 "view|bounds> <value>");
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
		else if (option == "path_radius")
		{
			char* end= nullptr;
			const float radius= std::strtof(value.c_str(), &end);
			if (end == value.c_str() || radius < 0.f)
			{
				outResults.push_back("path_radius must be metres, 0 or more");
				return false;
			}
			m_buildPanel->setPathRadius(radius);
			return true;
		}
		else if (option == "texture" && OcclusionMeshBuildStage::parseBool(value, bFlag))
		{
			m_buildPanel->setTexture(bFlag);
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
		outResults.push_back("voxels_clipped " + std::to_string(stats.voxelsClipped));
		outResults.push_back("texture_views " + std::to_string(stats.textureViewsUsed) + " "
							 + std::to_string(stats.textureViews));
		outResults.push_back("triangles_untextured " + std::to_string(stats.trianglesUntextured));
		outResults.push_back("atlas " + std::to_string(stats.atlasWidth) + " " + std::to_string(stats.atlasHeight));
		outResults.push_back("seconds " + std::to_string(stats.totalSeconds));
		outResults.push_back("coverage_good " + std::to_string(stats.coverageGood));
		outResults.push_back("coverage_weak " + std::to_string(stats.coverageWeak));
		outResults.push_back("coverage_unseen " + std::to_string(stats.coverageUnseen));
		outResults.push_back("bounds " + std::to_string(stats.boundsLo.x) + " " + std::to_string(stats.boundsLo.y) + " "
							 + std::to_string(stats.boundsLo.z) + " " + std::to_string(stats.boundsHi.x) + " "
							 + std::to_string(stats.boundsHi.y) + " " + std::to_string(stats.boundsHi.z));
		outResults.push_back(std::string("bounds_overridden ") + (stats.bBoundsOverridden ? "1" : "0"));
		outResults.push_back(std::string("voxel_grown ") + (stats.bVoxelGrown ? "1" : "0"));
		outResults.push_back("view " + k_OcclusionMeshPreviewViewStrings[(int)m_preview->getView()]);
		return true;
	}
	else if (command == "frame_mesh")
	{
		if (!m_preview->hasMesh())
		{
			outResults.push_back("no mesh to frame");
			return false;
		}
		frameMesh();
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
