//-- includes -----
#include "AlignCameraByOriginMarker/AppStage_AlignCameraByOriginMarker.h"
#include "AlignCameraByOriginMarker/GuiPanel_AlignCameraByOriginMarker.h"
#include "LocText.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ArucoMarkerPoseSampler.h"
#include "CalibrationRenderHelpers.h"
#include "CameraComponent.h"
#include "IAlignmentReferenceSource.h"
#include "IFrameCoupledPoseProvider.h"
#include "Colors.h"
#include "IMkFrameBuffer.h"
#include "IMkLineRenderer.h"
#include "IMkTriangulatedMesh.h"
#include "IMkTexture.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MathGLM.h"
#include "MathTypeConversion.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanViewport.h"
#include "MkGuiScopedWindow.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "StageComponent.h"
#include "TextStyle.h"
#include "VideoDisplayConstants.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

#include "glm/gtc/quaternion.hpp"

#include "imgui.h"
#include "MkGuiDrawUtils.h"

//-- statics ----
const char* AppStage_AlignCameraByOriginMarker::APP_STAGE_NAME= "AlignCameraByOriginMarker";

//-- public methods -----
AppStage_AlignCameraByOriginMarker::AppStage_AlignCameraByOriginMarker(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_AlignCameraByOriginMarker::APP_STAGE_NAME)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_AlignCameraByOriginMarker::~AppStage_AlignCameraByOriginMarker() {}

bool AppStage_AlignCameraByOriginMarker::tryEnterCalibration(AppStage* fromAppStage,
															 CameraComponentPtr targetCameraComponent)
{
	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();

	// 1. Target camera must have a valid video source with valid intrinsics
	VideoSourceComponentPtr videoSource= targetCameraComponent->getVideoSourceComponent();
	if (!videoSource)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignCameraByOriginMarker.noVideoSourceError"));
		return false;
	}

	if (!videoSource->areCameraIntrinsicsValid())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByOriginMarker.invalidIntrinsicsError"));
		return false;
	}

	// 2. Target camera must NOT have a tracking mount. A mounted camera is placed
	//    by its tracker instead, so it uses the VR alignment flow. Both a fixed
	//    camera and a frame-coupled one (an ARKit phone, whose pose arrives with
	//    each video frame) belong here; they differ only in where the result is
	//    stored, see computeAndApplyTargetTransform.
	if (targetCameraComponent->hasValidTrackingMountComponent())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByOriginMarker.alreadyHasTrackingMountError"));
		return false;
	}

	// 3. Owner stage must have a marker tracking volume
	StageComponentConstPtr ownerStage= targetCameraComponent->getOwnerStageComponent();
	if (!ownerStage)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignCameraByOriginMarker.noOwnerStageError"));
		return false;
	}

	TrackingVolumeComponentConstPtr trackingVolume= ownerStage->getTrackingVolumeConst();
	if (!trackingVolume || trackingVolume->getTrackingVolumeType() != eTrackingVolumeType::marker)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByOriginMarker.noMarkerTrackingVolumeError"));
		return false;
	}

	// 4. Tracking volume must have a valid origin marker assigned
	const MikanMarkerID originMarkerId= trackingVolume->getTrackingVolumeDefinition()->getOriginMarkerId();
	if (originMarkerId == INVALID_MIKAN_ID)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignCameraByOriginMarker.noOriginMarkerError"));
		return false;
	}

	auto* appStage= ownerWindow->pushAppStageOfType<AppStage_AlignCameraByOriginMarker>();
	appStage->setTargetCameraComponent(targetCameraComponent);

	return true;
}

void AppStage_AlignCameraByOriginMarker::setTargetCameraComponent(CameraComponentPtr cameraComponent)
{
	m_targetCameraComponent= cameraComponent;
}

// -- AppStage -- //
void AppStage_AlignCameraByOriginMarker::enter()
{
	AppStage::enter();
	assert(m_targetCameraComponent != nullptr);

	// Make sure the viewport camera is in stationary mode for the test calibration state
	MikanCameraPtr mkCamera= getFirstViewport()->getCurrentMikanCamera();
	mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Fetch the origin marker ID from the tracking volume
	StageComponentConstPtr ownerStage= m_targetCameraComponent->getOwnerStageComponent();
	assert(ownerStage != nullptr);
	TrackingVolumeComponentConstPtr trackingVolume= ownerStage->getTrackingVolumeConst();
	assert(trackingVolume != nullptr);
	m_originMarkerId= trackingVolume->getTrackingVolumeDefinition()->getOriginMarkerId();

	// Cache the target video source
	m_targetVideoSource= m_targetCameraComponent->getVideoSourceComponent();
	assert(m_targetVideoSource != nullptr);

	// A recorded take keeps the marker out of shot; its reference recording is
	// what gets sampled, and the source switches back when the stage exits
	if (auto* referenceSource= dynamic_cast<IAlignmentReferenceSource*>(m_targetVideoSource.get());
		referenceSource != nullptr && referenceSource->hasAlignmentReference())
	{
		referenceSource->beginAlignmentReference();
		m_bAlignmentReferenceActive= true;
	}

	// Create GUI panel
	m_calibrationPanel= addGuiPanel<GuiPanel_AlignCameraByOriginMarker>();
	m_calibrationPanel->OnBeginEvent= [this]() { onBeginEvent(); };
	m_calibrationPanel->OnRestartEvent= [this]() { onRestartEvent(); };
	m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_calibrationPanel->OnReturnEvent= [this]() { onReturnEvent(); };

	// Start the video stream immediately (no source camera selection needed)
	startVideoStream();
}

void AppStage_AlignCameraByOriginMarker::exit()
{
	setMenuState(eAlignCameraByOriginMarkerMenuState::inactive);

	// Free target calibration objects
	if (m_targetMarkerSampler != nullptr)
	{
		delete m_targetMarkerSampler;
		m_targetMarkerSampler= nullptr;
	}
	if (m_targetDistortionView != nullptr)
	{
		if (m_targetVideoSource)
			m_targetVideoSource->stopVideoStream(m_targetDistortionView);
		delete m_targetDistortionView;
		m_targetDistortionView= nullptr;
	}
	if (m_bAlignmentReferenceActive)
	{
		if (auto* referenceSource= dynamic_cast<IAlignmentReferenceSource*>(m_targetVideoSource.get()))
			referenceSource->endAlignmentReference();
		m_bAlignmentReferenceActive= false;
	}
	m_targetVideoSource= nullptr;

	m_targetCameraComponent= nullptr;

	// Release frame buffer resources
	if (m_frameBuffer)
		m_frameBuffer->disposeResources();

	AppStage::exit();
}

void AppStage_AlignCameraByOriginMarker::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	switch (m_calibrationPanel->getMenuState())
	{
	case eAlignCameraByOriginMarkerMenuState::pendingVideoStart:
	{
		if (m_targetDistortionView->isReceivingFrames())
		{
			setupCalibrator();
			setMenuState(eAlignCameraByOriginMarkerMenuState::verifySetup);
		}
		else if (m_targetVideoSource->getVideoStreamingStatus() == eVideoStreamingStatus::failed)
		{
			setMenuState(eAlignCameraByOriginMarkerMenuState::failedVideoStart);
		}
	}
	break;

	case eAlignCameraByOriginMarkerMenuState::verifySetup:
		updateVerifySetup();
		break;

	case eAlignCameraByOriginMarkerMenuState::capturing:
		updateCapturing();
		break;

	case eAlignCameraByOriginMarkerMenuState::testCalibration:
	{
		// Keep video feed updated for preview
		if (m_targetDistortionView)
			m_targetDistortionView->readAndProcessVideoFrame();

		// A frame-coupled camera keeps moving during the test, so the viewpoint
		// snapshot taken when this state was entered goes stale immediately and
		// the origin axes drift off the marker. Track the live pose instead.
		syncViewportToTargetCamera();
	}
	break;

	default:
		break;
	}
}

void AppStage_AlignCameraByOriginMarker::onGui()
{
	AppStage::onGui();

	// Video preview during active states
	const eAlignCameraByOriginMarkerMenuState state= m_calibrationPanel->getMenuState();
	const bool bShowVideo= state == eAlignCameraByOriginMarkerMenuState::verifySetup
						   || state == eAlignCameraByOriginMarkerMenuState::capturing;

	if (bShowVideo)
	{
		const ImVec2 displaySize= ImGui::GetMainViewport()->Size;
		const float panelWidth= 415.f * MkGui::getUiScale();
		const float videoAreaWidth= displaySize.x - panelWidth;
		constexpr ImGuiWindowFlags k_bgFlags= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
											  | ImGuiWindowFlags_NoBringToFrontOnFocus
											  | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
		ImGui::SetNextWindowSize(ImVec2(videoAreaWidth, displaySize.y));
		ImGui::SetNextWindowBgAlpha(0.0f);
		if (ImGui::Begin("##VideoTargetBg", nullptr, k_bgFlags))
		{
			IMkTexturePtr tgtTex= m_targetDistortionView ? m_targetDistortionView->getVideoTexture() : nullptr;
			if (tgtTex && tgtTex->getGlTextureId() != 0)
			{
				ImGui::Image((void*)(intptr_t)tgtTex->getGlTextureId(), ImVec2(videoAreaWidth, displaySize.y),
							 ImVec2(0, 0), ImVec2(1, 1));
			}
		}
		ImGui::End();
	}

	// Side panel with calibration controls
	const float panelWidth= 415.f * MkGui::getUiScale();
	const float displayWidth= m_ownerWindow->getWidth();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(panelWidth, 0), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##AlignCameraByOriginMarker", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_AlignCameraByOriginMarker::render(IMkViewportPtr targetViewport)
{
	AppStage::render(targetViewport);

	// During testCalibration, render the target camera view with axes at the stage origin
	if (m_calibrationPanel->getMenuState() != eAlignCameraByOriginMarkerMenuState::testCalibration)
		return;
	if (!m_frameBuffer->isValid())
		return;

	{
		MkScopedObjectBinding colorFramebufferBinding(
			m_ownerWindow->getGraphicsContext()->getMkStateStack().getCurrentState(),
			"AlignCameraByOriginMarker Color Framebuffer Scope", m_frameBuffer);

		if (colorFramebufferBinding)
		{
			// Render target video as background
			if (m_targetDistortionView)
				m_targetDistortionView->renderSelectedVideoBuffers();

			// Draw axes at the stage origin (identity = where the physical origin marker is)
			IMkGraphicsContext* graphicsContext= getGraphicsContext();
			const glm::mat4 stageOrigin= glm::mat4(1.f);
			drawTransformedAxes(graphicsContext, stageOrigin, 0.1f);

			TextStyle style= getDefaultTextStyle();
			drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(stageOrigin), L"Origin Marker");
		}

		// Clear the depth buffer before drawing the scene
		IMkState* mkState= m_ownerWindow->getGraphicsContext()->getMkStateStack().getCurrentState();
		mkStateClearBuffer(mkState, eMkClearFlags::depth);

		// Flush line and text renderers
		m_ownerWindow->getGraphicsContext()->getLineRenderer()->render(true);
		m_ownerWindow->getGraphicsContext()->getTextRenderer()->render();
	}

	// Blit framebuffer to screen as fullscreen quad
	{
		MkMaterialInstancePtr materialInstance= m_fullscreenQuad->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();

		if (auto materialBinding= material->bindMaterial())
		{
			auto colorTexture= m_frameBuffer->getColorTexture();
			materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				m_fullscreenQuad->drawElements();
			}
		}
	}
}

// -- Private helpers --

void AppStage_AlignCameraByOriginMarker::startVideoStream()
{
	// Create the distortion view eagerly — it is the stream ownership token
	m_targetDistortionView= new VideoFrameDistortionView(m_targetVideoSource, eVideoFrameProcessorMode::CALIBRATION);
	m_targetDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as a stream consumer — VideoSourceComponent::update() drives the retry loop
	m_targetVideoSource->startVideoStream(m_targetDistortionView);

	setMenuState(eAlignCameraByOriginMarkerMenuState::pendingVideoStart);
}

void AppStage_AlignCameraByOriginMarker::setupCalibrator()
{
	// Get the origin marker definition
	MarkerObjectSystemPtr markerSystem= getObjectSystemOfType<MarkerObjectSystem>();
	assert(markerSystem != nullptr);

	MarkerComponentPtr originMarkerComponent= markerSystem->getMarkerById(m_originMarkerId);
	assert(originMarkerComponent != nullptr);

	MarkerDefinitionConstPtr originMarkerDef= originMarkerComponent->getMarkerDefinition();
	assert(originMarkerDef != nullptr);

	// Set up target camera marker sampler (view already created in startVideoStream)
	m_targetMarkerSampler= new ArucoMarkerPoseSampler(m_targetCameraComponent, m_targetDistortionView,
													  ALIGN_CAMERA_BY_ORIGIN_MARKER_SAMPLE_COUNT, originMarkerDef);

	// Set up framebuffer for testCalibration view
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_targetVideoSource->getCameraIntrinsics(cameraIntrinsics);
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();

	// The test view draws the origin axes over the video through the viewport
	// camera, so that camera has to see the world the way the real lens does.
	// Left on its default projection the axes are drawn at the wrong field of
	// view, which reads as an overlay that misses the marker by more the further
	// the marker sits from image center.
	MikanCameraPtr mkCamera= getFirstViewport()->getCurrentMikanCamera();
	mkCamera->applyMonoCameraIntrinsics(&cameraIntrinsics);
	m_frameBuffer->setName("AlignCameraByOriginMarker");
	m_frameBuffer->setSize(monoIntrinsics.pixel_width, monoIntrinsics.pixel_height);
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->createResources();
	m_frameBuffer->setClearColor(glm::vec4(Colors::CornflowerBlue, 1.f));
}

void AppStage_AlignCameraByOriginMarker::updateVerifySetup()
{
	if (!m_targetDistortionView)
		return;

	// Process latest video frame
	m_targetDistortionView->readAndProcessVideoFrame();

	// Check if camera sees the origin marker
	const bool targetCanSeeMarker= m_targetMarkerSampler->computeApertureRelativeMarkerXform();
	m_calibrationPanel->setMarkerVisible(targetCanSeeMarker);
}

void AppStage_AlignCameraByOriginMarker::updateCapturing()
{
	if (!m_targetDistortionView)
		return;

	// Process latest video frame
	m_targetDistortionView->readAndProcessVideoFrame();

	// Sample aperture-to-marker transform
	if (m_targetMarkerSampler->computeApertureRelativeMarkerXform())
	{
		if (!m_targetMarkerSampler->hasFinishedSampling())
		{
			m_targetMarkerSampler->sampleLastApertureRelativeMarkerXform();
			sampleFrameCoupledWorldToStage();
		}
	}

	// Update progress
	m_calibrationPanel->setCaptureFraction(m_targetMarkerSampler->getCalibrationProgress());

	// Check if sampling is complete
	if (m_targetMarkerSampler->hasFinishedSampling())
	{
		computeAndApplyTargetTransform();
	}
}

// Record one sample of the ARKit-world-to-stage offset.
//
// A frame-coupled camera is handheld and moves between samples, so its
// aperture-relative marker poses each describe different geometry and cannot be
// averaged against one another the way a tripod-mounted camera's can. What is
// constant across samples is the offset from the source's own world space to
// stage space, so each sample is converted into that before being accumulated.
void AppStage_AlignCameraByOriginMarker::sampleFrameCoupledWorldToStage()
{
	IFrameCoupledPoseProvider* poseProvider= getFrameCoupledPoseProvider();
	if (poseProvider == nullptr)
		return;

	// The source's own world space, not the stage-space pose CameraComponent
	// consumes. Solving from the latter would fold any offset already stored into
	// the new one, so a second alignment would compound rather than replace the first.
	glm::mat4 cameraInWorld(1.f);
	uint32_t frameSeq= 0;
	if (!poseProvider->getLatestSourceWorldPose(cameraInWorld, frameSeq))
		return;

	// The marker is the stage origin, so inverting its aperture-relative pose
	// gives where the camera sits in stage space for this frame.
	const glm::dmat4 markerInCamera= m_targetMarkerSampler->getLastApertureRelativeMarkerXform();
	const glm::dmat4 cameraInStage= glm::inverse(markerInCamera);

	// cameraInStage = worldToStage * cameraInWorld, solved for worldToStage.
	const glm::dmat4 worldToStage= cameraInStage * glm::inverse(glm::dmat4(cameraInWorld));

	m_frameCoupledWorldToStageSamples.push_back(worldToStage);
}

void AppStage_AlignCameraByOriginMarker::computeAndApplyTargetTransform()
{
	// A frame-coupled source has its pose rewritten every frame from the video
	// stream, so a static camera transform would be overwritten immediately.
	// Its alignment is stored as a world-to-stage offset on the source instead.
	if (IFrameCoupledPoseProvider* poseProvider= getFrameCoupledPoseProvider())
	{
		if (m_frameCoupledWorldToStageSamples.empty())
			return;

		// Average the per-sample offsets. Rotation goes through quaternions
		// rather than averaging matrix elements, which would not stay a rotation.
		const size_t sampleCount= m_frameCoupledWorldToStageSamples.size();
		glm::dvec3 averageTranslation(0.0);
		glm::dquat averageRotation= glm::quat_cast(m_frameCoupledWorldToStageSamples[0]);

		for (size_t index= 0; index < sampleCount; ++index)
		{
			const glm::dmat4& sample= m_frameCoupledWorldToStageSamples[index];
			averageTranslation+= glm::dvec3(sample[3]);

			if (index == 0)
				continue;

			glm::dquat sampleRotation= glm::quat_cast(sample);

			// Flip the sample into the same hemisphere as the running mean first:
			// q and -q are the same rotation, so mixing the two signs averages
			// them toward nothing.
			if (glm::dot(averageRotation, sampleRotation) < 0.0)
			{
				sampleRotation= -sampleRotation;
			}

			// Running mean: weighting sample i by 1/(i+1) leaves the average of
			// everything seen so far.
			averageRotation= glm::slerp(averageRotation, sampleRotation, 1.0 / (double)(index + 1));
		}
		averageTranslation/= (double)sampleCount;

		const glm::dmat4 avgWorldToStage= glm_mat4_from_pose(glm::normalize(averageRotation), averageTranslation);

		poseProvider->setPoseOffset(glm::mat4(avgWorldToStage));

		MIKAN_LOG_INFO("AppStage_AlignCameraByOriginMarker::computeAndApplyTargetTransform")
			<< "Aligned frame-coupled camera from " << m_frameCoupledWorldToStageSamples.size() << " samples";

		setMenuState(eAlignCameraByOriginMarkerMenuState::testCalibration);
		return;
	}

	// 1. Get averaged aperture-to-marker transform
	MikanQuatd markerRot;
	MikanVector3d markerPos;
	if (!m_targetMarkerSampler->computeCalibratedMarkerPose(markerRot, markerPos))
		return;
	const glm::dmat4 avgApertureToMarker=
		glm_mat4_from_pose(MikanQuatd_to_glm_dquat(markerRot), MikanVector3d_to_glm_dvec3(markerPos));

	// 2. The origin marker IS the stage origin, so marker_StageSpace = identity.
	//    Therefore: cameraAperture_StageSpace = inverse(apertureToMarker) * identity
	//                                         = inverse(apertureToMarker)
	m_cameraApertureXform_StageSpace= glm::inverse(avgApertureToMarker);

	// 3. Apply final pose to the camera component
	m_targetCameraComponent->setRelativeTransform(glm::mat4(m_cameraApertureXform_StageSpace));

	setMenuState(eAlignCameraByOriginMarkerMenuState::testCalibration);
}

// The video source's frame-coupled pose interface, or null for the ordinary
// case of a camera whose pose comes from a tracked puck. A source that can
// carry poses but has none loaded (a recording with no pose track) is a fixed
// camera for this purpose, so it takes the same path as the puck case.
IFrameCoupledPoseProvider* AppStage_AlignCameraByOriginMarker::getFrameCoupledPoseProvider() const
{
	auto* poseProvider= dynamic_cast<IFrameCoupledPoseProvider*>(m_targetVideoSource.get());
	if (poseProvider != nullptr)
	{
		glm::mat4 transform;
		uint32_t frameSeq= 0;
		if (!poseProvider->getLatestSourceWorldPose(transform, frameSeq))
			return nullptr;
	}

	return poseProvider;
}

void AppStage_AlignCameraByOriginMarker::setMenuState(eAlignCameraByOriginMarkerMenuState newState)
{
	if (m_calibrationPanel)
	{
		m_calibrationPanel->setMenuState(newState);
	}

	if (newState == eAlignCameraByOriginMarkerMenuState::testCalibration)
	{
		syncViewportToTargetCamera();
	}
}

// Point the test viewpoint at wherever the target camera currently sits in stage
// space, so the stage-origin axes are drawn from the same place the video was shot.
// Stage space, deliberately: render() draws the origin marker at identity, so this
// viewpoint must stay in the stage's own frame rather than the editor's world frame.
void AppStage_AlignCameraByOriginMarker::syncViewportToTargetCamera()
{
	if (!m_targetCameraComponent)
		return;

	MikanCameraPtr mkCamera= getFirstViewport()->getCurrentMikanCamera();
	mkCamera->setCameraTransform(m_targetCameraComponent->getRelativeTransform().getMat4());
}

void AppStage_AlignCameraByOriginMarker::onBeginEvent()
{
	if (m_targetMarkerSampler)
		m_targetMarkerSampler->resetCalibrationState();
	m_frameCoupledWorldToStageSamples.clear();

	setMenuState(eAlignCameraByOriginMarkerMenuState::capturing);
}

void AppStage_AlignCameraByOriginMarker::onRestartEvent()
{
	if (m_targetMarkerSampler)
		m_targetMarkerSampler->resetCalibrationState();
	m_frameCoupledWorldToStageSamples.clear();
	m_calibrationPanel->setCaptureFraction(0.f);

	setMenuState(eAlignCameraByOriginMarkerMenuState::verifySetup);
}

void AppStage_AlignCameraByOriginMarker::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_AlignCameraByOriginMarker::onReturnEvent() { m_ownerWindow->popAppState(); }

// -- Remote Control -----
bool AppStage_AlignCameraByOriginMarker::handleRemoteControlCommand(const std::string& command,
																	const std::vector<std::string>& parameters,
																	std::vector<std::string>& outResults)
{
	if (command == "get_state")
	{
		return handleGetStateCommand(outResults);
	}
	else if (command == "get_marker_visible")
	{
		return handleGetMarkerVisibleCommand(outResults);
	}
	else if (command == "begin")
	{
		return handleBeginCommand(outResults);
	}
	else if (command == "restart")
	{
		return handleRestartCommand(outResults);
	}

	return AppStage::handleRemoteControlCommand(command, parameters, outResults);
}

bool AppStage_AlignCameraByOriginMarker::handleGetStateCommand(std::vector<std::string>& outResults)
{
	const eAlignCameraByOriginMarkerMenuState menuState= m_calibrationPanel->getMenuState();

	outResults.push_back(k_alignCameraByOriginMarkerMenuStateStrings[(int)menuState]);

	return true;
}

bool AppStage_AlignCameraByOriginMarker::handleGetMarkerVisibleCommand(std::vector<std::string>& outResults)
{
	const bool bIsVisible= m_calibrationPanel->getMarkerVisible();

	outResults.push_back(bIsVisible ? IRemoteControllable::k_true : IRemoteControllable::k_false);

	return true;
}

bool AppStage_AlignCameraByOriginMarker::handleBeginCommand(std::vector<std::string>& outResults)
{
	// Sampling only makes sense once the marker is actually in frame, which is the
	// same condition the Begin button is gated on in the panel.
	if (m_calibrationPanel->getMenuState() != eAlignCameraByOriginMarkerMenuState::verifySetup
		|| !m_calibrationPanel->getMarkerVisible())
	{
		outResults.push_back(IRemoteControllable::k_failure);
		return true;
	}

	onBeginEvent();
	outResults.push_back(IRemoteControllable::k_success);

	return true;
}

bool AppStage_AlignCameraByOriginMarker::handleRestartCommand(std::vector<std::string>& outResults)
{
	onRestartEvent();
	outResults.push_back(IRemoteControllable::k_success);

	return true;
}
