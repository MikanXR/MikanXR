#include "DepthProjection.h"

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/matrix.hpp"

#include <cmath>

DepthViewFrame DepthProjection::makeFrame(const glm::mat4& cameraToWorld)
{
	DepthViewFrame frame;
	frame.worldToCamera= glm::mat4(glm::inverse(glm::dmat4(cameraToWorld)));
	frame.cameraPosition= glm::vec3(cameraToWorld[3]);
	return frame;
}

bool DepthProjection::projectToPixel(const DepthViewFrame& frame, const PinholeIntrinsics& depthIntrinsics, int width,
									 int height, const glm::vec3& worldPoint, float& outDepth, int& outU, int& outV)
{
	const glm::vec3 cameraPoint= glm::vec3(frame.worldToCamera * glm::vec4(worldPoint, 1.f));
	if (!(cameraPoint.z > 0.f))
		return false;

	const double u= depthIntrinsics.fx * cameraPoint.x / cameraPoint.z + depthIntrinsics.cx;
	const double v= depthIntrinsics.fy * cameraPoint.y / cameraPoint.z + depthIntrinsics.cy;
	outU= (int)std::lround(u);
	outV= (int)std::lround(v);
	outDepth= cameraPoint.z;
	return outU >= 0 && outV >= 0 && outU < width && outV < height;
}
