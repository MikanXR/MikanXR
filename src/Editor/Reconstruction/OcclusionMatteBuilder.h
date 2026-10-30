#pragma once

#include "MatteTrack.h"
#include "OcclusionPrompts.h"
#include "ReconstructionTypes.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <opencv2/core.hpp>

#include <filesystem>
#include <string>
#include <vector>

class MovieDecoder;
class PoseTrack;
class Sam2Segmenter;

enum class eOcclusionMatteBuildPhase : int
{
	idle,
	decoding,
	complete,
};

struct OcclusionMatteSettings
{
	// Half width of the band around each occluding contour the matte may change, in video pixels.
	// Measured on the can take: 24 leaves a 17 px p90 on the marked frames, 48 gives 4.6, 64 reaches the
	// unbanded 2.9, since the mesh sits up to 30 px off its edge there.
	int bandPx= 64;
	// The matte is built at the video size divided by this: 1, or 2 for half resolution
	int matteScale= 1;
	// Every nth frame is segmented; the frames between take the last segmented labels
	int frameStep= 1;
	// Only frames whose fileIndex lies in [firstFrame, lastFrame] are segmented (lastFrame -1 for the
	// whole take); the others are written as keep. A stretch of a take dumped for inspection.
	int firstFrame= 0;
	int lastFrame= -1;
	// A run whose box is small is prompted on a crop around it, encoded on its own, which resolves a thin
	// part of a small object at the cost of an encoder pass per such run
	bool bCropSmallRuns= true;
	// A run whose chosen mask covers less than this share of the mesh's near side within its band
	// is dropped, and its band stays with the mesh: the segmenter did not find the object the
	// contour belongs to, and whatever it did find would carve that object
	float minRunCoverage= 0.5f;
	OcclusionPromptSettings prompts;
	// Named in the sidecar header
	std::string modelName;
};

// One occlusion mesh as the matte sees it: in the take's ARKit world
struct MatteMesh
{
	const TriangleMesh* mesh= nullptr;
	glm::mat4 meshToTakeWorld= glm::mat4(1.f);
	std::string name;
	std::string modelPath;
};

// One frame as it was processed, for a preview or a dump
struct OcclusionMatteFrame
{
	size_t fileIndex= 0;
	int64_t timeUs= 0;
	cv::Mat bgr;    // CV_8UC3 at the video size
	cv::Mat depth;  // CV_32FC1 metres at the matte size, +inf where no mesh
	cv::Mat labels; // CV_8UC1 at the matte size
	std::vector<OcclusionPromptRun> runs;
	// Per run, how the chosen mask agrees with the mesh's near side inside the run's box: the IoU the
	// choice was made on, the share of the near side the mask covers, and the share of the mask that
	// lies outside the near side. -1 for a run that got no mask.
	std::vector<float> runIoU;
	std::vector<float> runCoverage;
	std::vector<float> runExcess;
	// Per run, the mask its band was judged by (CV_8UC1 at the matte size), empty for a run that got none
	std::vector<cv::Mat> runMasks;
};

// What the builder reports to while it runs, from the worker thread
class IOcclusionMatteBuildObserver
{
public:
	virtual ~IOcclusionMatteBuildObserver() {}
	virtual bool isCancelRequested() const= 0;
	// done and total are frames of the take
	virtual void onProgress(eOcclusionMatteBuildPhase phase, int done, int total)= 0;
	// Every frame the segmenter ran on, after its record was written
	virtual void onFrameProcessed(const OcclusionMatteFrame& frame) {}
};

struct OcclusionMatteBuildStats
{
	int framesDecoded= 0;
	int framesSegmented= 0;
	int framesHeld= 0;      // took the previous frame's labels (the frame step)
	int framesUnmatched= 0; // decoded frames the track has no pose for
	int runsTotal= 0;
	int runsDropped= 0; // runs whose mask covered too little of the mesh to be trusted
	int matteWidth= 0;
	int matteHeight= 0;
	float segmentSeconds= 0.f; // encoder and decoder time
	float totalSeconds= 0.f;
};

struct OcclusionMatteBuildResult
{
	bool bSucceeded= false;
	bool bCancelled= false;
	std::string failureReason;
	std::filesystem::path jsonPath;
	OcclusionMatteBuildStats stats;
};

// Builds a take's occlusion matte: per frame, the meshes rasterized to depth
// at the recorded pose, the occluding contours prompted into the segmenter,
// and the labels written as the frame's record. Pure data in and out: the
// stage and the headless command call build() the same way, with a segmenter
// they started on the calling thread.
class OcclusionMatteBuilder
{
public:
	// decoder must be open on the take's movie; segmenter must be started. Writes the sidecar
	// pair at jsonPath (see MatteTrack::sidecarPathForCapture) with the capture's session id.
	static bool build(const PoseTrack& track, MovieDecoder& decoder, const std::vector<MatteMesh>& meshes,
					  const OcclusionMatteSettings& settings, Sam2Segmenter& segmenter,
					  const std::filesystem::path& jsonPath, IOcclusionMatteBuildObserver* observer,
					  OcclusionMatteBuildResult& outResult);

	// The settings' pixel values scaled to the matte's resolution
	static OcclusionPromptSettings promptSettingsAtScale(const OcclusionMatteSettings& settings);
};
