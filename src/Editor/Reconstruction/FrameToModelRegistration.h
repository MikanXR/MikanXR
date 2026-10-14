#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <limits>

struct RegistrationConfig
{
	int iterations= 10;
	float maxCorrespondenceM= 0.10f; // tightens linearly to a third over the iterations
	float maxNormalAngleDeg= 30.f;
	float huberM= 0.02f;
	float dataSigmaM= 0.005f;  // point-to-plane noise of one pixel, the unit the prior is read against
	float priorSigmaM= 0.003f; // how far the data may pull the pose from the prior per frame
	float priorSigmaDeg= 0.2f;
	int minCorrespondences= 2000;
	float maxCorrectionM= 0.15f; // a solve that moves further than this keeps the prior
	float maxCorrectionDeg= 5.f;
	float depthMin= 0.1f;
	float depthMax= 5.f;
};

struct RegistrationResult
{
	glm::mat4 cameraToWorld= glm::mat4(1.f); // refined, or the prior when rejected
	bool bAccepted= false;
	int iterations= 0;
	int correspondences= 0;
	float rmsBeforeM= std::numeric_limits<float>::quiet_NaN();
	float rmsAfterM= std::numeric_limits<float>::quiet_NaN();
	float translationM= 0.f; // size of the correction against the prior
	float rotationDeg= 0.f;
};

// Projective point-to-plane ICP of one depth frame against a ray cast of
// the model. Every camera here uses OpenCV axes. The model map was cast from
// modelCameraToWorld, normally the prior itself; each frame point under the
// current estimate projects into that camera to find its model point, so the
// association costs one projection per point and no search. A prior term on
// the deviation from priorCameraToWorld holds the directions a frame's
// geometry leaves unconstrained, such as along a lone wall or floor.
class FrameToModelRegistration
{
public:
	// Camera-space points of a depth map, CV_32FC3, through integer pixel
	// positions (u - cx) / fx * z. Rows with depth 0 give (0, 0, 0).
	static void unproject(const cv::Mat& depthMeters, const PinholeIntrinsics& intrinsics, cv::Mat& outPoints);

	// Per-pixel normals of a camera-space point map (CV_32FC3) from the
	// central differences of its neighbours, pointing toward the camera.
	// outOk (CV_8UC1) is 0 where a neighbour is invalid or lies across a
	// depth jump of more than a tenth of the depth.
	static void computeFrameNormals(const cv::Mat& points, const cv::Mat& valid, cv::Mat& outNormals, cv::Mat& outOk);

	static RegistrationResult registerFrame(const cv::Mat& depthMeters, const cv::Mat& weight,
											const PinholeIntrinsics& intrinsics, const glm::mat4& priorCameraToWorld,
											const ModelMap& model, const glm::mat4& modelCameraToWorld,
											const RegistrationConfig& config);
};
