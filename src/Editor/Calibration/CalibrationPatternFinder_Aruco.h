#pragma once

#include "CalibrationPatternFinder.h"

class CalibrationPatternFinder_Aruco : public CalibrationPatternFinder
{
public:
	// Detects the stage's origin marker (default behavior)
	CalibrationPatternFinder_Aruco(CameraComponentConstPtr cameraComponent, VideoFrameDistortionView* distortionView);

	// Detects an explicit marker definition (e.g., a utility marker)
	CalibrationPatternFinder_Aruco(CameraComponentConstPtr cameraComponent, VideoFrameDistortionView* distortionView,
								   MarkerDefinitionConstPtr markerDefinition);
	virtual ~CalibrationPatternFinder_Aruco();

	virtual eCalibrationPatternType getCalibrationPatternType() const override
	{
		return eCalibrationPatternType::mode_aruco;
	}
	virtual bool findNewCalibrationPattern(const float minSeperationDist= 0.f) override;
	// The square-marker solve rather than the general one the other patterns use
	virtual bool estimateNewCalibrationPatternPose(glm::dmat4& outCameraToPatternXform) override;
	virtual bool fetchLastFoundCalibrationPattern(t_opencv_point2d_list& outImagePoints,
												  t_opencv_pointID_list& outImagePointIDs,
												  cv::Point2f outBoundingQuad[4]) override;
	virtual void renderCalibrationPattern2D() const override;

	// Finds the marker in a grayscale image that needs no undistortion, a recorded
	// marker shot, and solves its aperture-relative pose with the detector, corner
	// geometry, and solve the finder uses on a live view, so both land on the same
	// pose for the same pixels
	static bool estimateMarkerPoseInImage(const cv::Mat& grayscaleImage, const MikanMonoIntrinsics& intrinsics,
										  MarkerDefinitionConstPtr markerDefinition,
										  eCharucoDictionaryType dictionaryType, glm::dmat4& outApertureToMarkerXform);

protected:
	class ArucoBoardData* m_markerData;
};
