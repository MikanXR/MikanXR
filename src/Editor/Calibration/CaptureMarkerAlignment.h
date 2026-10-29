#pragma once

#include "ComponentFwd.h"
#include "ProjectConfigConstants.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <string>

class Capture;
struct PoseTrackFrame;

namespace cv
{
class Mat;
}

// Solves a recorded take's source-world-to-stage offset from the marker shot it
// carries, the image of the origin marker and the pose it was taken from. The
// Align Camera stage solves the same offset by sampling that shot through a live
// view. This reads it straight from the capture, with no window or GL context,
// so a take aligns itself when it opens.
namespace CaptureMarkerAlignment
{
// The offset from one marker shot: the image (BGR or grayscale, at the
// resolution the intrinsics describe) and the frame of the marker_pose track it
// was taken with. ARKit reports no lens distortion, so the image is solved as is.
bool solveWorldToStage(const cv::Mat& markerImage, const PoseTrackFrame& markerShotFrame, int imageWidth,
					   int imageHeight, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError);

// The offset from an open capture's marker_image and marker_pose members
bool solveWorldToStage(Capture& capture, MarkerDefinitionConstPtr originMarker, eCharucoDictionaryType dictionaryType,
					   glm::mat4& outWorldToStage, std::string& outError);
} // namespace CaptureMarkerAlignment
