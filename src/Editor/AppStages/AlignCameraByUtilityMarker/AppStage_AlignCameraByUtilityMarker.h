#pragma once

//-- includes -----
#include "AppStage.h"
#include "ComponentFwd.h"
#include "Constants_AlignCameraByUtilityMarker.h"
#include "MikanCoreTypes.h"
#include "MikanRendererFwd.h"
#include "MikanTypeFwd.h"

#include "glm/ext/matrix_double4x4.hpp"

#include <memory>

class GuiPanel_AlignCameraByUtilityMarker;

//-- definitions -----
class AppStage_AlignCameraByUtilityMarker : public AppStage
{
public:
	static const char* APP_STAGE_NAME;

	AppStage_AlignCameraByUtilityMarker(class IEditorWindow* ownerWindow);
	virtual ~AppStage_AlignCameraByUtilityMarker();

	static bool tryEnterCalibration(AppStage* fromAppStage, CameraComponentPtr targetCameraComponent);

	void setTargetCameraComponent(CameraComponentPtr cameraComponent);

	// -- AppStage -- //
	virtual void enter() override;
	virtual void exit() override;
	virtual void update(float deltaSeconds) override;
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport) override;

protected:
	void openSourceCameraDialog();
	void onSourceCameraSelected(MikanCameraID cameraId);
	void startVideoStreams();
	void setupCalibrators();
	void updateVerifySetup();
	void updateCapturing();
	void computeAndApplyTargetTransform();
	void setMenuState(eAlignCameraByUtilityMarkerMenuState newState);

	void onBeginEvent();
	void onRestartEvent();
	void onCancelEvent();
	void onReturnEvent();

private:
	GuiPanel_AlignCameraByUtilityMarker* m_calibrationPanel= nullptr;

	// Target camera (being calibrated - no tracking mount)
	CameraComponentPtr m_targetCameraComponent;
	VideoSourceComponentPtr m_targetVideoSource;
	bool m_bAlignmentReferenceActive= false;
	class VideoFrameDistortionView* m_targetDistortionView= nullptr;
	class ArucoMarkerPoseSampler* m_targetMarkerSampler= nullptr;

	// Source camera (already calibrated with tracking mount + aperture offset)
	CameraComponentPtr m_sourceCameraComponent;
	VideoSourceComponentPtr m_sourceVideoSource;
	class VideoFrameDistortionView* m_sourceDistortionView= nullptr;
	class ArucoMarkerPoseSampler* m_sourceMarkerSampler= nullptr;
	class VRDevicePoseSampler* m_sourcePuckSampler= nullptr;

	// Utility marker ID from tracking volume
	MikanMarkerID m_utilityMarkerId= INVALID_MIKAN_ID;

	// Averaged results
	glm::dmat4 m_markerXform_StageSpace;         // from source sampler + puck compose
	glm::dmat4 m_targetApertureXform_StageSpace; // final computed pose

	IMkFrameBufferPtr m_frameBuffer;
	IMkTriangulatedMeshPtr m_fullscreenQuad;
};
