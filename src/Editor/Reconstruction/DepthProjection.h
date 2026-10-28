#pragma once

#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

// A camera pose made ready for projecting world points into its depth image:
// the inverse is taken once per view rather than once per point
struct DepthViewFrame
{
	glm::mat4 worldToCamera= glm::mat4(1.f); // OpenCV axes (+Y down, +Z forward)
	glm::vec3 cameraPosition= glm::vec3(0.f);
};

// The one projection of a world point into a frame's depth image that the
// passes judging a mesh against a take's depth share, so they agree on which
// pixel a point lands on
class DepthProjection
{
public:
	// cameraToWorld is in OpenCV axes
	static DepthViewFrame makeFrame(const glm::mat4& cameraToWorld);

	// The point's depth along the view's axis and its nearest pixel in a
	// width x height depth image, false when it falls behind the camera or off
	// the image
	static bool projectToPixel(const DepthViewFrame& frame, const PinholeIntrinsics& depthIntrinsics, int width,
							   int height, const glm::vec3& worldPoint, float& outDepth, int& outU, int& outV);
};
