#include "SceneLightingCapture/AppStage_SceneLightingCapture.h"
#include "SceneLightingCapture/GuiPanel_SceneLightingCapture.h"

#include "imgui.h"
#include "MkGuiScopedWindow.h"

#include "CalibrationRenderHelpers.h"
#include "CameraComponent.h"
#include "Colors.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkLineRenderer.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "IMkViewport.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "LightEnvironmentComponent.h"
#include "LocText.h"
#include "Logger.h"
#include "ModalModelDownload/ModalDialog_ModelDownload.h"

#include "MikanCamera.h"
#include "MikanViewport.h"

#include "ModelRepository.h"
#include "PathUtils.h"
#include "StringUtils.h"
#include "TransformComponent.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include <opencv2/opencv.hpp>

#include <filesystem>

const char* AppStage_SceneLightingCapture::APP_STAGE_NAME= "SceneLightingCapture";

/// Where each step sits in the progress bar, weighted by how long it actually
/// takes. The diffusion decomposition dominates and is the only step that
/// reports sub-progress - it is a sequence of discrete VAE and denoise calls
/// rather than one opaque Run - so it gets the widest band and moves smoothly
/// through it. The others can only jump.
static float computeEstimateProgressFraction(eSceneLightingEstimatePhase phase, int completedUnits, int totalUnits)
{
	switch (phase)
	{
	case eSceneLightingEstimatePhase::loadingModels:
		return 0.03f;
	case eSceneLightingEstimatePhase::decomposingShading:
	{
		constexpr float k_bandStart= 0.15f;
		constexpr float k_bandEnd= 0.80f;
		const float unitFraction= (totalUnits > 0) ? (float)completedUnits / (float)totalUnits : 0.f;
		return k_bandStart + unitFraction * (k_bandEnd - k_bandStart);
	}
	case eSceneLightingEstimatePhase::estimatingGeometry:
		return 0.85f;
	case eSceneLightingEstimatePhase::fittingLighting:
		return 0.96f;
	case eSceneLightingEstimatePhase::complete:
		return 1.f;
	default:
		return 0.f;
	}
}

AppStage_SceneLightingCapture::AppStage_SceneLightingCapture(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, APP_STAGE_NAME)
{
}

AppStage_SceneLightingCapture::~AppStage_SceneLightingCapture() {}

void AppStage_SceneLightingCapture::setTargetProbe(LightEnvironmentComponentPtr targetProbe)
{
	m_targetProbe= targetProbe;
}

void AppStage_SceneLightingCapture::setSourceCamera(CameraComponentPtr cameraComponent)
{
	m_currentSceneCameraComponent= cameraComponent;
	m_videoSourceComponent= cameraComponent ? cameraComponent->getVideoSourceComponent() : nullptr;
}

void AppStage_SceneLightingCapture::enter()
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
	// The fit consumes the undistorted color buffer, so color undistortion must
	// be on regardless of what the rest of the app left it set to.
	m_monoDistortionView->setColorUndistortDisabled(false);

	m_videoSourceComponent->startVideoStream(m_monoDistortionView);

	m_capturePanel= addGuiPanel<GuiPanel_SceneLightingCapture>();
	m_capturePanel->setProbeName(m_targetProbe ? m_targetProbe->getName()
											   : std::string(locText("sceneLightingCapture.noProbe")));
	m_capturePanel->OnCaptureEvent= [this]() { onCaptureEvent(); };
	m_capturePanel->OnCancelCaptureEvent= [this]() { onCancelCaptureEvent(); };
	m_capturePanel->OnApplyEvent= [this]() { onApplyEvent(); };
	m_capturePanel->OnRedoEvent= [this]() { onRedoEvent(); };
	m_capturePanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_capturePanel->OnOkEvent= [this]() { onCancelEvent(); };

	m_bHasResult= false;

	// Fullscreen quad the verification preview is drawn with. vFlipped matches
	// the video quad so the preview lines up with the plate underneath it.
	m_previewQuad= createFullscreenQuadMesh(getGraphicsContext(), true);
	m_builtPreviewMode= eLightingPreviewMode::COUNT;

	startEstimateWorker();

	setMenuState(eSceneLightingCaptureMenuState::pendingVideoStartStreamRequest);
}

void AppStage_SceneLightingCapture::exit()
{
	setMenuState(eSceneLightingCaptureMenuState::inactive);

	// Stop the worker before anything else: it cancels any estimate in flight,
	// joins, and frees the ONNX sessions (gigabytes) on the thread that created
	// them.
	stopEstimateWorker();

	m_previewQuad= nullptr;
	m_previewTexture= nullptr;
	m_previewImage.release();
	m_builtPreviewMode= eLightingPreviewMode::COUNT;

	m_currentSceneCameraComponent= nullptr;
	m_mkCamera= nullptr;
	m_targetProbe= nullptr;

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

void AppStage_SceneLightingCapture::setMenuState(eSceneLightingCaptureMenuState newState)
{
	if (m_capturePanel != nullptr)
		m_capturePanel->setMenuState(newState);
}

void AppStage_SceneLightingCapture::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	const eSceneLightingCaptureMenuState menuState= m_capturePanel->getMenuState();

	if (menuState == eSceneLightingCaptureMenuState::pendingVideoStartStreamRequest)
	{
		if (m_monoDistortionView->isReceivingFrames())
		{
			setMenuState(eSceneLightingCaptureMenuState::verifyCameraSetup);
		}
		else if (m_videoSourceComponent->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eSceneLightingCaptureMenuState::failedVideoStartStreamRequest);
		}
		return;
	}

	// Keep the undistorted color buffer current while the operator is framing
	// the shot; the capture reads whatever the last processed frame produced.
	if (menuState == eSceneLightingCaptureMenuState::verifyCameraSetup)
	{
		m_monoDistortionView->readAndProcessVideoFrame();
		return;
	}

	if (menuState == eSceneLightingCaptureMenuState::runningInference)
	{
		// Push the worker's progress every frame, then pick the result up once
		// it lands.
		m_estimateElapsedSeconds+= deltaSeconds;

		const eSceneLightingEstimatePhase phase= (eSceneLightingEstimatePhase)m_estimatePhase.load();
		const float fraction=
			computeEstimateProgressFraction(phase, m_estimateUnitsCompleted.load(), m_estimateUnitsTotal.load());
		m_capturePanel->setEstimateProgress(phase, fraction, m_estimateElapsedSeconds, m_bCancelRequested.load());

		bool bEstimateFinished= false;
		{
			std::lock_guard<std::mutex> lock(m_workerMutex);
			bEstimateFinished= m_bEstimateFinished;
		}

		if (bEstimateFinished)
			consumeEstimateOutput();
	}
}

void AppStage_SceneLightingCapture::onGui()
{
	AppStage::onGui();

	constexpr float k_panelWidth= 415.f;
	const float displayWidth= m_ownerWindow->getWidth();
	const float displayHeight= m_ownerWindow->getHeight();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - k_panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(k_panelWidth, displayHeight), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##SceneLightingCapture", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_SceneLightingCapture::onCaptureEvent()
{
	// Neither model ships with the editor, and this capture needs both: the
	// shading from Marigold and the normals from MoGe-2. The dialog re-runs
	// this handler once whatever was missing has been downloaded.
	if (ModalDialog_ModelDownload::requestModels(
			this, {eModelId::marigold, eModelId::moge2}, [this]() { onCaptureEvent(); },
			[this]()
			{
				m_capturePanel->setFailureReason(locText("sceneLightingCapture.modelNotInstalled"));
				setMenuState(eSceneLightingCaptureMenuState::failedInference);
			}))
	{
		return;
	}

	// Gather everything the worker needs here: the video buffers and the
	// tracked camera pose belong to the UI thread.
	m_monoDistortionView->readAndProcessVideoFrame();

	cv::Mat* bgrBuffer= m_monoDistortionView->getBGRUndistortBuffer();
	if (bgrBuffer == nullptr || bgrBuffer->empty())
	{
		m_capturePanel->setFailureReason(locText("sceneLightingCapture.noUndistortedFrame"));
		setMenuState(eSceneLightingCaptureMenuState::failedInference);
		return;
	}

	// The models return camera-space normals, so the fit needs the camera's
	// orientation to place the recovered environment in stage space. Stage space
	// and not world space: the estimate belongs to the stage it was captured on,
	// and a client anchors it at that stage wherever it places it.
	glm::mat4 cameraPose(1.f);
	if (!m_currentSceneCameraComponent->getStageSpaceAperturePose(cameraPose))
	{
		m_capturePanel->setFailureReason(locText("sceneLightingCapture.cameraPoseUnresolved"));
		setMenuState(eSceneLightingCaptureMenuState::failedInference);
		return;
	}

	// The calibrated FOV only affects MoGe-2's metric depth recovery, not the
	// normals the fit consumes, but the real value is available here so pass it.
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_currentSceneCameraComponent->getApertureIntrinsics(cameraIntrinsics);

	EstimateRequest request;
	// Cloned because the video pipeline reuses the buffer as soon as the next
	// frame arrives, and the worker reads it for seconds afterwards.
	request.bgrFrame= bgrBuffer->clone();
	request.cameraToStageRotation= glm::mat3(cameraPose);
	request.fovXDegrees= (float)cameraIntrinsics.getMonoIntrinsics().hfov;

	m_bCancelRequested= false;
	m_estimatePhase= (int)eSceneLightingEstimatePhase::loadingModels;
	m_estimateUnitsCompleted= 0;
	m_estimateUnitsTotal= 0;
	m_estimateElapsedSeconds= 0.f;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_pendingRequest= std::move(request);
		m_bEstimateRequested= true;
		m_bEstimateFinished= false;
	}
	m_workerSignal.notify_one();

	setMenuState(eSceneLightingCaptureMenuState::runningInference);
}

void AppStage_SceneLightingCapture::onCancelCaptureEvent()
{
	m_bCancelRequested= true;

	// Also tell ONNX Runtime to abandon whichever run is in flight. Without
	// this the cancel would not land until the current denoise step finished,
	// and on the CPU fallback a single step is most of the wait.
	std::lock_guard<std::mutex> lock(m_workerMutex);
	if (m_estimator)
		m_estimator->requestCancel();
}

void AppStage_SceneLightingCapture::startEstimateWorker()
{
	if (m_workerThread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		m_bWorkerShutdownRequested= false;
		m_bEstimateRequested= false;
		m_bEstimateFinished= false;
	}

	m_workerThread= std::thread([this]() { estimateWorkerMain(); });
}

void AppStage_SceneLightingCapture::stopEstimateWorker()
{
	if (!m_workerThread.joinable())
		return;

	// Cancel anything in flight first, otherwise the join waits out a full
	// estimate while the window is closing.
	m_bCancelRequested= true;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		if (m_estimator)
			m_estimator->requestCancel();
		m_bWorkerShutdownRequested= true;
	}
	m_workerSignal.notify_one();

	m_workerThread.join();
}

void AppStage_SceneLightingCapture::estimateWorkerMain()
{
	for (;;)
	{
		EstimateRequest request;
		{
			std::unique_lock<std::mutex> lock(m_workerMutex);
			m_workerSignal.wait(lock, [this]() { return m_bWorkerShutdownRequested || m_bEstimateRequested; });

			if (m_bWorkerShutdownRequested)
			{
				// The ONNX sessions have to be destroyed on the thread that
				// created and ran them.
				m_estimator= nullptr;
				return;
			}

			request= std::move(m_pendingRequest);
			m_bEstimateRequested= false;
		}

		EstimateOutput output;
		runEstimateRequest(request, output);

		{
			std::lock_guard<std::mutex> lock(m_workerMutex);
			m_estimateOutput= std::move(output);
			m_bEstimateFinished= true;
		}

		m_estimatePhase= (int)eSceneLightingEstimatePhase::complete;
	}
}

void AppStage_SceneLightingCapture::runEstimateRequest(const EstimateRequest& request, EstimateOutput& outOutput)
{
	const auto bIsCancelled= [this]() { return m_bCancelRequested.load(); };

	// -- step 1: model load (first capture only) --
	m_estimatePhase= (int)eSceneLightingEstimatePhase::loadingModels;

	SceneLightingEstimator* estimator= nullptr;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		estimator= m_estimator.get();
	}

	if (estimator == nullptr)
	{
		const std::filesystem::path modelDirectory= ModelRepository::resolveDirectory(eModelId::marigold);
		const std::filesystem::path mogeModelDirectory= ModelRepository::resolveDirectory(eModelId::moge2);

		SceneLightingEstimator::Config config;
		config.modelDirectory= modelDirectory.string();
		config.mogeModelDirectory= mogeModelDirectory.string();

		// startup() pulls in several gigabytes, so it runs outside the lock.
		auto newEstimator= std::make_unique<SceneLightingEstimator>();
		if (!newEstimator->startup(config))
		{
			outOutput.failureReason= locFormat("sceneLightingCapture.modelLoadFailedFmt",
											   modelDirectory.string().c_str(), mogeModelDirectory.string().c_str());
			return;
		}

		std::lock_guard<std::mutex> lock(m_workerMutex);
		estimator= newEstimator.get();
		m_estimator= std::move(newEstimator);
	}
	outOutput.executionProvider= estimator->getActiveExecutionProvider();

	if (bIsCancelled())
	{
		outOutput.bCancelled= true;
		return;
	}

	// -- steps 2-4: the estimate reports its own phases as it goes --
	SceneLightingEstimator::Progress progress;
	progress.onProgress= [this](eSceneLightingEstimatePhase phase, int completedUnits, int totalUnits)
	{
		m_estimatePhase= (int)phase;
		m_estimateUnitsCompleted= completedUnits;
		m_estimateUnitsTotal= totalUnits;
	};
	progress.isCancelled= [this]() { return m_bCancelRequested.load(); };

	if (!estimator->estimate(request.bgrFrame, request.cameraToStageRotation, request.fovXDegrees, outOutput.result,
							 progress))
	{
		// A cancelled estimate fails the same way a broken one does, so the
		// cancel flag is what tells the two apart.
		if (bIsCancelled())
			outOutput.bCancelled= true;
		else
			outOutput.failureReason= locText("sceneLightingCapture.estimationFailedSeeLog");
		return;
	}

	outOutput.bSucceeded= true;
}

void AppStage_SceneLightingCapture::consumeEstimateOutput()
{
	EstimateOutput output;
	{
		std::lock_guard<std::mutex> lock(m_workerMutex);
		output= std::move(m_estimateOutput);
		m_estimateOutput= EstimateOutput();
		m_bEstimateFinished= false;
	}

	if (!output.executionProvider.empty())
		m_capturePanel->setExecutionProvider(output.executionProvider);

	m_estimatePhase= (int)eSceneLightingEstimatePhase::idle;

	if (output.bCancelled)
	{
		// Back to framing rather than out of the tool: cancelling should not
		// throw away the framing or the loaded models.
		MIKAN_LOG_INFO("AppStage_SceneLightingCapture") << "Lighting estimate cancelled";
		setMenuState(eSceneLightingCaptureMenuState::verifyCameraSetup);
		return;
	}

	if (!output.bSucceeded)
	{
		m_capturePanel->setFailureReason(output.failureReason);
		setMenuState(eSceneLightingCaptureMenuState::failedInference);
		return;
	}

	m_result= std::move(output.result);
	m_bHasResult= true;
	// New model outputs, so whatever preview was built is stale.
	m_builtPreviewMode= eLightingPreviewMode::COUNT;

	const glm::vec3 ambient= m_result.environment.coefficients[0] * 0.282095f * 3.14159265f;
	m_capturePanel->setEstimateSummary(m_result.directionality,
									   StringUtils::stringify(m_result.keyLightDirection.x, ", ",
															  m_result.keyLightDirection.y, ", ",
															  m_result.keyLightDirection.z),
									   StringUtils::stringify(ambient.r, ", ", ambient.g, ", ", ambient.b),
									   m_result.sampleCount, m_result.negativeSolidAngleFraction);

	setMenuState(eSceneLightingCaptureMenuState::verifyEstimate);
}

void AppStage_SceneLightingCapture::applyEstimate()
{
	if (!m_bHasResult || !m_targetProbe)
		return;

	LightEnvironmentDefinitionPtr definition= m_targetProbe->getLightEnvironmentDefinition();
	if (!definition)
		return;

	definition->setLightingEnvironment(m_result.environment);

	MIKAN_LOG_INFO("AppStage_SceneLightingCapture")
		<< "Applied estimated lighting to probe '" << m_targetProbe->getName() << "' (directionality "
		<< m_result.directionality << ")";
}

void AppStage_SceneLightingCapture::onApplyEvent()
{
	applyEstimate();
	setMenuState(eSceneLightingCaptureMenuState::captureComplete);
}

void AppStage_SceneLightingCapture::onRedoEvent()
{
	m_bHasResult= false;
	setMenuState(eSceneLightingCaptureMenuState::verifyCameraSetup);
}

void AppStage_SceneLightingCapture::onCancelEvent() { getOwnerWindow()->popAppState(); }

void AppStage_SceneLightingCapture::buildLightingPreviewImage(eLightingPreviewMode mode)
{
	if (mode == m_builtPreviewMode || mode == eLightingPreviewMode::litSphere)
		return;

	SceneLightingEstimator::eReconstructionView view= SceneLightingEstimator::eReconstructionView::lighting;
	switch (mode)
	{
	case eLightingPreviewMode::relitScene:
		view= SceneLightingEstimator::eReconstructionView::relit;
		break;
	case eLightingPreviewMode::modelShading:
		view= SceneLightingEstimator::eReconstructionView::modelShading;
		break;
	default:
		break;
	}

	// The reconstruction lives with the fit rather than here, so the headless
	// -estimateLighting -dump path renders exactly what this panel shows.
	m_previewImage= SceneLightingEstimator::renderReconstructionImage(m_result, view);
	if (m_previewImage.empty())
		return;

	const int width= m_previewImage.cols;
	const int height= m_previewImage.rows;

	if (m_previewTexture == nullptr)
	{
		m_previewTexture= CreateMkTexture((uint16_t)width, (uint16_t)height, nullptr,
										  MK_RGB,  // texture format
										  MK_BGR); // buffer format
		m_previewTexture->setGenerateMipMap(false);
		if (!m_previewTexture->createTexture())
		{
			MIKAN_LOG_ERROR("AppStage_SceneLightingCapture") << "Failed to create the lighting preview texture";
			m_previewTexture= nullptr;
			return;
		}
	}

	m_previewTexture->copyBufferIntoTexture(m_previewImage.data, m_previewImage.step[0] * m_previewImage.rows);
	m_builtPreviewMode= mode;
}

void AppStage_SceneLightingCapture::renderLightingPreviewImage()
{
	if (!m_bHasResult)
		return;

	const eLightingPreviewMode mode= m_capturePanel->getPreviewMode();
	buildLightingPreviewImage(mode);

	if (m_previewQuad == nullptr || m_previewTexture == nullptr || m_builtPreviewMode != mode)
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

void AppStage_SceneLightingCapture::renderLitSpherePreview()
{
	if (!m_bHasResult)
		return;

	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	const float frameWidth= (float)m_monoDistortionView->getFrameWidth();
	const float frameHeight= (float)m_monoDistortionView->getFrameHeight();

	// A disc of points in the corner of the video frame, each colored by the
	// irradiance the recovered environment delivers to that surface normal.
	// This is the same construction as the lit sphere in
	// tools/sh_lighting_fit.py, so the two can be compared directly.
	const float radius= fminf(frameWidth, frameHeight) * 0.18f;
	const glm::vec2 center(frameWidth / 2.f, frameHeight / 2.f);
	const int steps= 48;

	// Normalize against the brightest sample so the preview is readable
	// regardless of the scene's absolute exposure, which is only recovered up
	// to a global scale anyway.
	float peak= 1e-6f;
	for (int y= -steps; y <= steps; ++y)
	{
		for (int x= -steps; x <= steps; ++x)
		{
			const float nx= (float)x / (float)steps;
			const float ny= (float)y / (float)steps;
			const float r2= nx * nx + ny * ny;
			if (r2 > 1.f)
				continue;

			const glm::vec3 normal(nx, ny, sqrtf(fmaxf(0.f, 1.f - r2)));
			const glm::vec3 irradiance= m_result.cameraSpaceEnvironment.evalIrradiance(normal);
			peak= fmaxf(peak, fmaxf(irradiance.r, fmaxf(irradiance.g, irradiance.b)));
		}
	}

	// Resolve the line renderer and viewport bounds ONCE. drawPointList2d does
	// this internally per call, so calling it per point would repeat the
	// viewport lookup several thousand times for what the renderer batches
	// anyway. The bounds convention here matches that helper exactly.
	IMkLineRenderer* lineRenderer= graphicsContext->getLineRenderer();
	IMkViewportPtr renderingViewport= graphicsContext->getRenderingViewport();
	glm::i32vec2 viewportOrigin, viewportSize;
	if (lineRenderer == nullptr || renderingViewport == nullptr
		|| !renderingViewport->getRenderingViewport(viewportOrigin, viewportSize))
	{
		return;
	}

	const float viewportX0= (float)viewportOrigin.x;
	const float viewportY0= (float)viewportOrigin.y;
	const float viewportX1= (float)viewportOrigin.x + (float)viewportSize.x - 1.f;
	const float viewportY1= (float)viewportOrigin.y + (float)viewportSize.y - 1.f;

	for (int y= -steps; y <= steps; ++y)
	{
		for (int x= -steps; x <= steps; ++x)
		{
			const float nx= (float)x / (float)steps;
			const float ny= (float)y / (float)steps;
			const float r2= nx * nx + ny * ny;
			if (r2 > 1.f)
				continue;

			const glm::vec3 normal(nx, ny, sqrtf(fmaxf(0.f, 1.f - r2)));

			// Preview in CAMERA space: it is being compared against the video
			// frame, which is also camera space.
			glm::vec3 color= m_result.cameraSpaceEnvironment.evalIrradiance(normal) / peak;
			// Order-2 SH can evaluate negative; clamp exactly as the renderer does.
			color= glm::vec3(fmaxf(color.r, 0.f), fmaxf(color.g, 0.f), fmaxf(color.b, 0.f));

			const glm::vec2 framePoint(center.x + nx * radius, center.y - ny * radius);
			const glm::vec2 windowPoint= remapPointIntoTarget(frameWidth, frameHeight, viewportX0, viewportY0,
															  viewportX1, viewportY1, framePoint);

			lineRenderer->addPoint2d(windowPoint, color, 3.f);
		}
	}
}

void AppStage_SceneLightingCapture::render(IMkViewportPtr targetViewport)
{
	switch (m_capturePanel->getMenuState())
	{
	case eSceneLightingCaptureMenuState::verifyCameraSetup:
	case eSceneLightingCaptureMenuState::runningInference:
		m_monoDistortionView->renderSelectedVideoBuffers();
		break;

	case eSceneLightingCaptureMenuState::verifyEstimate:
	case eSceneLightingCaptureMenuState::captureComplete:
		// The plate is drawn first either way, so a preview that has not been
		// built yet leaves the frame visible rather than a black screen.
		m_monoDistortionView->renderSelectedVideoBuffers();
		if (m_capturePanel->getPreviewMode() == eLightingPreviewMode::litSphere)
			renderLitSpherePreview();
		else
			renderLightingPreviewImage();
		break;

	default:
		break;
	}

	getGraphicsContext()->getLineRenderer()->render(true);
}
