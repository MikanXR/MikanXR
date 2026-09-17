#include "DepthMeshCapture/AppStage_DepthMeshCapture.h"
#include "DepthMeshCapture/GuiPanel_DepthMeshCapture.h"

#include "imgui.h"
#include "MkGuiScopedWindow.h"

#include "CalibrationPatternFinder_Aruco.h"
#include "CalibrationRenderHelpers.h"
#include "CameraComponent.h"
#include "IEditorWindow.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "IMkViewport.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanCamera.h"
#include "MikanViewport.h"
#include "ModalModelDownload/ModalDialog_ModelDownload.h"
#include "ModelRepository.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "PathUtils.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "StageComponent.h"
#include "StringUtils.h"
#include "Transform.h"
#include "TransformComponent.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>

const char* AppStage_DepthMeshCapture::APP_STAGE_NAME= "DepthMeshCapture";

/// Median model depth in a small window around a (subpixel) frame position.
/// A single-pixel read at a marker corner would be at the mercy of local depth
/// noise and of landing on the marker's own silhouette.
static float sampleMedianDepth(const MoGeInference::Result& geometry, float pixelX, float pixelY)
{
	constexpr int k_windowRadius= 2;
	const int centerX= (int)std::lround(pixelX);
	const int centerY= (int)std::lround(pixelY);

	std::vector<float> samples;
	for (int y= centerY - k_windowRadius; y <= centerY + k_windowRadius; ++y)
	{
		if (y < 0 || y >= geometry.depth.rows)
			continue;
		const float* depthRow= geometry.depth.ptr<float>(y);
		const uint8_t* maskRow= geometry.mask.ptr<uint8_t>(y);
		for (int x= centerX - k_windowRadius; x <= centerX + k_windowRadius; ++x)
		{
			if (x < 0 || x >= geometry.depth.cols)
				continue;
			if (maskRow[x] != 0 && std::isfinite(depthRow[x]))
				samples.push_back(depthRow[x]);
		}
	}
	if (samples.empty())
		return 0.f;

	std::nth_element(samples.begin(), samples.begin() + samples.size() / 2, samples.end());
	return samples[samples.size() / 2];
}

AppStage_DepthMeshCapture::AppStage_DepthMeshCapture(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, APP_STAGE_NAME)
{
}

AppStage_DepthMeshCapture::~AppStage_DepthMeshCapture() {}

void AppStage_DepthMeshCapture::setSourceCamera(CameraComponentPtr cameraComponent)
{
	m_currentSceneCameraComponent= cameraComponent;
	m_videoSourceComponent= cameraComponent ? cameraComponent->getVideoSourceComponent() : nullptr;
}

void AppStage_DepthMeshCapture::enter()
{
	AppStage::enter();

	m_mkCamera= getFirstViewport()->getCurrentMikanCamera();
	m_mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_currentSceneCameraComponent->getApertureIntrinsics(cameraIntrinsics);
	m_mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);

	// The distortion view is the stream ownership token, same as the other
	// calibration stages.
	m_monoDistortionView= new VideoFrameDistortionView(m_videoSourceComponent, eVideoFrameProcessorMode::CALIBRATION);
	m_monoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);
	// The model consumes the undistorted color buffer - the calibrated FOV it
	// is given describes the undistorted image, not the raw one.
	m_monoDistortionView->setColorUndistortDisabled(false);

	m_videoSourceComponent->startVideoStream(m_monoDistortionView);

	m_capturePanel= addGuiPanel<GuiPanel_DepthMeshCapture>();
	m_capturePanel->OnCaptureEvent= [this]() { onCaptureEvent(); };
	m_capturePanel->OnCancelCaptureEvent= [this]() { onCancelCaptureEvent(); };
	m_capturePanel->OnApplyEvent= [this]() { onApplyEvent(); };
	m_capturePanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_capturePanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_capturePanel->OnOkEvent= [this]() { onCancelEvent(); };

	m_bHasResult= false;

	startCaptureWorker();

	setMenuState(eDepthMeshCaptureMenuState::pendingVideoStartStreamRequest);
}

void AppStage_DepthMeshCapture::exit()
{
	setMenuState(eDepthMeshCaptureMenuState::inactive);

	// Stop the worker before anything else: it cancels any capture in flight,
	// joins, and frees the ONNX session (over a gigabyte) on the thread that
	// created it.
	stopCaptureWorker();

	m_currentSceneCameraComponent= nullptr;
	m_mkCamera= nullptr;

	if (m_monoDistortionView != nullptr)
	{
		if (m_videoSourceComponent)
			m_videoSourceComponent->stopVideoStream(m_monoDistortionView);
		delete m_monoDistortionView;
		m_monoDistortionView= nullptr;
	}

	m_videoSourceComponent= nullptr;

	AppStage::exit();
}

void AppStage_DepthMeshCapture::setMenuState(eDepthMeshCaptureMenuState newState)
{
	if (m_capturePanel != nullptr)
		m_capturePanel->setMenuState(newState);
}

void AppStage_DepthMeshCapture::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	const eDepthMeshCaptureMenuState menuState= m_capturePanel->getMenuState();

	if (menuState == eDepthMeshCaptureMenuState::pendingVideoStartStreamRequest)
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setMenuState(eDepthMeshCaptureMenuState::verifyCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eDepthMeshCaptureMenuState::failedVideoStartStreamRequest);
		}
		return;
	}

	// Keep the undistorted color buffer current while the operator is framing
	// the shot; the capture reads whatever the last processed frame produced.
	if (menuState == eDepthMeshCaptureMenuState::verifyCameraSetup)
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		return;
	}

	if (menuState == eDepthMeshCaptureMenuState::runningInference)
	{
		// Push the worker's progress every frame, then pick the result up once
		// it lands. The elapsed clock is what makes the readout visibly live
		// during the two long, opaque steps.
		m_captureElapsedSeconds+= deltaSeconds;
		m_capturePanel->setCaptureProgress((eDepthMeshCapturePhase)m_capturePhase.load(), m_captureElapsedSeconds,
										   m_bCancelRequested.load());

		bool bCaptureFinished= false;
		{
			std::lock_guard<std::mutex> lock(m_workerMutex);
			bCaptureFinished= m_bCaptureFinished;
		}

		if (bCaptureFinished)
			consumeCaptureOutput();
	}
}

void AppStage_DepthMeshCapture::onGui()
{
	AppStage::onGui();

	constexpr float k_panelWidth= 415.f;
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - k_panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(k_panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##DepthMeshCapture", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_DepthMeshCapture::onCaptureEvent()
{
	// The model is over a gigabyte and is not shipped, so the first capture on
	// a machine offers to fetch it. The dialog re-runs this handler once the
	// download lands, and does nothing at all when the model is already there.
	if (ModalDialog_ModelDownload::requestModels(
			this, {eModelId::moge2}, [this]() { onCaptureEvent(); },
			[this]()
			{
				m_capturePanel->setFailureReason(locText("depthMeshCapture.modelNotInstalled"));
				setMenuState(eDepthMeshCaptureMenuState::failedInference);
			}))
	{
		return;
	}

	// Gather everything the worker needs here: the video buffers and the
	// distortion view the marker detector reads belong to the UI thread.
	m_monoDistortionView->readAndProcessVideoFrame();

	cv::Mat* bgrBuffer= m_monoDistortionView->getBGRUndistortBuffer();
	if (bgrBuffer == nullptr || bgrBuffer->empty())
	{
		m_capturePanel->setFailureReason(locText("depthMeshCapture.noUndistortedFrame"));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return;
	}

	// Metric scale rides directly on the assumed FOV (a wrong guess shifts
	// depth by tens of percent), so the calibrated value is required, not a
	// nicety.
	MikanVideoSourceIntrinsics cameraIntrinsics;
	if (!m_currentSceneCameraComponent->getApertureIntrinsics(cameraIntrinsics))
	{
		m_capturePanel->setFailureReason(locText("depthMeshCapture.cameraIntrinsicsUnresolved"));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return;
	}

	CaptureRequest request;
	// Cloned because the video pipeline reuses the buffer as soon as the next
	// frame arrives, and the worker reads it for seconds afterwards.
	request.bgrFrame= bgrBuffer->clone();
	request.fovXDegrees= (float)cameraIntrinsics.getMonoIntrinsics().hfov;
	request.bHasMarker= tryDetectMarkerCorners(request.markerCornerPixels, request.markerCornerDepths);
	request.storedScaleCorrection= m_currentSceneCameraComponent->getCameraDefinition()->getDepthMeshScaleCorrection();

	// Keep the frame the geometry came from; Create Stencil saves it as the
	// proxy's projected texture. Shares the clone's buffer - both are read-only
	// from here on.
	m_capturedFrame= request.bgrFrame;

	m_bCancelRequested= false;
	m_capturePhase= (int)eDepthMeshCapturePhase::loadingModel;
	m_captureElapsedSeconds= 0.f;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_pendingRequest= std::move(request);
		m_bCaptureRequested= true;
		m_bCaptureFinished= false;
	}
	m_workerSignal.notify_one();

	setMenuState(eDepthMeshCaptureMenuState::runningInference);
}

void AppStage_DepthMeshCapture::onCancelCaptureEvent()
{
	m_bCancelRequested= true;

	// Also tell ONNX Runtime to abandon the run in flight. Without this the
	// cancel would not take effect until the inference finished on its own,
	// which is the step the operator is most likely waiting out.
	std::lock_guard<std::mutex> lock(m_workerMutex);
	if (m_inference)
		m_inference->requestCancel();
}

void AppStage_DepthMeshCapture::startCaptureWorker()
{
	if (m_workerThread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_bWorkerShutdownRequested= false;
		m_bCaptureRequested= false;
		m_bCaptureFinished= false;
	}

	m_workerThread= std::thread([this]() { captureWorkerMain(); });
}

void AppStage_DepthMeshCapture::stopCaptureWorker()
{
	if (!m_workerThread.joinable())
		return;

	// Cancel anything in flight first, otherwise the join waits out a full
	// inference (up to ~10s on the CPU fallback) while the window is closing.
	m_bCancelRequested= true;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		if (m_inference)
			m_inference->requestCancel();
		m_bWorkerShutdownRequested= true;
	}
	m_workerSignal.notify_one();

	m_workerThread.join();
}

void AppStage_DepthMeshCapture::captureWorkerMain()
{
	for (;;)
	{
		CaptureRequest request;
		{
			std::unique_lock<std::mutex> lock(m_workerMutex);
			m_workerSignal.wait(lock, [this]() { return m_bWorkerShutdownRequested || m_bCaptureRequested; });

			if (m_bWorkerShutdownRequested)
			{
				// The ONNX session has to be destroyed on the thread that
				// created and ran it.
				m_inference= nullptr;
				return;
			}

			request= std::move(m_pendingRequest);
			m_bCaptureRequested= false;
		}

		CaptureOutput output;
		runCaptureRequest(request, output);

		{
			std::lock_guard<std::mutex> lock(m_workerMutex);
			m_captureOutput= std::move(output);
			m_bCaptureFinished= true;
		}

		m_capturePhase= (int)eDepthMeshCapturePhase::complete;
	}
}

void AppStage_DepthMeshCapture::runCaptureRequest(const CaptureRequest& request, CaptureOutput& outOutput)
{
	const auto bIsCancelled= [this]() { return m_bCancelRequested.load(); };

	// -- step 1: model load (first capture only) --
	m_capturePhase= (int)eDepthMeshCapturePhase::loadingModel;

	MoGeInference* inference= nullptr;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		inference= m_inference.get();
	}

	if (inference == nullptr)
	{
		const std::filesystem::path modelDirectory= ModelRepository::resolveDirectory(eModelId::moge2);

		MoGeInference::Config config;
		config.modelDirectory= modelDirectory.string();

		// startup() pulls in over a gigabyte, so it runs outside the lock.
		auto newInference= std::make_unique<MoGeInference>();
		if (!newInference->startup(config))
		{
			outOutput.failureReason= locFormat("depthMeshCapture.modelLoadFailedFmt", modelDirectory.string().c_str());
			return;
		}

		std::lock_guard<std::mutex> lock(m_workerMutex);
		inference= newInference.get();
		m_inference= std::move(newInference);
	}
	outOutput.executionProvider= inference->getActiveExecutionProvider();

	if (bIsCancelled())
	{
		outOutput.bCancelled= true;
		return;
	}

	// -- step 2: inference --
	m_capturePhase= (int)eDepthMeshCapturePhase::runningInference;
	if (!inference->run(request.bgrFrame, request.fovXDegrees, outOutput.geometry))
	{
		// A cancelled run fails the same way a broken one does, so the cancel
		// flag is what tells the two apart.
		if (bIsCancelled())
			outOutput.bCancelled= true;
		else
			outOutput.failureReason= locText("depthMeshCapture.inferenceFailed");
		return;
	}

	if (bIsCancelled())
	{
		outOutput.bCancelled= true;
		return;
	}

	// -- step 3: metric scale correction. A marker detected in this capture
	// gives ground truth and wins; otherwise fall back to the factor persisted
	// from a previous marker calibration on this camera.
	m_capturePhase= (int)eDepthMeshCapturePhase::calibratingScale;

	float markerFactor= 1.f, cornerSpread= 0.f;
	if (request.bHasMarker
		&& computeScaleFromMarkerCorners(outOutput.geometry, request.markerCornerPixels, request.markerCornerDepths,
										 markerFactor, cornerSpread))
	{
		outOutput.scaleSource= eDepthScaleCorrectionSource::arucoMarker;
		outOutput.appliedScaleCorrection= markerFactor;
		outOutput.markerCornerSpread= cornerSpread;
	}
	else
	{
		outOutput.scaleSource= (request.storedScaleCorrection != 1.f) ? eDepthScaleCorrectionSource::storedOnCamera
																	  : eDepthScaleCorrectionSource::none;
		outOutput.appliedScaleCorrection= request.storedScaleCorrection;
		outOutput.markerCornerSpread= 0.f;
	}

	if (outOutput.appliedScaleCorrection != 1.f)
	{
		// A uniform scale on camera-space geometry: depth and all three point
		// components. Infinities (invalid pixels) stay infinite; normals are
		// scale-free and untouched.
		outOutput.geometry.depth*= outOutput.appliedScaleCorrection;
		outOutput.geometry.points*= outOutput.appliedScaleCorrection;
	}

	MIKAN_LOG_INFO("AppStage_DepthMeshCapture")
		<< "Scale correction " << outOutput.appliedScaleCorrection << " ("
		<< (outOutput.scaleSource == eDepthScaleCorrectionSource::arucoMarker
				? "aruco marker"
				: (outOutput.scaleSource == eDepthScaleCorrectionSource::storedOnCamera ? "stored on camera" : "none"))
		<< ", corner spread " << outOutput.markerCornerSpread * 100.f << "%)";

	if (bIsCancelled())
	{
		outOutput.bCancelled= true;
		return;
	}

	// -- step 4: mesh generation --
	m_capturePhase= (int)eDepthMeshCapturePhase::generatingMesh;

	DepthMeshGenerator::Config meshConfig;
	if (!DepthMeshGenerator::generateMesh(outOutput.geometry, meshConfig, outOutput.mesh, outOutput.meshStats))
	{
		outOutput.failureReason= locText("depthMeshCapture.meshGenerationFailed");
		return;
	}

	if (bIsCancelled())
	{
		outOutput.bCancelled= true;
		return;
	}

	outOutput.bSucceeded= true;
}

void AppStage_DepthMeshCapture::consumeCaptureOutput()
{
	CaptureOutput output;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		output= std::move(m_captureOutput);
		m_captureOutput= CaptureOutput();
		m_bCaptureFinished= false;
	}

	if (!output.executionProvider.empty())
		m_capturePanel->setExecutionProvider(output.executionProvider);

	m_capturePhase= (int)eDepthMeshCapturePhase::idle;

	if (output.bCancelled)
	{
		// Back to framing rather than out of the tool: cancelling a capture
		// should not throw away the framing or the loaded model.
		MIKAN_LOG_INFO("AppStage_DepthMeshCapture") << "Capture cancelled";
		setMenuState(eDepthMeshCaptureMenuState::verifyCameraSetup);
		return;
	}

	if (!output.bSucceeded)
	{
		m_capturePanel->setFailureReason(output.failureReason);
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return;
	}

	m_geometry= std::move(output.geometry);
	m_mesh= std::move(output.mesh);
	m_meshStats= output.meshStats;
	m_scaleCorrectionSource= output.scaleSource;
	m_appliedScaleCorrection= output.appliedScaleCorrection;
	m_markerCornerSpread= output.markerCornerSpread;
	m_bHasResult= true;

	m_capturePanel->setMeshSummary((int)m_mesh.vertices.size(), (int)m_mesh.getTriangleCount(),
								   m_meshStats.culledDiscontinuityEdges, m_meshStats.nearDepth, m_meshStats.farDepth);
	m_capturePanel->setScaleCorrection(m_scaleCorrectionSource, m_appliedScaleCorrection, m_markerCornerSpread);

	setMenuState(eDepthMeshCaptureMenuState::verifyMesh);
}

bool AppStage_DepthMeshCapture::tryDetectMarkerCorners(std::vector<cv::Point2f>& outCornerPixels,
													   std::vector<float>& outCornerDepths)
{
	auto markerSystem= getSystemOfType<MarkerObjectSystem>();
	if (!markerSystem)
		return false;

	std::vector<int> markerIds;
	markerSystem->getTypedComponentIdList(markerIds);

	for (int markerId : markerIds)
	{
		MarkerComponentPtr markerComponent= markerSystem->getMarkerById(markerId);
		if (!markerComponent)
			continue;
		MarkerDefinitionConstPtr markerDefinition= markerComponent->getMarkerDefinition();
		if (!markerDefinition)
			continue;

		// Detection + solvePnP against the calibrated intrinsics, exactly as
		// the camera-alignment tools do. The transform comes back in Mikan
		// camera space (metres), matching the geometry result's convention.
		CalibrationPatternFinder_Aruco finder(m_currentSceneCameraComponent, m_monoDistortionView, markerDefinition);

		glm::dmat4 apertureToMarkerXform;
		if (!finder.estimateNewCalibrationPatternPose(apertureToMarkerXform))
			continue;

		// The subpixel corner pixels, paired with the marker-local corner
		// geometry the PnP solve used.
		t_opencv_point2d_list imagePoints;
		t_opencv_pointID_list imagePointIds;
		cv::Point2f boundingQuad[4];
		if (!finder.fetchLastFoundCalibrationPattern(imagePoints, imagePointIds, boundingQuad))
			continue;

		OpenGLCalibrationGeometry markerLocalGeometry;
		finder.getOpenGLSolvePnPGeometry(&markerLocalGeometry);
		if (imagePoints.size() != markerLocalGeometry.points.size())
			continue;

		outCornerPixels.clear();
		outCornerDepths.clear();
		for (size_t cornerIndex= 0; cornerIndex < imagePoints.size(); ++cornerIndex)
		{
			const glm::vec3& localPoint= markerLocalGeometry.points[cornerIndex];
			const glm::dvec4 cornerCameraSpace=
				apertureToMarkerXform * glm::dvec4(localPoint.x, localPoint.y, localPoint.z, 1.0);
			const float trueDepth= (float)-cornerCameraSpace.z;
			if (trueDepth <= 0.f)
				continue;

			outCornerPixels.push_back(imagePoints[cornerIndex]);
			outCornerDepths.push_back(trueDepth);
		}

		if (outCornerPixels.empty())
			continue;

		MIKAN_LOG_INFO("AppStage_DepthMeshCapture") << "Detected marker " << markerId << " for scale calibration ("
													<< outCornerPixels.size() << " usable corners)";

		return true;
	}

	return false;
}

bool AppStage_DepthMeshCapture::computeScaleFromMarkerCorners(const MoGeInference::Result& geometry,
															  const std::vector<cv::Point2f>& cornerPixels,
															  const std::vector<float>& cornerDepths, float& outFactor,
															  float& outCornerSpread)
{
	if (cornerPixels.size() != cornerDepths.size())
		return false;

	std::vector<float> ratios;
	for (size_t cornerIndex= 0; cornerIndex < cornerPixels.size(); ++cornerIndex)
	{
		const float sampledDepth= sampleMedianDepth(geometry, cornerPixels[cornerIndex].x, cornerPixels[cornerIndex].y);
		if (sampledDepth <= 0.f)
			continue;

		ratios.push_back(cornerDepths[cornerIndex] / sampledDepth);
	}

	// Require at least 3 of the 4 corners: one bad corner (grazing angle,
	// depth-edge contamination) should not silently define the factor.
	if (ratios.size() < 3)
		return false;

	std::sort(ratios.begin(), ratios.end());
	const float factor= ratios[ratios.size() / 2];

	float spread= 0.f;
	for (float ratio : ratios)
		spread= std::max(spread, std::fabs(ratio / factor - 1.f));

	MIKAN_LOG_INFO("AppStage_DepthMeshCapture") << "Marker scale calibration: factor " << factor << " from "
												<< ratios.size() << " corners, spread " << spread * 100.f << "%";

	outFactor= factor;
	outCornerSpread= spread;
	return true;
}

bool AppStage_DepthMeshCapture::createStencilFromMesh()
{
	if (!m_bHasResult)
		return false;

	// The mesh vertices are camera-local, so the stencil's world transform is
	// simply the capturing camera's pose. An untracked camera would place the
	// proxy at a stale or default pose, so fail loudly instead.
	glm::mat4 cameraPose(1.f);
	if (!m_currentSceneCameraComponent->getWorldSpaceAperturePose(cameraPose))
	{
		m_capturePanel->setFailureReason(locText("depthMeshCapture.cameraPoseUnresolved"));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return false;
	}

	const std::filesystem::path projectDirectory= PathUtils::getProjectDirectory();
	if (projectDirectory.empty())
	{
		m_capturePanel->setFailureReason(locText("depthMeshCapture.noProjectLoaded"));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return false;
	}

	// Unique per capture: regenerating under a reused path would collide with
	// the model resource cache, which keys on the file path.
	std::time_t now= std::time(nullptr);
	std::tm timeInfo;
	localtime_s(&timeInfo, &now);
	char stencilName[64];
	std::strftime(stencilName, sizeof(stencilName), "DepthProxy_%Y%m%d_%H%M%S", &timeInfo);

	const std::filesystem::path modelsDirectory= projectDirectory / "models";
	std::filesystem::create_directories(modelsDirectory);
	const std::filesystem::path objPath= modelsDirectory / (std::string(stencilName) + ".obj");

	// The captured frame becomes the proxy's projected texture: the mesh's UVs
	// are the frame's pixel coordinates, so the plate maps back onto the
	// geometry exactly. Written before the obj so the mtl never dangles.
	std::string textureFileName;
	if (!m_capturedFrame.empty())
	{
		textureFileName= std::string(stencilName) + ".png";
		const std::filesystem::path texturePath= modelsDirectory / textureFileName;
		if (!cv::imwrite(texturePath.string(), m_capturedFrame))
		{
			MIKAN_LOG_WARNING("AppStage_DepthMeshCapture")
				<< "Failed to write capture texture " << texturePath.string() << "; stencil will be untextured";
			textureFileName.clear();
		}
	}

	if (!DepthMeshGenerator::saveObj(m_mesh, objPath.string(), stencilName, textureFileName))
	{
		m_capturePanel->setFailureReason(locFormat("depthMeshCapture.meshWriteFailedFmt", objPath.string().c_str()));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return false;
	}

	// Parent the stencil under the active scene so it lands in that scene's
	// subtree in the project outliner. With no active scene it falls back to
	// the camera's stage, which files it in the outliner's unparented tray.
	MikanTransformID parentTransformId= INVALID_MIKAN_ID;
	auto sceneSystem= getSystemOfType<SceneObjectSystem>();
	SceneComponentPtr currentScene= sceneSystem ? sceneSystem->getCurrentScene() : nullptr;
	if (currentScene)
	{
		parentTransformId= currentScene->getSceneId();
	}
	else
	{
		StageComponentConstPtr stageComponent= m_currentSceneCameraComponent->getOwnerStageComponent();
		parentTransformId= stageComponent ? stageComponent->getComponentId() : INVALID_MIKAN_ID;
	}

	// The stencil stores an absolute model path - the importer loads the path
	// verbatim, so a relative one would silently fail to resolve.
	auto modelStencilSystem= getSystemOfType<ModelStencilSystem>();
	ModelStencilComponentPtr stencilComponent= modelStencilSystem->addNewObjectByTypedDefinition(
		[&](ModelStencilDefinitionPtr definition)
		{
			definition->setComponentName(stencilName);
			definition->setParentTransformId(parentTransformId);
			definition->setRelativeTransform(GlmTransform());
			definition->setIsDisabled(false);
			definition->setModelPath(objPath);
			return true;
		});
	if (!stencilComponent)
	{
		m_capturePanel->setFailureReason(locText("depthMeshCapture.stencilCreateFailed"));
		setMenuState(eDepthMeshCaptureMenuState::failedInference);
		return false;
	}

	stencilComponent->setWorldTransform(cameraPose);

	// Committing a marker-calibrated capture persists the factor on the
	// camera, so marker-less captures from this camera reuse it.
	if (m_scaleCorrectionSource == eDepthScaleCorrectionSource::arucoMarker)
	{
		m_currentSceneCameraComponent->getCameraDefinition()->setDepthMeshScaleCorrection(m_appliedScaleCorrection);
		MIKAN_LOG_INFO("AppStage_DepthMeshCapture")
			<< "Persisted depth scale correction " << m_appliedScaleCorrection << " on camera '"
			<< m_currentSceneCameraComponent->getName() << "'";
	}

	m_capturePanel->setCreatedStencilName(stencilName);

	MIKAN_LOG_INFO("AppStage_DepthMeshCapture")
		<< "Created depth proxy stencil '" << stencilName << "' (" << m_mesh.getTriangleCount() << " triangles, "
		<< objPath.string() << ")";

	return true;
}

void AppStage_DepthMeshCapture::onApplyEvent()
{
	if (createStencilFromMesh())
		setMenuState(eDepthMeshCaptureMenuState::captureComplete);
}

void AppStage_DepthMeshCapture::onRedoEvent()
{
	m_bHasResult= false;
	setMenuState(eDepthMeshCaptureMenuState::verifyCameraSetup);
}

void AppStage_DepthMeshCapture::onCancelEvent() { getOwnerWindow()->popAppState(); }

void AppStage_DepthMeshCapture::renderDepthPreview()
{
	if (!m_bHasResult)
		return;

	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	IMkLineRenderer* lineRenderer= graphicsContext->getLineRenderer();
	IMkViewportPtr renderingViewport= graphicsContext->getRenderingViewport();
	glm::i32vec2 viewportOrigin, viewportSize;
	if (lineRenderer == nullptr || renderingViewport == nullptr
		|| !renderingViewport->getRenderingViewport(viewportOrigin, viewportSize))
	{
		return;
	}

	const float frameWidth= (float)m_monoDistortionView->getFrameWidth();
	const float frameHeight= (float)m_monoDistortionView->getFrameHeight();
	const float viewportX0= (float)viewportOrigin.x;
	const float viewportY0= (float)viewportOrigin.y;
	const float viewportX1= (float)viewportOrigin.x + (float)viewportSize.x - 1.f;
	const float viewportY1= (float)viewportOrigin.y + (float)viewportSize.y - 1.f;

	// Colored by normalized inverse depth (red = near, blue = far), matching
	// the panel text. Inverse depth spreads the resolution toward the near
	// field, which is where the proxy has to be right.
	const float nearInverse= 1.f / std::max(m_meshStats.nearDepth, 1e-3f);
	const float farInverse= 1.f / std::max(m_meshStats.farDepth, 1e-3f);
	const float inverseRange= std::max(nearInverse - farInverse, 1e-6f);

	// A stride independent of the mesh stride: the preview needs readability,
	// not density.
	constexpr int k_previewStride= 6;
	for (int y= 0; y < m_geometry.depth.rows; y+= k_previewStride)
	{
		const float* depthRow= m_geometry.depth.ptr<float>(y);
		const uint8_t* maskRow= m_geometry.mask.ptr<uint8_t>(y);
		for (int x= 0; x < m_geometry.depth.cols; x+= k_previewStride)
		{
			if (maskRow[x] == 0 || !std::isfinite(depthRow[x]))
				continue;

			const float normalized= (1.f / depthRow[x] - farInverse) / inverseRange;
			const glm::vec3 color(normalized, 0.25f, 1.f - normalized);

			const glm::vec2 framePoint((float)x + 0.5f, (float)y + 0.5f);
			const glm::vec2 windowPoint= remapPointIntoTarget(frameWidth, frameHeight, viewportX0, viewportY0,
															  viewportX1, viewportY1, framePoint);
			lineRenderer->addPoint2d(windowPoint, color, 2.f);
		}
	}
}

void AppStage_DepthMeshCapture::render(IMkViewportPtr targetViewport)
{
	switch (m_capturePanel->getMenuState())
	{
	case eDepthMeshCaptureMenuState::verifyCameraSetup:
	case eDepthMeshCaptureMenuState::runningInference:
		m_monoDistortionView->renderSelectedVideoBuffers();
		break;

	case eDepthMeshCaptureMenuState::verifyMesh:
	case eDepthMeshCaptureMenuState::captureComplete:
		m_monoDistortionView->renderSelectedVideoBuffers();
		renderDepthPreview();
		break;

	default:
		break;
	}

	getGraphicsContext()->getLineRenderer()->render(true);
}
