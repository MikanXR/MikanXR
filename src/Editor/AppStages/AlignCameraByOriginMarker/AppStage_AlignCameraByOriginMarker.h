#pragma once

//-- includes -----
#include "AppStage.h"
#include "ComponentFwd.h"
#include "Constants_AlignCameraByOriginMarker.h"
#include "MikanCoreTypes.h"
#include "MikanRendererFwd.h"
#include "MikanTypeFwd.h"

#include "glm/ext/matrix_double4x4.hpp"

#include <memory>
#include <vector>

class GuiPanel_AlignCameraByOriginMarker;

//-- definitions -----
class AppStage_AlignCameraByOriginMarker : public AppStage
{
public:
	static const char* APP_STAGE_NAME;

	AppStage_AlignCameraByOriginMarker(class IEditorWindow* ownerWindow);
	virtual ~AppStage_AlignCameraByOriginMarker();

	static bool tryEnterCalibration(AppStage* fromAppStage, CameraComponentPtr targetCameraComponent);

	void setTargetCameraComponent(CameraComponentPtr cameraComponent);

	// -- AppStage -- //
	virtual void enter() override;
	virtual void exit() override;
	virtual void update(float deltaSeconds) override;
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport) override;

protected:
	void startVideoStream();
	void setupCalibrator();
	void updateVerifySetup();
	void updateCapturing();
	void computeAndApplyTargetTransform();
	// Frame-coupled (moving) cameras store their alignment as a world-to-stage
	// offset on the video source rather than as a static camera transform.
	void sampleFrameCoupledWorldToStage();
	class IFrameCoupledPoseProvider* getFrameCoupledPoseProvider() const;
	void setMenuState(eAlignCameraByOriginMarkerMenuState newState);
	void syncViewportToTargetCamera();

	void onBeginEvent();
	void onRestartEvent();
	void onCancelEvent();
	void onReturnEvent();

	// -- Remote Control -- //
	virtual bool handleRemoteControlCommand(const std::string& command, const std::vector<std::string>& parameters,
											std::vector<std::string>& outResults) override;
	bool handleGetStateCommand(std::vector<std::string>& outResults);
	bool handleGetMarkerVisibleCommand(std::vector<std::string>& outResults);
	bool handleBeginCommand(std::vector<std::string>& outResults);
	bool handleRestartCommand(std::vector<std::string>& outResults);

private:
	GuiPanel_AlignCameraByOriginMarker* m_calibrationPanel= nullptr;

	// Target camera (being calibrated - no tracking mount)
	CameraComponentPtr m_targetCameraComponent;
	VideoSourceComponentPtr m_targetVideoSource;
	bool m_bAlignmentReferenceActive= false;
	class VideoFrameDistortionView* m_targetDistortionView= nullptr;
	class ArucoMarkerPoseSampler* m_targetMarkerSampler= nullptr;

	// Origin marker ID from tracking volume
	MikanMarkerID m_originMarkerId= INVALID_MIKAN_ID;

	// Final computed camera aperture pose in stage space
	glm::dmat4 m_cameraApertureXform_StageSpace;

	// One world-to-stage estimate per accepted sample, for a frame-coupled
	// source. Empty for every other camera type.
	std::vector<glm::dmat4> m_frameCoupledWorldToStageSamples;

	IMkFrameBufferPtr m_frameBuffer;
	IMkTriangulatedMeshPtr m_fullscreenQuad;
};
