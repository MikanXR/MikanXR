#pragma once

#include "ComponentFwd.h"

#include <vector>

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/matrix_float4x4.hpp"

// A single natural-feature point triangulated from multiple tracked camera views.
struct NaturalFeaturePoint
{
	glm::vec3 worldPosition= glm::vec3(0.f); // stage/world space, meters
	float meanReprojErrorPx= 0.f;
	int observationCount= 0;
	float maxParallaxDeg= 0.f;
};

// Live statistics surfaced to the GUI so the user knows when coverage is sufficient.
struct CloudBuildStats
{
	int trackedFeatureCount= 0;
	int cloudPointCount= 0;
	int keyframeCount= 0;
	float meanReprojErrorPx= 0.f;
	float coverageMeters= 0.f; // bounding-box diagonal of the accumulated cloud
};

// Builds a sparse world-space point cloud from natural image features tracked across a moving,
// pose-tracked camera. Mirrors the AnchorTriangulator pattern:
// a pimpl state struct, a ctor taking the camera + distortion view, and render* overlays.
class NaturalFeatureCloudBuilder
{
public:
	NaturalFeatureCloudBuilder(CameraComponentPtr cameraComponent, class VideoFrameDistortionView* distortionView);
	virtual ~NaturalFeatureCloudBuilder();

	void resetCaptureState();

	// Detect/track features on the current undistorted frame and triangulate matured tracks.
	// Call once per processed frame from AppStage::update() during capture.
	void processCurrentFrame();

	// Optional 2D region of interest (frame-buffer pixel coords) restricting feature detection/acceptance.
	void setScreenRegionOfInterest(const glm::vec2& minPixel, const glm::vec2& maxPixel);
	void clearScreenRegionOfInterest();

	bool hasUsableCloud() const;
	const std::vector<NaturalFeaturePoint>& getCloudPoints() const;
	CloudBuildStats getStats() const;

	// Rendering overlays (follow AnchorTriangulator::render* conventions)
	void renderTrackedFeatures2d();
	void renderCloudPoints3d();

protected:
	// Internal capture state (pimpl to keep OpenCV out of the header)
	struct NaturalFeatureCloudState* m_state;

	// Camera used for capture
	CameraComponentPtr m_cameraComponent;

	// Video buffer state
	class VideoFrameDistortionView* m_distortionView;
};
