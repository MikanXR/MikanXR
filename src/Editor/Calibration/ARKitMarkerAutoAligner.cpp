#include "ARKitMarkerAutoAligner.h"
#include "ArucoMarkerPoseSampler.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "FrameCoupledAlignment.h"
#include "IEditorWindow.h"
#include "IFrameCoupledPoseProvider.h"
#include "Logger.h"
#include "ProjectManager.h"
#include "StageComponent.h"
#include "TrackingVolumeComponent.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"

ARKitMarkerAutoAligner::ARKitMarkerAutoAligner(IEditorWindow* ownerWindow, VideoSourceComponentPtr videoSource)
	: m_ownerWindow(ownerWindow)
	, m_videoSource(videoSource)
{
	// The view is the stream ownership token: subscribing is what makes the
	// source deliver frames to it
	m_view= std::make_unique<VideoFrameDistortionView>(videoSource, eVideoFrameProcessorMode::CALIBRATION);
	m_view->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);
	videoSource->startVideoStream(m_view.get());
}

ARKitMarkerAutoAligner::~ARKitMarkerAutoAligner() { releaseStream(); }

void ARKitMarkerAutoAligner::releaseStream()
{
	m_sampler.reset();
	if (m_view)
	{
		if (VideoSourceComponentPtr videoSource= m_videoSource.lock())
		{
			videoSource->stopVideoStream(m_view.get());
		}
		m_view.reset();
	}
}

bool ARKitMarkerAutoAligner::shouldRetire() const
{
	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	if (!videoSource || m_bSolved)
		return true;

	auto* poseProvider= dynamic_cast<IFrameCoupledPoseProvider*>(videoSource.get());
	return poseProvider == nullptr || poseProvider->hasPoseOffset();
}

// The sampler reads the camera's intrinsics and the stage's origin marker at
// construction, so it waits for frames and refuses a camera it cannot sample
bool ARKitMarkerAutoAligner::ensureSampler()
{
	if (m_sampler)
		return true;

	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	if (!videoSource || !m_view || !m_view->isReceivingFrames())
		return false;

	ProjectManagerPtr projectManager= m_ownerWindow->getProjectManager();
	CameraObjectSystemPtr cameraSystem=
		projectManager ? projectManager->getSystemOfType<CameraObjectSystem>() : nullptr;
	m_camera= cameraSystem ? cameraSystem->findCameraForVideoSource(videoSource->getComponentId()) : nullptr;
	if (!m_camera)
		return false;

	MikanVideoSourceIntrinsics intrinsics;
	if (!m_camera->getApertureIntrinsics(intrinsics)
		|| intrinsics.intrinsics_type != MikanIntrinsicsType::MONO_CAMERA_INTRINSICS)
	{
		return false;
	}

	StageComponentConstPtr stage= m_camera->getOwnerStageComponent();
	TrackingVolumeComponentConstPtr trackingVolume= stage ? stage->getTrackingVolumeConst() : nullptr;
	if (!trackingVolume || !trackingVolume->getTrackingVolumeDefinition()->getOriginMarker())
		return false;

	m_sampler= std::make_unique<ArucoMarkerPoseSampler>(m_camera, m_view.get(), k_sampleCount);
	return true;
}

void ARKitMarkerAutoAligner::update(double nowSeconds)
{
	if (m_bSolved || !m_view)
		return;
	if (m_lastDetectSeconds >= 0.0 && nowSeconds - m_lastDetectSeconds < k_detectIntervalSeconds)
		return;
	m_lastDetectSeconds= nowSeconds;

	if (!ensureSampler())
		return;

	VideoSourceComponentPtr videoSource= m_videoSource.lock();
	auto* poseProvider= videoSource ? dynamic_cast<IFrameCoupledPoseProvider*>(videoSource.get()) : nullptr;
	if (poseProvider == nullptr)
		return;

	// The readback and undistort run here, at the detection rate rather than
	// the frame rate
	m_view->readAndProcessVideoFrame();
	m_bMarkerVisible= m_sampler->computeApertureRelativeMarkerXform();
	if (!m_bMarkerVisible)
	{
		if (m_lastSeenSeconds >= 0.0 && nowSeconds - m_lastSeenSeconds > k_lostResetSeconds)
		{
			m_samples.clear();
		}
		return;
	}
	m_lastSeenSeconds= nowSeconds;

	glm::mat4 cameraInWorld(1.f);
	uint32_t frameSeq= 0;
	if (!poseProvider->getLatestSourceWorldPose(cameraInWorld, frameSeq))
		return;

	m_samples.push_back(FrameCoupledAlignment::computeWorldToStageSample(
		m_sampler->getLastApertureRelativeMarkerXform(), cameraInWorld));
	if ((int)m_samples.size() < k_sampleCount)
		return;

	poseProvider->setPoseOffset(glm::mat4(FrameCoupledAlignment::averageWorldToStage(m_samples)));
	m_bSolved= true;
	MIKAN_LOG_INFO("ARKitMarkerAutoAligner")
		<< "Aligned " << videoSource->getDevicePath() << " from " << m_samples.size() << " marker samples";
	releaseStream();
}
