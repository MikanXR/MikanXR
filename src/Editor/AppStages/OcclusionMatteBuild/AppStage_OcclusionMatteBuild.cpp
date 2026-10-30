#include "OcclusionMatteBuild/AppStage_OcclusionMatteBuild.h"
#include "OcclusionMatteBuild/GuiPanel_OcclusionMatteBuild.h"

#include "imgui.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedWindow.h"

#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "Capture.h"
#include "FileVideoSourceComponent.h"
#include "FileVideoSourceSystem.h"
#include "GlbMeshReader.h"
#include "IEditorWindow.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MatteTrack.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ModalModelDownload/ModalDialog_ModelDownload.h"
#include "ModelRepository.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "MovieDecoder.h"
#include "ObjectSystemFwd.h"
#include "PathUtils.h"
#include "Sam2Segmenter.h"
#include "StageComponent.h"

#include "glm/glm.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>

const char* AppStage_OcclusionMatteBuild::APP_STAGE_NAME= "OcclusionMatteBuild";

namespace OcclusionMatteBuildStage
{
// The preview overlay is shrunk to this width before it crosses to the UI
// thread. The viewport shows it at its own size, so a video frame goes whole.
const int k_previewMaxWidth= 1920;
// How far a labelled pixel is pulled toward its tint
const float k_previewTintBlend= 0.6f;

eModelId toModelId(eOcclusionMatteModel model)
{
	return model == eOcclusionMatteModel::tiny ? eModelId::sam2_tiny : eModelId::sam2_small;
}

bool isGltfExtension(const std::filesystem::path& path)
{
	std::string extension= path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	return extension == ".glb" || extension == ".gltf";
}

// True when the project has a model stencil the build can render
bool hasRenderableStencil(ModelStencilSystemPtr stencilSystem)
{
	if (!stencilSystem)
		return false;

	return stencilSystem->getTypedComponentByPredicate(
			   [](ModelStencilComponentConstPtr stencil)
			   {
				   ModelStencilDefinitionPtr definition= stencil->getModelStencilDefinition();
				   return definition && !definition->getIsDisabled() && definition->hasModelPath();
			   })
		   != nullptr;
}
} // namespace OcclusionMatteBuildStage

// -- BuildObserver -----
bool AppStage_OcclusionMatteBuild::BuildObserver::isCancelRequested() const
{
	return m_stage->m_bCancelRequested.load();
}

void AppStage_OcclusionMatteBuild::BuildObserver::onProgress(eOcclusionMatteBuildPhase phase, int done, int total)
{
	m_stage->m_buildPhase= (int)phase;
	m_stage->m_framesDone= done;
	m_stage->m_frameCount= total;
}

void AppStage_OcclusionMatteBuild::BuildObserver::onFrameProcessed(const OcclusionMatteFrame& frame)
{
	m_stage->composePreviewOverlay(frame);
}

// -- AppStage_OcclusionMatteBuild -----
AppStage_OcclusionMatteBuild::AppStage_OcclusionMatteBuild(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, APP_STAGE_NAME)
{
}

AppStage_OcclusionMatteBuild::~AppStage_OcclusionMatteBuild() {}

bool AppStage_OcclusionMatteBuild::tryEnterBuild(AppStage* fromAppStage, FileVideoSourceComponentPtr videoSource)
{
	if (!fromAppStage || !videoSource)
		return false;

	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();
	FileVideoSourceDefinitionPtr definition= videoSource->getFileVideoSourceDefinition();

	if (PathUtils::getProjectDirectory().empty())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noProjectLoaded"));
		return false;
	}

	const std::filesystem::path mediaPath= PathUtils::resolveProjectResource(definition->getMediaPath());
	if (mediaPath.empty())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noMediaPath"));
		return false;
	}

	// Only a trackcamera carries the per-frame poses the meshes are rendered at
	Capture capture;
	std::string captureError;
	if (CaptureKindUtils::kindFromExtension(mediaPath) != eCaptureKind::trackcamera
		|| !capture.open(mediaPath, captureError))
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.notTrackCamera"));
		return false;
	}

	const CaptureArchiveMember* movieMember= capture.findMember(eCaptureMemberRole::movie);
	if (movieMember == nullptr)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noMovie"));
		return false;
	}

	// The matte is rendered from the track the source plays: the override, when
	// one was set by hand, or the capture's own track. An override from another
	// session is a leftover of a different take and is skipped the way playback
	// skips it, since its frames index nothing in this capture.
	PoseTrack track;
	const std::filesystem::path trackPath= PathUtils::resolveProjectResource(definition->getPoseTrackPath());
	if (!definition->getPoseTrackPath().empty())
	{
		if (trackPath.empty() || !track.loadFromFile(trackPath))
		{
			ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.poseTrackMissing"));
			return false;
		}
		if (track.getSessionId() != capture.getManifest().sessionId)
		{
			MIKAN_LOG_WARNING("AppStage_OcclusionMatteBuild")
				<< "The pose track override " << trackPath << " was recorded in session " << track.getSessionId()
				<< ", not the capture's " << capture.getManifest().sessionId << ", so the capture's own track is used";
			track.clear();
		}
	}
	if (!track.isLoaded())
	{
		track= capture.getPoseTrack(eCaptureMemberRole::movie_pose);
	}
	if (!track.isLoaded() || track.getFrameCount() == 0)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noPoseTrack"));
		return false;
	}

	// The meshes live in the stage's world; the take's alignment is what puts
	// them back into the take's own world for the render
	if (!videoSource->hasPoseOffset())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.notAligned"));
		return false;
	}

	auto cameraSystem= ownerWindow->getProjectManager()->getSystemOfType<CameraObjectSystem>();
	CameraComponentPtr camera=
		cameraSystem ? cameraSystem->findCameraForVideoSource(videoSource->getVideoSourceId()) : CameraComponentPtr();
	StageComponentConstPtr stage= camera ? camera->getOwnerStageComponent() : StageComponentConstPtr();
	if (!stage)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noCamera"));
		return false;
	}

	if (!OcclusionMatteBuildStage::hasRenderableStencil(
			ownerWindow->getProjectManager()->getSystemOfType<ModelStencilSystem>()))
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("occlusionMatteBuild.noModelStencil"));
		return false;
	}

	auto* buildStage= ownerWindow->pushAppStageOfType<AppStage_OcclusionMatteBuild>();
	buildStage->setSourceVideoSource(videoSource, stage, track, mediaPath, *movieMember);
	return true;
}

void AppStage_OcclusionMatteBuild::setSourceVideoSource(FileVideoSourceComponentPtr videoSource,
														StageComponentConstPtr stage, const PoseTrack& track,
														const std::filesystem::path& capturePath,
														const CaptureArchiveMember& movieMember)
{
	m_videoSource= videoSource;
	m_stage= stage;
	m_track= track;
	m_capturePath= capturePath;
	m_movieMember= movieMember;
}

void AppStage_OcclusionMatteBuild::enter()
{
	AppStage::enter();

	m_buildPanel= addGuiPanel<GuiPanel_OcclusionMatteBuild>();
	m_buildPanel->OnBuildEvent= [this]() { onBuildEvent(); };
	m_buildPanel->OnCancelBuildEvent= [this]() { onCancelBuildEvent(); };
	m_buildPanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_buildPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_buildPanel->OnOkEvent= [this]() { onCancelEvent(); };

	int stencilCount= 0;
	if (auto stencilSystem= getSystemOfType<ModelStencilSystem>())
	{
		stencilSystem->visitComponents(
			[&stencilCount](ModelStencilComponentPtr stencil)
			{
				ModelStencilDefinitionPtr definition= stencil->getModelStencilDefinition();
				if (definition && !definition->getIsDisabled() && definition->hasModelPath())
					stencilCount++;
			});
	}

	const size_t frameCount= m_track.getFrameCount();
	const float durationSeconds=
		frameCount > 1 ? (float)(m_track.getFrame(frameCount - 1).timeUs - m_track.getFrame(0).timeUs) * 1e-6f : 0.f;
	m_buildPanel->setTakeSummary((int)frameCount, m_track.getImageWidth(), m_track.getImageHeight(), durationSeconds,
								 stencilCount);

	m_bHasResult= false;
	startBuildWorker();

	// Drawn with the video's own flip so the overlay reads the way the take plays
	m_previewQuad= createFullscreenQuadMesh(getGraphicsContext(), true);

	setMenuState(eOcclusionMatteBuildMenuState::verifyTake);
}

void AppStage_OcclusionMatteBuild::exit()
{
	setMenuState(eOcclusionMatteBuildMenuState::inactive);

	// The worker joins before anything it reads goes away, and frees the
	// segmenter on the thread that created it
	stopBuildWorker();

	releasePreview();
	m_previewQuad= nullptr;
	m_result= OcclusionMatteBuildResult();
	m_bHasResult= false;

	m_videoSource= nullptr;
	m_stage= nullptr;

	AppStage::exit();
}

void AppStage_OcclusionMatteBuild::setMenuState(eOcclusionMatteBuildMenuState newState)
{
	if (m_buildPanel != nullptr)
		m_buildPanel->setMenuState(newState);
}

eModelId AppStage_OcclusionMatteBuild::getChosenModelId() const
{
	return OcclusionMatteBuildStage::toModelId(m_buildPanel->getModel());
}

OcclusionMatteSettings AppStage_OcclusionMatteBuild::gatherSettings() const
{
	OcclusionMatteSettings settings;
	settings.bandPx= std::max(m_buildPanel->getBandPx(), 1);
	settings.matteScale= m_buildPanel->getMatteScale() == 2 ? 2 : 1;
	settings.frameStep= std::max(m_buildPanel->getFrameStep(), 1);
	settings.bCropSmallRuns= m_buildPanel->getCropSmallRuns();
	if (const ModelCatalogEntry* entry= ModelCatalog::findEntry(getChosenModelId()))
		settings.modelName= entry->name;
	return settings;
}

bool AppStage_OcclusionMatteBuild::gatherMeshes(std::vector<RequestMesh>& outMeshes)
{
	outMeshes.clear();

	auto stencilSystem= getSystemOfType<ModelStencilSystem>();
	if (!stencilSystem)
	{
		m_buildPanel->setFailureReason(locText("occlusionMatteBuild.noModelStencil"));
		return false;
	}

	// The meshes are read here, on the UI thread, since the scene objects are
	// not for the worker to touch. Each goes to the take's own world through
	// the inverse of the transform that carries the take's meshes into the
	// editor's world.
	const glm::mat4 worldToTake=
		glm::inverse(FileVideoSourceSystem::computeTakeToWorldTransform(*m_stage, *m_videoSource));
	stencilSystem->visitComponents(
		[this, &outMeshes, &worldToTake](ModelStencilComponentPtr stencil)
		{
			ModelStencilDefinitionPtr definition= stencil->getModelStencilDefinition();
			if (!definition || definition->getIsDisabled() || !definition->hasModelPath())
				return;

			const std::filesystem::path storedPath= definition->getModelPath();
			if (!OcclusionMatteBuildStage::isGltfExtension(storedPath))
			{
				MIKAN_LOG_INFO("AppStage_OcclusionMatteBuild")
					<< "Skipping the model stencil '" << stencil->getName() << "': " << storedPath.string()
					<< " is not a glTF model";
				return;
			}

			RequestMesh requestMesh;
			std::string meshError;
			const std::filesystem::path modelPath= PathUtils::resolveProjectResource(storedPath);
			if (modelPath.empty() || !GlbMeshReader::read(modelPath, requestMesh.mesh, meshError))
			{
				MIKAN_LOG_WARNING("AppStage_OcclusionMatteBuild")
					<< "Skipping the model stencil '" << stencil->getName() << "': " << meshError;
				return;
			}

			requestMesh.meshToTakeWorld= worldToTake * stencil->getWorldTransform();
			requestMesh.name= stencil->getName();
			requestMesh.modelPath= storedPath.string();
			outMeshes.push_back(std::move(requestMesh));
		});

	if (outMeshes.empty())
	{
		m_buildPanel->setFailureReason(locText("occlusionMatteBuild.noReadableMesh"));
		return false;
	}
	return true;
}

void AppStage_OcclusionMatteBuild::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	updatePreviewTexture();

	if (m_buildPanel->getMenuState() != eOcclusionMatteBuildMenuState::building)
		return;

	m_buildElapsedSeconds+= deltaSeconds;
	m_buildPanel->setBuildProgress((eOcclusionMatteBuildPhase)m_buildPhase.load(), m_framesDone.load(),
								   m_frameCount.load(), m_buildElapsedSeconds, m_bCancelRequested.load());

	bool bBuildFinished= false;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		bBuildFinished= m_bBuildFinished;
	}

	if (bBuildFinished)
		consumeBuildOutput();
}

void AppStage_OcclusionMatteBuild::onGui()
{
	AppStage::onGui();

	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##OcclusionMatteBuild", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

// -- build worker -----
void AppStage_OcclusionMatteBuild::onBuildEvent()
{
	if (m_buildPanel->getMenuState() != eOcclusionMatteBuildMenuState::verifyTake)
		return;

	// The segmenter's weights are not shipped, so the first build on a machine
	// offers to fetch them. The dialog re-runs this handler once the download
	// lands, and does nothing at all when the model is already there.
	if (ModalDialog_ModelDownload::requestModels(
			this, {getChosenModelId()}, [this]() { onBuildEvent(); },
			[this]()
			{
				m_buildPanel->setFailureReason(locText("occlusionMatteBuild.modelNotInstalled"));
				setMenuState(eOcclusionMatteBuildMenuState::failed);
			}))
	{
		return;
	}

	BuildRequest request;
	if (!gatherMeshes(request.meshes))
	{
		setMenuState(eOcclusionMatteBuildMenuState::failed);
		return;
	}
	request.track= m_track;
	request.capturePath= m_capturePath;
	request.movieMember= m_movieMember;
	request.settings= gatherSettings();
	request.jsonPath= MatteTrack::sidecarPathForCapture(m_capturePath);
	request.modelDirectory= ModelRepository::resolveDirectory(getChosenModelId());

	releasePreview();
	m_bHasResult= false;
	m_bCancelRequested= false;
	m_buildPhase= (int)eOcclusionMatteBuildPhase::idle;
	m_framesDone= 0;
	m_frameCount= (int)m_track.getFrameCount();
	m_buildElapsedSeconds= 0.f;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_pendingRequest= std::move(request);
		m_bBuildRequested= true;
		m_bBuildFinished= false;
	}
	m_workerSignal.notify_one();

	setMenuState(eOcclusionMatteBuildMenuState::building);
}

void AppStage_OcclusionMatteBuild::onCancelBuildEvent()
{
	m_bCancelRequested= true;

	// The builder polls the flag between frames; the segmenter is told as well
	// so a run in flight gives up instead of finishing its frame first
	std::lock_guard<std::mutex> lock(m_workerMutex);
	if (m_segmenter)
		m_segmenter->requestCancel();
}

void AppStage_OcclusionMatteBuild::startBuildWorker()
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

void AppStage_OcclusionMatteBuild::stopBuildWorker()
{
	if (!m_workerThread.joinable())
		return;

	// Cancel anything in flight first, so the join waits out at most one
	// frame's segmentation
	m_bCancelRequested= true;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		if (m_segmenter)
			m_segmenter->requestCancel();
		m_bWorkerShutdownRequested= true;
	}
	m_workerSignal.notify_one();

	m_workerThread.join();
}

void AppStage_OcclusionMatteBuild::buildWorkerMain()
{
	for (;;)
	{
		BuildRequest request;
		{
			std::unique_lock<std::mutex> lock(m_workerMutex);
			m_workerSignal.wait(lock, [this]() { return m_bWorkerShutdownRequested || m_bBuildRequested; });

			if (m_bWorkerShutdownRequested)
			{
				// The ONNX sessions are destroyed on the thread that created
				// and ran them
				m_segmenter= nullptr;
				m_segmenterModelDirectory.clear();
				return;
			}

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

void AppStage_OcclusionMatteBuild::runBuildRequest(const BuildRequest& request, BuildOutput& outOutput)
{
	// -- the segmenter, started on first use and restarted when the model changes --
	Sam2Segmenter* segmenter= nullptr;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		if (m_segmenter && m_segmenterModelDirectory != request.modelDirectory)
		{
			m_segmenter= nullptr;
			m_segmenterModelDirectory.clear();
		}
		segmenter= m_segmenter.get();
	}

	if (segmenter == nullptr)
	{
		Sam2Segmenter::Config config;
		config.modelDirectory= request.modelDirectory.string();
		config.preferGpu= true;

		// startup() loads the two graphs, so it runs outside the lock
		auto newSegmenter= std::make_unique<Sam2Segmenter>();
		if (!newSegmenter->startup(config))
		{
			outOutput.result.failureReason=
				locFormat("occlusionMatteBuild.modelLoadFailedFmt", request.modelDirectory.string().c_str());
			return;
		}

		std::lock_guard<std::mutex> lock(m_workerMutex);
		segmenter= newSegmenter.get();
		m_segmenter= std::move(newSegmenter);
		m_segmenterModelDirectory= request.modelDirectory;
	}

	if (m_bCancelRequested.load())
	{
		outOutput.result.bCancelled= true;
		return;
	}

	// The movie plays in place inside the capture, decoded here on the worker
	MovieDecoder decoder;
	if (!decoder.openMovieUrl(Capture::makeSubfileUrl(request.capturePath, request.movieMember)))
	{
		outOutput.result.failureReason= locText("occlusionMatteBuild.movieUnreadable");
		return;
	}

	std::vector<MatteMesh> matteMeshes;
	matteMeshes.reserve(request.meshes.size());
	for (const RequestMesh& requestMesh : request.meshes)
	{
		MatteMesh matteMesh;
		matteMesh.mesh= &requestMesh.mesh;
		matteMesh.meshToTakeWorld= requestMesh.meshToTakeWorld;
		matteMesh.name= requestMesh.name;
		matteMesh.modelPath= requestMesh.modelPath;
		matteMeshes.push_back(matteMesh);
	}

	BuildObserver observer(this);
	OcclusionMatteBuilder::build(request.track, decoder, matteMeshes, request.settings, *segmenter, request.jsonPath,
								 &observer, outOutput.result);
	if (!outOutput.result.bSucceeded && !outOutput.result.bCancelled && !outOutput.result.failureReason.empty())
	{
		MIKAN_LOG_WARNING("AppStage_OcclusionMatteBuild")
			<< "Occlusion matte build failed: " << outOutput.result.failureReason;
	}
}

void AppStage_OcclusionMatteBuild::consumeBuildOutput()
{
	BuildOutput output;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		output= std::move(m_buildOutput);
		m_buildOutput= BuildOutput();
		m_bBuildFinished= false;
	}

	m_buildPhase= (int)eOcclusionMatteBuildPhase::idle;

	// The sidecar sits beside the capture, so the source picks it up without
	// reopening the media: the new one after a build, the old one after a
	// cancelled or failed build
	if (m_videoSource)
		m_videoSource->reloadOcclusionMatte();

	if (output.result.bCancelled)
	{
		MIKAN_LOG_INFO("AppStage_OcclusionMatteBuild") << "Build cancelled";
		setMenuState(eOcclusionMatteBuildMenuState::verifyTake);
		return;
	}

	if (!output.result.bSucceeded)
	{
		m_buildPanel->setFailureReason(output.result.failureReason);
		setMenuState(eOcclusionMatteBuildMenuState::failed);
		return;
	}

	m_result= std::move(output.result);
	m_bHasResult= true;

	m_buildPanel->setBuildSummary(m_result.stats, (int)m_track.getFrameCount(), m_result.jsonPath.string());

	MIKAN_LOG_INFO("AppStage_OcclusionMatteBuild")
		<< "Built the occlusion matte " << m_result.jsonPath.string() << " (" << m_result.stats.framesSegmented
		<< " frames segmented, " << m_result.stats.framesHeld << " held)";

	setMenuState(eOcclusionMatteBuildMenuState::buildComplete);
}

void AppStage_OcclusionMatteBuild::onRedoEvent()
{
	m_bHasResult= false;
	setMenuState(eOcclusionMatteBuildMenuState::verifyTake);
}

void AppStage_OcclusionMatteBuild::onCancelEvent() { getOwnerWindow()->popAppState(); }

// -- preview -----
void AppStage_OcclusionMatteBuild::composePreviewOverlay(const OcclusionMatteFrame& frame)
{
	if (frame.bgr.empty() || frame.bgr.type() != CV_8UC3 || frame.labels.empty() || frame.labels.type() != CV_8UC1)
		return;

	// Shrunk first, so the tint runs over the preview's pixels rather than the
	// frame's. The labels land on the same grid by nearest neighbour, which
	// also covers a matte built at half the video size.
	const int previewWidth= std::min(frame.bgr.cols, OcclusionMatteBuildStage::k_previewMaxWidth);
	const int previewHeight= std::max((int)((int64_t)frame.bgr.rows * previewWidth / frame.bgr.cols), 1);
	const cv::Size previewSize(previewWidth, previewHeight);

	cv::Mat overlay;
	if (frame.bgr.size() == previewSize)
		overlay= frame.bgr.clone();
	else
		cv::resize(frame.bgr, overlay, previewSize, 0.0, 0.0, cv::INTER_AREA);

	cv::Mat labels;
	if (frame.labels.size() == previewSize)
		labels= frame.labels;
	else
		cv::resize(frame.labels, labels, previewSize, 0.0, 0.0, cv::INTER_NEAREST);

	const float keep= 1.f - OcclusionMatteBuildStage::k_previewTintBlend;
	const float tint= OcclusionMatteBuildStage::k_previewTintBlend * 255.f;
	for (int y= 0; y < overlay.rows; ++y)
	{
		cv::Vec3b* overlayRow= overlay.ptr<cv::Vec3b>(y);
		const uint8_t* labelRow= labels.ptr<uint8_t>(y);
		for (int x= 0; x < overlay.cols; ++x)
		{
			const uint8_t label= labelRow[x];
			if (label == (uint8_t)eMatteLabel::keep)
				continue;

			// Added pixels toward green, removed toward red, in BGR
			cv::Vec3b& pixel= overlayRow[x];
			const int tintChannel= label == (uint8_t)eMatteLabel::added ? 1 : 2;
			for (int channel= 0; channel < 3; ++channel)
			{
				const float value= keep * pixel[channel] + (channel == tintChannel ? tint : 0.f);
				pixel[channel]= (uint8_t)std::min(value, 255.f);
			}
		}
	}

	std::lock_guard<std::mutex> lock(m_previewMutex);
	m_pendingOverlay= std::move(overlay);
	m_bOverlayChanged= true;
}

void AppStage_OcclusionMatteBuild::updatePreviewTexture()
{
	{
		std::lock_guard<std::mutex> lock(m_previewMutex);
		if (!m_bOverlayChanged)
			return;
		m_overlay= std::move(m_pendingOverlay);
		m_pendingOverlay= cv::Mat();
		m_bOverlayChanged= false;
	}

	if (m_overlay.empty() || !m_overlay.isContinuous())
		return;

	const uint16_t width= (uint16_t)m_overlay.cols;
	const uint16_t height= (uint16_t)m_overlay.rows;
	if (m_previewTexture
		&& (m_previewTexture->getTextureWidth() != width || m_previewTexture->getTextureHeight() != height))
	{
		m_previewTexture->disposeTexture();
		m_previewTexture= nullptr;
	}

	if (m_previewTexture == nullptr)
	{
		m_previewTexture= CreateMkTexture(width, height, nullptr,
										  MK_RGB,  // texture format
										  MK_BGR); // buffer format
		m_previewTexture->setGenerateMipMap(false);
		if (!m_previewTexture->createTexture())
		{
			MIKAN_LOG_ERROR("AppStage_OcclusionMatteBuild") << "Failed to create the matte preview texture";
			m_previewTexture= nullptr;
			return;
		}
	}

	m_previewTexture->copyBufferIntoTexture(m_overlay.data, m_overlay.step[0] * m_overlay.rows);
	m_buildPanel->setHasPreview(true);
}

void AppStage_OcclusionMatteBuild::renderPreview()
{
	if (m_previewQuad == nullptr || m_previewTexture == nullptr)
		return;

	MkMaterialInstancePtr materialInstance= m_previewQuad->getMaterialInstance();
	MkMaterialConstPtr material= materialInstance->getMaterial();
	if (auto materialBinding= material->bindMaterial())
	{
		materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, m_previewTexture);
		if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
		{
			m_previewQuad->drawElements();
		}
	}
}

void AppStage_OcclusionMatteBuild::render(IMkViewportPtr targetViewport)
{
	switch (m_buildPanel->getMenuState())
	{
	case eOcclusionMatteBuildMenuState::building:
	case eOcclusionMatteBuildMenuState::buildComplete:
		renderPreview();
		break;

	default:
		break;
	}
}

void AppStage_OcclusionMatteBuild::releasePreview()
{
	{
		std::lock_guard<std::mutex> lock(m_previewMutex);
		m_pendingOverlay= cv::Mat();
		m_bOverlayChanged= false;
	}
	m_overlay= cv::Mat();

	if (m_buildPanel != nullptr)
		m_buildPanel->setHasPreview(false);
	if (m_previewTexture)
	{
		m_previewTexture->disposeTexture();
		m_previewTexture= nullptr;
	}
}

// -- remote control -----
bool AppStage_OcclusionMatteBuild::handleRemoteControlCommand(const std::string& command,
															  const std::vector<std::string>& parameters,
															  std::vector<std::string>& outResults)
{
	const eOcclusionMatteBuildMenuState menuState= m_buildPanel->getMenuState();

	if (command == "get_state")
	{
		outResults.push_back(k_OcclusionMatteBuildMenuStateStrings[(int)menuState]);
		return true;
	}
	else if (command == "set_option")
	{
		if (parameters.size() != 2)
		{
			outResults.push_back("usage: set_option <model|band|scale|step|crop> <value>");
			return false;
		}
		if (menuState != eOcclusionMatteBuildMenuState::verifyTake)
		{
			outResults.push_back("options can only change before a build");
			return false;
		}

		const std::string& option= parameters[0];
		const std::string& value= parameters[1];
		if (option == "model")
		{
			for (int modelIndex= 0; modelIndex < (int)eOcclusionMatteModel::COUNT; ++modelIndex)
			{
				if (k_OcclusionMatteModelStrings[modelIndex] == value)
				{
					m_buildPanel->setModel((eOcclusionMatteModel)modelIndex);
					return true;
				}
			}
			outResults.push_back("unknown model " + value);
			return false;
		}
		else if (option == "band")
		{
			const int bandPx= std::atoi(value.c_str());
			if (bandPx < 1)
			{
				outResults.push_back("band must be at least 1 pixel");
				return false;
			}
			m_buildPanel->setBandPx(bandPx);
			return true;
		}
		else if (option == "scale")
		{
			const int scale= std::atoi(value.c_str());
			if (scale != 1 && scale != 2)
			{
				outResults.push_back("scale must be 1 or 2");
				return false;
			}
			m_buildPanel->setMatteScale(scale);
			return true;
		}
		else if (option == "step")
		{
			const int step= std::atoi(value.c_str());
			if (step < 1)
			{
				outResults.push_back("step must be at least 1");
				return false;
			}
			m_buildPanel->setFrameStep(step);
			return true;
		}
		else if (option == "crop")
		{
			if (value != "0" && value != "1")
			{
				outResults.push_back("crop must be 0 or 1");
				return false;
			}
			m_buildPanel->setCropSmallRuns(value == "1");
			return true;
		}

		outResults.push_back("unknown option " + option);
		return false;
	}
	else if (command == "build")
	{
		if (menuState != eOcclusionMatteBuildMenuState::verifyTake)
		{
			outResults.push_back("build needs the verifyTake state");
			return false;
		}
		onBuildEvent();
		// A missing model leaves the download dialog up, which the panel's state reports
		outResults.push_back(k_OcclusionMatteBuildMenuStateStrings[(int)m_buildPanel->getMenuState()]);
		return true;
	}
	else if (command == "cancel")
	{
		if (menuState == eOcclusionMatteBuildMenuState::building)
			onCancelBuildEvent();
		else
			onRedoEvent();
		return true;
	}
	else if (command == "status")
	{
		static const char* k_phaseNames[]= {"idle", "decoding", "complete"};
		const int phase= std::clamp(m_buildPhase.load(), 0, 2);
		outResults.push_back("state " + k_OcclusionMatteBuildMenuStateStrings[(int)menuState]);
		outResults.push_back(std::string("phase ") + k_phaseNames[phase]);
		outResults.push_back("frames " + std::to_string(m_framesDone.load()));
		outResults.push_back("total " + std::to_string(m_frameCount.load()));
		outResults.push_back("elapsed " + std::to_string(m_buildElapsedSeconds));
		if (m_bHasResult)
		{
			const OcclusionMatteBuildStats& stats= m_result.stats;
			outResults.push_back("records " + std::to_string(m_track.getFrameCount()));
			outResults.push_back("matte " + std::to_string(stats.matteWidth) + " " + std::to_string(stats.matteHeight));
			outResults.push_back("segmented " + std::to_string(stats.framesSegmented));
			outResults.push_back("held " + std::to_string(stats.framesHeld));
			outResults.push_back("unmatched " + std::to_string(stats.framesUnmatched));
			outResults.push_back("runs " + std::to_string(stats.runsTotal));
			outResults.push_back("dropped " + std::to_string(stats.runsDropped));
			outResults.push_back("segment_seconds " + std::to_string(stats.segmentSeconds));
			outResults.push_back("seconds " + std::to_string(stats.totalSeconds));
			outResults.push_back("sidecar " + m_result.jsonPath.string());
		}
		else if (menuState == eOcclusionMatteBuildMenuState::failed)
		{
			outResults.push_back("failure " + m_buildPanel->getFailureReason());
		}
		return true;
	}
	else if (command == "return")
	{
		onCancelEvent();
		return true;
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}
