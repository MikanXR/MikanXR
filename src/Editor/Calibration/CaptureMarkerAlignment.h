#pragma once

#include "ComponentFwd.h"
#include "ProjectConfigConstants.h"
#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

class Capture;
class PoseTrack;
struct CaptureArchiveMember;
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

// The marker shot catches ARKit's world at one instant, and that world moves a
// few millimetres over a take. The take's depth places the table under the
// marker in each frame's own world, so the median over the take is the table's
// height in the world averaged over the take, which is the world the take's
// mesh is fused in. Snapping the shot's stage origin onto that height along the
// marker's normal keeps the shot's tilt, in-plane position, and heading, which
// the shot measures better than a table patch does.
class TableHeightEstimator
{
public:
	// Frames that must see the table around the marker for an estimate
	static constexpr int k_minFrames= 10;

	// worldToStage is the shot's offset, whose origin is the marker centre and
	// whose +Y is the marker's normal. markerLengthM sizes the annulus of table
	// around the marker that is fitted, leaving out the printed square itself.
	TableHeightEstimator(const glm::mat4& worldToStage, float markerLengthM);

	// One frame of the take's depth: metres and confidence at the depth
	// resolution, its depth intrinsics, and its camera-to-world in OpenCV axes.
	// Returns whether the frame saw enough of the table to measure it.
	bool addFrame(const cv::Mat& depthMeters, const cv::Mat& confidence, const PinholeIntrinsics& depthIntrinsics,
				  const glm::mat4& cameraToWorld);

	inline int getFramesUsed() const { return (int)m_heights.size(); }

	// The table's height above the shot's marker centre along its normal, the
	// median over the frames, with the median absolute deviation as its spread.
	// False with fewer than k_minFrames frames.
	bool estimate(float& outHeightM, float& outSpreadM) const;

private:
	glm::vec3 m_markerCentre;
	glm::vec3 m_markerNormal;
	float m_innerRadius;
	float m_outerRadius;
	std::vector<float> m_heights;
};

// The shot's offset moved along the marker's normal by heightM, so the stage
// origin lands on the table
glm::mat4 snapWorldToStageHeight(const glm::mat4& worldToStage, float heightM);

struct TableHeightResult
{
	float heightM= 0.f; // the table above the shot's marker centre, along its normal
	float spreadM= 0.f; // median absolute deviation over the frames
	int framesUsed= 0;
	glm::mat4 snappedWorldToStage= glm::mat4(1.f);
};

// Reads every depth record of a take with its own reader and estimates the
// table height under the shot's marker, so it runs on any thread. track is the
// capture's movie track, whose file indices key the depth records. bCancel,
// when given, is checked between records.
bool snapToTableHeight(const std::filesystem::path& capturePath, const CaptureArchiveMember& depthMember,
					   const CaptureArchiveMember& confidenceMember, const PoseTrack& track,
					   const glm::mat4& worldToStage, float markerLengthM, const std::atomic<bool>* bCancel,
					   TableHeightResult& outResult, std::string& outError);
} // namespace CaptureMarkerAlignment
