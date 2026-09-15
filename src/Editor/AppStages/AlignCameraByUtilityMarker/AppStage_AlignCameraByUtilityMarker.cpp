//-- includes -----
#include "AlignCameraByUtilityMarker/AppStage_AlignCameraByUtilityMarker.h"
#include "AlignCameraByUtilityMarker/GuiPanel_AlignCameraByUtilityMarker.h"
#include "LocText.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ModalSelectCamera/ModalDialog_SelectCamera.h"
#include "ArucoMarkerPoseSampler.h"
#include "VRDevicePoseSampler.h"
#include "VRDevicePoseView.h"
#include "CalibrationRenderHelpers.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "Colors.h"
#include "IMkFrameBuffer.h"
#include "IMkLineRenderer.h"
#include "IMkTriangulatedMesh.h"
#include "IMkTexture.h"
#include "MarkerComponent.h"
#include "MarkerObjectSystem.h"
#include "MathGLM.h"
#include "MathTypeConversion.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "MikanViewport.h"
#include "MkGuiScopedWindow.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkStateModifiers.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "StageComponent.h"
#include "TextStyle.h"
#include "VideoDisplayConstants.h"
#include "VideoFrameDistortionView.h"
#include "IAlignmentReferenceSource.h"
#include "VideoSourceComponent.h"
#include "VRTrackingVolumeComponent.h"

#include "glm/gtc/quaternion.hpp"

#include "imgui.h"

//-- statics ----
const char* AppStage_AlignCameraByUtilityMarker::APP_STAGE_NAME= "AlignCameraByUtilityMarker";

//-- public methods -----
AppStage_AlignCameraByUtilityMarker::AppStage_AlignCameraByUtilityMarker(IEditorWindow* ownerWindow)
	: AppStage(ownerWindow, AppStage_AlignCameraByUtilityMarker::APP_STAGE_NAME)
	, m_frameBuffer(createMkFrameBuffer())
	, m_fullscreenQuad(createFullscreenQuadMesh(ownerWindow->getGraphicsContext().get(), false))
{
}

AppStage_AlignCameraByUtilityMarker::~AppStage_AlignCameraByUtilityMarker() {}

bool AppStage_AlignCameraByUtilityMarker::tryEnterCalibration(AppStage* fromAppStage,
															  CameraComponentPtr targetCameraComponent)
{
	IEditorWindow* ownerWindow= fromAppStage->getOwnerWindow();

	// 1. Target camera must have a valid video source with valid intrinsics
	VideoSourceComponentPtr videoSource= targetCameraComponent->getVideoSourceComponent();
	if (!videoSource)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage, locText("alignCameraByUtilityMarker.noVideoSourceError"));
		return false;
	}

	if (!videoSource->areCameraIntrinsicsValid())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByUtilityMarker.invalidIntrinsicsError"));
		return false;
	}

	// 2. Target camera must NOT have a tracking mount (this method is for untracked cameras)
	if (targetCameraComponent->hasValidTrackingMountComponent())
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByUtilityMarker.alreadyHasTrackingMountError"));
		return false;
	}

	// 3. Owner stage must have a VR tracking volume
	VRTrackingVolumeComponentConstPtr trackingVolume= targetCameraComponent->getVRTrackingVolumeComponent();
	if (!trackingVolume)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByUtilityMarker.noTrackingVolumeError"));
		return false;
	}

	// 4. VR tracking volume must have a valid utility marker assigned
	const MikanMarkerID utilityMarkerId= trackingVolume->getVRTrackingVolumeDefinition()->getUtilityMarkerId();
	if (utilityMarkerId == INVALID_MIKAN_ID)
	{
		ModalDialog_MessageBox::showMessageBox(fromAppStage,
											   locText("alignCameraByUtilityMarker.noUtilityMarkerError"));
		return false;
	}

	auto* appStage= ownerWindow->pushAppStageOfType<AppStage_AlignCameraByUtilityMarker>();
	appStage->setTargetCameraComponent(targetCameraComponent);

	return true;
}

void AppStage_AlignCameraByUtilityMarker::setTargetCameraComponent(CameraComponentPtr cameraComponent)
{
	m_targetCameraComponent= cameraComponent;
}

// -- AppStage -- //
void AppStage_AlignCameraByUtilityMarker::enter()
{
	AppStage::enter();
	assert(m_targetCameraComponent != nullptr);

	// Make sure the viewport camera is in stationary mode for the test calibration state
	MikanCameraPtr mkCamera= getFirstViewport()->getCurrentMikanCamera();
	mkCamera->setCameraMovementMode(eCameraMovementMode::stationary);

	// Fetch the utility marker ID from the tracking volume
	VRTrackingVolumeComponentConstPtr trackingVolume= m_targetCameraComponent->getVRTrackingVolumeComponent();
	assert(trackingVolume != nullptr);
	m_utilityMarkerId= trackingVolume->getVRTrackingVolumeDefinition()->getUtilityMarkerId();

	// Cache the target video source
	m_targetVideoSource= m_targetCameraComponent->getVideoSourceComponent();
	assert(m_targetVideoSource != nullptr);

	// Create GUI panel
	m_calibrationPanel= addGuiPanel<GuiPanel_AlignCameraByUtilityMarker>();
	m_calibrationPanel->OnBeginEvent= [this]() { onBeginEvent(); };
	m_calibrationPanel->OnRestartEvent= [this]() { onRestartEvent(); };
	m_calibrationPanel->OnCancelEvent= [this]() { onCancelEvent(); };
	m_calibrationPanel->OnReturnEvent= [this]() { onReturnEvent(); };

	// Open the source camera selection dialog first
	openSourceCameraDialog();
}

void AppStage_AlignCameraByUtilityMarker::exit()
{
	setMenuState(eAlignCameraByUtilityMarkerMenuState::inactive);

	// Free source calibration objects
	if (m_sourceMarkerSampler != nullptr)
	{
		delete m_sourceMarkerSampler;
		m_sourceMarkerSampler= nullptr;
	}
	if (m_sourcePuckSampler != nullptr)
	{
		delete m_sourcePuckSampler;
		m_sourcePuckSampler= nullptr;
	}
	if (m_sourceDistortionView != nullptr)
	{
		if (m_sourceVideoSource)
			m_sourceVideoSource->stopVideoStream(m_sourceDistortionView);
		delete m_sourceDistortionView;
		m_sourceDistortionView= nullptr;
	}
	m_sourceVideoSource= nullptr;

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

	m_sourceCameraComponent= nullptr;
	m_targetCameraComponent= nullptr;

	// Release frame buffer resources
	if (m_frameBuffer)
		m_frameBuffer->disposeResources();

	AppStage::exit();
}

void AppStage_AlignCameraByUtilityMarker::update(float deltaSeconds)
{
	AppStage::update(deltaSeconds);

	switch (m_calibrationPanel->getMenuState())
	{
	case eAlignCameraByUtilityMarkerMenuState::pendingVideoStart:
	{
		const bool sourceFailed=
			!m_sourceVideoSource || m_sourceVideoSource->getVideoStreamingStatus() == eVideoStreamingStatus::failed;
		const bool targetFailed= m_targetVideoSource->getVideoStreamingStatus() == eVideoStreamingStatus::failed;

		if (sourceFailed || targetFailed)
		{
			setMenuState(eAlignCameraByUtilityMarkerMenuState::failedVideoStart);
		}
		else if (m_sourceDistortionView->isReceivingFrames() && m_targetDistortionView->isReceivingFrames())
		{
			setupCalibrators();
			setMenuState(eAlignCameraByUtilityMarkerMenuState::verifySetup);
		}
	}
	break;

	case eAlignCameraByUtilityMarkerMenuState::verifySetup:
		updateVerifySetup();
		break;

	case eAlignCameraByUtilityMarkerMenuState::capturing:
		updateCapturing();
		break;

	case eAlignCameraByUtilityMarkerMenuState::testCalibration:
	{
		// Keep video feeds updated for preview
		if (m_targetDistortionView)
			m_targetDistortionView->readAndProcessVideoFrame();
	}
	break;

	default:
		break;
	}
}

void AppStage_AlignCameraByUtilityMarker::onGui()
{
	AppStage::onGui();

	// Side-by-side video preview during active states
	const eAlignCameraByUtilityMarkerMenuState state= m_calibrationPanel->getMenuState();
	const bool bShowVideo= state == eAlignCameraByUtilityMarkerMenuState::verifySetup
						   || state == eAlignCameraByUtilityMarkerMenuState::capturing;

	if (bShowVideo)
	{
		const ImVec2 displaySize= ImGui::GetMainViewport()->Size;
		constexpr float k_panelWidth= 415.f;
		const float videoAreaWidth= displaySize.x - k_panelWidth;
		const float halfH= displaySize.y * 0.5f;
		constexpr ImGuiWindowFlags k_bgFlags= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
											  | ImGuiWindowFlags_NoBringToFrontOnFocus
											  | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
		ImGui::SetNextWindowSize(ImVec2(videoAreaWidth, halfH));
		ImGui::SetNextWindowBgAlpha(0.0f);
		if (ImGui::Begin("##VideoSourceBg", nullptr, k_bgFlags))
		{
			// Source video (top)
			IMkTexturePtr srcTex= m_sourceDistortionView ? m_sourceDistortionView->getVideoTexture() : nullptr;
			if (srcTex && srcTex->getGlTextureId() != 0)
			{
				ImGui::Image((void*)(intptr_t)srcTex->getGlTextureId(), ImVec2(videoAreaWidth, halfH), ImVec2(0, 0),
							 ImVec2(1, 1));
			}
		}
		ImGui::End();

		ImGui::SetNextWindowPos(ImVec2(0.f, halfH));
		ImGui::SetNextWindowSize(ImVec2(videoAreaWidth, halfH));
		ImGui::SetNextWindowBgAlpha(0.0f);
		if (ImGui::Begin("##VideoTargetBg", nullptr, k_bgFlags))
		{
			// Target video (Bottom)
			IMkTexturePtr tgtTex= m_targetDistortionView ? m_targetDistortionView->getVideoTexture() : nullptr;
			if (tgtTex && tgtTex->getGlTextureId() != 0)
			{
				ImGui::Image((void*)(intptr_t)tgtTex->getGlTextureId(), ImVec2(videoAreaWidth, halfH), ImVec2(0, 0),
							 ImVec2(1, 1));
			}
		}
		ImGui::End();
	}

	// Side panel with calibration controls
	constexpr float k_panelWidth= 415.f;
	const float displayWidth= m_ownerWindow->getWidth();

	ImGui::SetNextWindowPos(ImVec2(displayWidth - k_panelWidth, 0.f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(k_panelWidth, 0), ImGuiCond_Always);
	constexpr ImGuiWindowFlags k_flags=
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
	MkGuiScopedWindow panel("##AlignCameraByUtilityMarker", nullptr, k_flags);
	if (!panel)
		return;

	for (IGuiPanel* guiPanel : m_guiPanels)
		guiPanel->onGui();
}

void AppStage_AlignCameraByUtilityMarker::render(IMkViewportPtr targetViewport)
{
	AppStage::render(targetViewport);

	// During testCalibration, render the target camera view with axes at the marker
	if (m_calibrationPanel->getMenuState() != eAlignCameraByUtilityMarkerMenuState::testCalibration)
		return;
	if (!m_frameBuffer->isValid())
		return;

	{
		MkScopedObjectBinding colorFramebufferBinding(
			m_ownerWindow->getGraphicsContext()->getMkStateStack().getCurrentState(),
			"AlignCameraByUtilityMarker Color Framebuffer Scope", m_frameBuffer);

		if (colorFramebufferBinding)
		{
			// Render target video as background
			if (m_targetDistortionView)
				m_targetDistortionView->renderSelectedVideoBuffers();

			// Draw axes at the utility marker's computed stage-space position
			IMkGraphicsContext* graphicsContext= getGraphicsContext();
			const glm::mat4 markerXform= glm::mat4(m_markerXform_StageSpace);
			drawTransformedAxes(graphicsContext, markerXform, 0.1f);

			TextStyle style= getDefaultTextStyle();
			drawTextAtWorldPosition(graphicsContext, style, glm_mat4_get_position(markerXform), L"Utility Marker");
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

void AppStage_AlignCameraByUtilityMarker::openSourceCameraDialog()
{
	setMenuState(eAlignCameraByUtilityMarkerMenuState::selectSourceCamera);

	ModalDialog_SelectCamera::selectCamera(
		this, [this](MikanCameraID cameraId) { onSourceCameraSelected(cameraId); }, [this]() { onCancelEvent(); },
		[](CameraComponentPtr c) -> bool
		{ return c->hasValidTrackingMountComponent() && c->hasValidApertureOffsetXform(); });
}

void AppStage_AlignCameraByUtilityMarker::onSourceCameraSelected(MikanCameraID cameraId)
{
	CameraObjectSystemPtr cameraSystem= getObjectSystemOfType<CameraObjectSystem>();
	assert(cameraSystem != nullptr);

	m_sourceCameraComponent= cameraSystem->getCameraById(cameraId);
	if (!m_sourceCameraComponent)
	{
		ModalDialog_MessageBox::showMessageBox(this, locText("alignCameraByUtilityMarker.sourceCameraNotFoundError"));
		openSourceCameraDialog();
		return;
	}

	VideoSourceComponentPtr sourceVideoSource= m_sourceCameraComponent->getVideoSourceComponent();
	if (!sourceVideoSource || !sourceVideoSource->areCameraIntrinsicsValid())
	{
		ModalDialog_MessageBox::showMessageBox(
			this, locText("alignCameraByUtilityMarker.sourceCameraInvalidIntrinsicsError"));
		m_sourceCameraComponent= nullptr;
		openSourceCameraDialog();
		return;
	}

	m_sourceVideoSource= sourceVideoSource;
	startVideoStreams();
}

void AppStage_AlignCameraByUtilityMarker::startVideoStreams()
{
	// Create views eagerly — they are the stream ownership tokens
	m_sourceDistortionView= new VideoFrameDistortionView(m_sourceVideoSource, eVideoFrameProcessorMode::CALIBRATION);
	m_sourceDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// A recorded take keeps the marker out of shot; its reference recording is
	// what gets sampled, and the source switches back when the stage exits
	if (auto* referenceSource= dynamic_cast<IAlignmentReferenceSource*>(m_targetVideoSource.get());
		referenceSource != nullptr && referenceSource->hasAlignmentReference())
	{
		referenceSource->beginAlignmentReference();
		m_bAlignmentReferenceActive= true;
	}

	m_targetDistortionView= new VideoFrameDistortionView(m_targetVideoSource, eVideoFrameProcessorMode::CALIBRATION);
	m_targetDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register as stream consumers — VideoSourceComponent::update() drives the retry loop
	m_sourceVideoSource->startVideoStream(m_sourceDistortionView);
	m_targetVideoSource->startVideoStream(m_targetDistortionView);

	setMenuState(eAlignCameraByUtilityMarkerMenuState::pendingVideoStart);
}

void AppStage_AlignCameraByUtilityMarker::setupCalibrators()
{
	// Get the utility marker definition
	MarkerObjectSystemPtr markerSystem= getObjectSystemOfType<MarkerObjectSystem>();
	assert(markerSystem != nullptr);

	MarkerComponentPtr utilityMarkerComponent= markerSystem->getMarkerById(m_utilityMarkerId);
	assert(utilityMarkerComponent != nullptr);

	MarkerDefinitionConstPtr utilityMarkerDef= utilityMarkerComponent->getMarkerDefinition();
	assert(utilityMarkerDef != nullptr);

	// Set up source camera samplers (views already created in startVideoStreams)
	// Use explicit utilityMarkerDef so we track the utility marker, not the stage origin marker
	m_sourceMarkerSampler= new ArucoMarkerPoseSampler(m_sourceCameraComponent, m_sourceDistortionView,
													  ALIGN_CAMERA_BY_UTILITY_MARKER_SAMPLE_COUNT, utilityMarkerDef);

	// Set up target camera samplers (views already created in startVideoStreams)
	m_targetMarkerSampler= new ArucoMarkerPoseSampler(m_targetCameraComponent, m_targetDistortionView,
													  ALIGN_CAMERA_BY_UTILITY_MARKER_SAMPLE_COUNT, utilityMarkerDef);

	// Set up source puck sampler
	VRDevicePoseViewPtr puckPoseView=
		m_sourceCameraComponent->makeTrackingMountPoseView(eVRDevicePoseSpace::VRTrackingSystemPose);
	m_sourcePuckSampler=
		new VRDevicePoseSampler(puckPoseView, m_sourceCameraComponent, ALIGN_CAMERA_BY_UTILITY_MARKER_SAMPLE_COUNT);

	// Set up framebuffer for testCalibration view
	MikanVideoSourceIntrinsics cameraIntrinsics;
	m_targetVideoSource->getCameraIntrinsics(cameraIntrinsics);
	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics.getMonoIntrinsics();
	m_frameBuffer->setName("AlignCameraByUtilityMarker");
	m_frameBuffer->setSize(monoIntrinsics.pixel_width, monoIntrinsics.pixel_height);
	m_frameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_frameBuffer->createResources();
	m_frameBuffer->setClearColor(glm::vec4(Colors::CornflowerBlue, 1.f));
}

void AppStage_AlignCameraByUtilityMarker::updateVerifySetup()
{
	if (!m_sourceDistortionView || !m_targetDistortionView)
		return;

	// Process latest video frames
	m_sourceDistortionView->readAndProcessVideoFrame();
	m_targetDistortionView->readAndProcessVideoFrame();

	// Check if source camera sees the utility marker
	const bool sourceCanSeeMarker= m_sourceMarkerSampler->computeApertureRelativeMarkerXform();
	m_calibrationPanel->setSourceMarkerVisible(sourceCanSeeMarker);

	// Check if target camera sees the utility marker
	const bool targetCanSeeMarker= m_targetMarkerSampler->computeApertureRelativeMarkerXform();
	m_calibrationPanel->setTargetMarkerVisible(targetCanSeeMarker);
}

void AppStage_AlignCameraByUtilityMarker::updateCapturing()
{
	if (!m_sourceDistortionView || !m_targetDistortionView)
		return;

	// Process latest video frames
	m_sourceDistortionView->readAndProcessVideoFrame();
	m_targetDistortionView->readAndProcessVideoFrame();

	// Sample from source camera: marker pose + puck pose
	if (m_sourcePuckSampler->computeVRDeviceXform() && m_sourceMarkerSampler->computeApertureRelativeMarkerXform())
	{
		if (!m_sourcePuckSampler->hasFinishedSampling())
		{
			m_sourcePuckSampler->sampleLastVRDeviceXform();
		}

		if (!m_sourceMarkerSampler->hasFinishedSampling())
		{
			m_sourceMarkerSampler->sampleLastApertureRelativeMarkerXform();
		}
	}

	// Sample from target camera (aperture-relative marker pose)
	if (m_targetMarkerSampler->computeApertureRelativeMarkerXform())
	{
		if (!m_targetMarkerSampler->hasFinishedSampling())
		{
			m_targetMarkerSampler->sampleLastApertureRelativeMarkerXform();
		}
	}

	// Update progress
	m_calibrationPanel->setSourceCaptureFraction(m_sourceMarkerSampler->getCalibrationProgress());
	m_calibrationPanel->setTargetCaptureFraction(m_targetMarkerSampler->getCalibrationProgress());

	// Check if both have finished sampling
	if (m_sourceMarkerSampler->hasFinishedSampling() && m_targetMarkerSampler->hasFinishedSampling())
	{
		computeAndApplyTargetTransform();
	}
}

void AppStage_AlignCameraByUtilityMarker::computeAndApplyTargetTransform()
{
	// 1. Averaged source puck pose in VR space
	MikanQuatd puckRot;
	MikanVector3d puckPos;
	if (!m_sourcePuckSampler->computeCalibratedDevicePose(puckRot, puckPos))
		return;
	const glm::dmat4 avgSourcePuckXform_VRSpace=
		glm_mat4_from_pose(MikanQuatd_to_glm_dquat(puckRot), MikanVector3d_to_glm_dvec3(puckPos));

	// 2. Source aperture offset (fixed, not sampled)
	glm::mat4 sourceApertureOffset;
	m_sourceCameraComponent->getApertureOffsetXform(sourceApertureOffset);

	// 3. Averaged source aperture-to-marker transform
	MikanQuatd srcMarkerRot;
	MikanVector3d srcMarkerPos;
	if (!m_sourceMarkerSampler->computeCalibratedMarkerPose(srcMarkerRot, srcMarkerPos))
		return;
	const glm::dmat4 avgSrcApertureToMarker=
		glm_mat4_from_pose(MikanQuatd_to_glm_dquat(srcMarkerRot), MikanVector3d_to_glm_dvec3(srcMarkerPos));

	// 4. Compose to get marker in VR Tracking Space
	glm::dmat4 srcApertureXform_VRSpace=
		glm_composite_xform(glm::dmat4(sourceApertureOffset), avgSourcePuckXform_VRSpace);
	glm::dmat4 markerXform_VRSpace= glm_composite_xform(avgSrcApertureToMarker, srcApertureXform_VRSpace);

	// 5. Averaged target aperture-to-marker transform
	MikanQuatd targetMarkerRot;
	MikanVector3d targetMarkerPos;
	if (!m_targetMarkerSampler->computeCalibratedMarkerPose(targetMarkerRot, targetMarkerPos))
		return;
	const glm::dmat4 avgTargetApertureToMarker=
		glm_mat4_from_pose(MikanQuatd_to_glm_dquat(targetMarkerRot), MikanVector3d_to_glm_dvec3(targetMarkerPos));

	// 6. Compute the target aperture transform in VRSpace
	const glm::dmat4 avgMarkerToTargetAperture= glm::inverse(avgTargetApertureToMarker);
	const glm::dmat4 targetApertureXform_VRSpace= glm_composite_xform(avgMarkerToTargetAperture, markerXform_VRSpace);

	// 7. Convert the target aperture transform from VRSpace to StageSpace
	const auto stageComponent= m_targetCameraComponent->getOwnerStageComponent();
	const auto trackingVolumeDefinition=
		std::static_pointer_cast<const VRTrackingVolumeComponent>(stageComponent->getTrackingVolumeConst());
	const glm::mat4 vrSpaceToStageSpace= trackingVolumeDefinition->getVRSpaceToStageSpace();

	// 8. Compute final target aperture pose in stage space and apply
	m_targetApertureXform_StageSpace= glm_composite_xform(targetApertureXform_VRSpace, vrSpaceToStageSpace);
	m_targetCameraComponent->setRelativeTransform(glm::mat4(m_targetApertureXform_StageSpace));

	// (For Debug) Compute the marker transform in stage space
	m_markerXform_StageSpace= glm_composite_xform(avgTargetApertureToMarker, m_targetApertureXform_StageSpace);

	setMenuState(eAlignCameraByUtilityMarkerMenuState::testCalibration);
}

void AppStage_AlignCameraByUtilityMarker::setMenuState(eAlignCameraByUtilityMarkerMenuState newState)
{
	if (m_calibrationPanel)
	{
		m_calibrationPanel->setMenuState(newState);
	}

	if (newState == eAlignCameraByUtilityMarkerMenuState::testCalibration)
	{
		if (m_targetCameraComponent)
		{
			MikanCameraPtr mkCamera= getFirstViewport()->getCurrentMikanCamera();

			mkCamera->setCameraTransform(m_targetCameraComponent->getRelativeTransform().getMat4());
		}
	}
}

void AppStage_AlignCameraByUtilityMarker::onBeginEvent()
{
	// Reset all samplers
	if (m_sourceMarkerSampler)
		m_sourceMarkerSampler->resetCalibrationState();
	if (m_sourcePuckSampler)
		m_sourcePuckSampler->resetCalibrationState();
	if (m_targetMarkerSampler)
		m_targetMarkerSampler->resetCalibrationState();

	setMenuState(eAlignCameraByUtilityMarkerMenuState::capturing);
}

void AppStage_AlignCameraByUtilityMarker::onRestartEvent()
{
	// Reset all samplers
	if (m_sourceMarkerSampler)
		m_sourceMarkerSampler->resetCalibrationState();
	if (m_sourcePuckSampler)
		m_sourcePuckSampler->resetCalibrationState();
	if (m_targetMarkerSampler)
		m_targetMarkerSampler->resetCalibrationState();
	m_calibrationPanel->setSourceCaptureFraction(0.f);
	m_calibrationPanel->setTargetCaptureFraction(0.f);

	setMenuState(eAlignCameraByUtilityMarkerMenuState::verifySetup);
}

void AppStage_AlignCameraByUtilityMarker::onCancelEvent() { m_ownerWindow->popAppState(); }

void AppStage_AlignCameraByUtilityMarker::onReturnEvent() { m_ownerWindow->popAppState(); }
