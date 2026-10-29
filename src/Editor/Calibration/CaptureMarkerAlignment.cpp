#include "CaptureMarkerAlignment.h"
#include "CalibrationPatternFinder_Aruco.h"
#include "CameraMath.h"
#include "Capture.h"
#include "FrameCoupledAlignment.h"
#include "MikanVideoSourceTypes.h"
#include "PoseTrack.h"

#include "opencv2/imgproc.hpp"

namespace CaptureMarkerAlignment
{
bool solveWorldToStage(const cv::Mat& markerImage, const PoseTrackFrame& markerShotFrame, int imageWidth,
					   int imageHeight, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError)
{
	if (!originMarker)
	{
		outError= "no origin marker";
		return false;
	}

	if (markerImage.empty() || markerImage.cols != imageWidth || markerImage.rows != imageHeight)
	{
		outError= "the marker image is not the size its intrinsics describe";
		return false;
	}

	cv::Mat grayscaleImage;
	if (markerImage.channels() == 1)
		grayscaleImage= markerImage;
	else
		cv::cvtColor(markerImage, grayscaleImage, cv::COLOR_BGR2GRAY);

	MikanMonoIntrinsics intrinsics;
	createMonoIntrinsicsFromPinhole(imageWidth, imageHeight, markerShotFrame.fx, markerShotFrame.fy, markerShotFrame.cx,
									markerShotFrame.cy, intrinsics);

	glm::dmat4 apertureToMarker;
	if (!CalibrationPatternFinder_Aruco::estimateMarkerPoseInImage(grayscaleImage, intrinsics, originMarker,
																   dictionaryType, apertureToMarker))
	{
		outError= "the origin marker was not found in the marker image";
		return false;
	}

	// One sample: the shot's camera is where the pose says it was, so there is
	// nothing to average
	outWorldToStage=
		glm::mat4(FrameCoupledAlignment::computeWorldToStageSample(apertureToMarker, markerShotFrame.cameraToWorld));
	return true;
}

bool solveWorldToStage(Capture& capture, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError)
{
	const PoseTrack& markerTrack= capture.getPoseTrack(eCaptureMemberRole::marker_pose);
	if (!markerTrack.isLoaded() || markerTrack.getFrameCount() == 0)
	{
		outError= "the capture has no marker pose";
		return false;
	}

	cv::Mat markerImage;
	if (!capture.decodeImage(eCaptureMemberRole::marker_image, markerImage, outError))
		return false;

	return solveWorldToStage(markerImage, markerTrack.getFrame(0), markerTrack.getImageWidth(),
							 markerTrack.getImageHeight(), originMarker, dictionaryType, outWorldToStage, outError);
}
} // namespace CaptureMarkerAlignment
