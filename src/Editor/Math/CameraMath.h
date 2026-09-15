#pragma once

// -- includes -----
#include "OpenCVFwd.h"
#include "ComponentFwd.h"
#include "VideoDisplayConstants.h"

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/vector_double3.hpp"

#include <memory>

#define DEFAULT_MONO_HFOV 60.0 // 60 degrees
#define DEFAULT_MONO_ZNEAR 0.1 // 0.1 meters (10cm)
#define DEFAULT_MONO_ZFAR 20.0 // 20 meters

using t_opengl_point3d_list= std::vector<glm::vec3>;

struct OpenCVCalibrationGeometry
{
	t_opencv_point3d_list points;
};

struct OpenGLCalibrationGeometry
{
	t_opengl_point3d_list points;
};

// -- interface -----
glm::mat4 computeGLMCameraViewMatrix(const glm::mat4& poseXform);
bool computeOpenCVCameraExtrinsicMatrix(CameraComponentPtr cameraComponent, cv::Matx34f& out);

bool computeMonoLensCameraCalibration(const int frameWidth, const int frameHeight,
									  const OpenCVCalibrationGeometry& opencvLensCalibrationGeometry,
									  const std::vector<t_opencv_point2d_list>& cvImagePointsList,
									  const std::vector<t_opencv_pointID_list>& cvImagePointIDs,
									  struct MikanMonoIntrinsics& outIntrinsics, double& outReprojectionError);

bool computeOpenCVCameraRelativePatternTransform(const struct MikanMonoIntrinsics& intrinsics,
												 const t_opencv_point2d_list& imagePoints,
												 const t_opencv_point3d_list& objectPointsMM, cv::Quatd& outOrientation,
												 cv::Vec3d& outPositionMM, double* outMeanError= nullptr);
void convertOpenCVCameraRelativePoseToGLMMat(const cv::Quatd& orientation, const cv::Vec3d& positionMM,
											 glm::dmat4& outXform);

void extractCameraIntrinsicMatrixParameters(const struct MikanMatrix3d& intrinsic_matrix, float& out_focal_length_x,
											float& out_focal_length_y, float& out_principal_point_x,
											float& out_principal_point_y, float& out_skew);
void extractCameraIntrinsicMatrixParameters(const cv::Matx33f& intrinsic_matrix, float& out_focal_length_x,
											float& out_focal_length_y, float& out_principal_point_x,
											float& out_principal_point_y, float& out_skew);
bool computeOpenCVCameraRectification(VideoSourceComponentPtr videoSource, VideoFrameSection section,
									  cv::Matx33d& rotationOut, cv::Matx34d& projectionOut);

void createDefautMonoIntrinsics(int pixelWidth, int pixelHeight, struct MikanMonoIntrinsics& outIntrinsics);
// Intrinsics for a distortion-free pinhole camera reported as focal lengths and a
// principal point in pixels (what ARKit and a recorded pose track carry). Fills
// both camera matrices identically and derives the field of view from them.
void createMonoIntrinsicsFromPinhole(int pixelWidth, int pixelHeight, double fx, double fy, double cx, double cy,
									 struct MikanMonoIntrinsics& outIntrinsics);

void computeOpenGLProjMatFromCameraIntrinsics(const struct MikanMonoIntrinsics& intrinsics, glm::mat4& outProjection,
											  int* outViewport= nullptr);

enum class eStereoIntrinsicsSide
{
	left,
	right
};
void computeOpenGLProjMatFromCameraIntrinsics(const struct MikanStereoIntrinsics& intrinsics,
											  eStereoIntrinsicsSide side, glm::mat4& outProjection,
											  int* outViewport= nullptr);

void computeCameraRayAtPixel(const struct MikanMonoIntrinsics& intrinsics, const glm::mat4& cameraXform,
							 const glm::vec2& imagePoint, glm::vec3& outRayStart, glm::vec3& outRayDirection);

/**
 * Computes the camera tracking mount (or "puck") to aperature offset transform
 * from known VR-space tracking mount poses and optical offset from tracking pattern.
 *
 * Params:
 * cameraPuckXform_VRSpace: camera's tracking mount pose in VR tracking space
 * matPuckXform_VRSpace:    mat's tracking mount pose in VR tracking space
 * apertureToPatternXform:    camera-to-pattern transform from solvePnP (optical measurement)
 * matPuckOffsetMM:         physical offset (X, Y, Z) in mm from mat puck origin to pattern origin
 *
 * Return: relative offset of the aperture from its tracking puck
 */
struct CameraPuckToApertureResults
{
	glm::dmat4 patternXform_VRSpace;
	glm::dmat4 apertureXform_VRSpace;
	glm::dmat4 apertureToPatternXform_CameraSpace;
	glm::dmat4 apertureToMatPuckXform_CameraSpace;
	glm::dmat4 cameraPuckToApertureXform;
	bool bIsValid= false;

	void reset()
	{
		patternXform_VRSpace= glm::dmat4(1.0);
		apertureXform_VRSpace= glm::dmat4(1.0);
		apertureToPatternXform_CameraSpace= glm::dmat4(1.0);
		apertureToMatPuckXform_CameraSpace= glm::dmat4(1.0);
		cameraPuckToApertureXform= glm::dmat4(1.0);
		bIsValid= false;
	}
};

void computeCameraPuckToApertureXform(const glm::dmat4& cameraPuckXform_VRSpace, const glm::dmat4& matPuckXform_VRSpace,
									  const glm::dmat4& apertureToPatternXform, const glm::dvec3& matPuckOffsetMM,
									  CameraPuckToApertureResults& outResults);