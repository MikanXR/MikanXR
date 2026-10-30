#pragma once

#include <opencv2/core.hpp>

#include <vector>

// Settings in pixels of the depth map the prompts are found on
struct OcclusionPromptSettings
{
	// A neighbour farther by more than this share of its depth is across an occluding contour
	float depthJump= 0.05f;
	// Half width of the band around a contour inside which the matte may change the mesh
	int bandPx= 24;
	// Contour pixels closer than this join one run, when their depths are within runDepthRatio
	int runGapPx= 9;
	// Contour pixels within the gap whose depths differ by more than this ratio lie on different
	// objects, and a run never joins them: where a can's silhouette meets a stack of paper's, one
	// run along both sees the paper's depth as its near side, prompts the paper, and carves the can
	float runDepthRatio= 1.2f;
	// A shorter run is mesh noise: its two or three points prompt whatever surface they land on
	int runMinPoints= 40;
	// A run whose contour spans more than this is split at the median of its longer side until each
	// part fits, so one prompt never spreads its points over the several objects a merged contour
	// runs along: a can, a laptop and a stack of paper on a desk make one contour against the room
	int runMaxSpanPx= 640;
	// One prompt point per this many contour pixels, at most maxPointsPerRun
	int pointSpacingPx= 40;
	int maxPointsPerRun= 12;
	// A notch into a run's mask narrower than the band over this is filled before the mask may
	// remove: a ragged border where the frame cuts an object, or a hole the print on a label
	// punches, is not an edge the segmenter found, and the mesh keeps it. Adding uses the mask as
	// it is, since the filling would reach across a gap the mask left on purpose
	int maskNotchDivisor= 8;
};

// One occluding contour run of a depth map and the prompt made from it. The
// contour lies on the near side of the depth jump, so the run names the
// occluder whose silhouette it is.
struct OcclusionPromptRun
{
	std::vector<cv::Point> contour;
	// Prompt points inside the occluder, at the depth map's resolution
	std::vector<cv::Point2f> positives;
	// The contour's bounds grown by the band, clipped to the image
	cv::Rect box;
	// Depth separating the run's near side from its far side
	float splitDepth= 0.f;
	// CV_8UC1 at the depth map's size: the covered pixels nearer than splitDepth inside the box.
	// The segmenter's candidate masks are judged against it.
	cv::Mat nearMask;
};

class OcclusionPrompts
{
public:
	// depth is CV_32FC1 eye depth in metres, +inf where nothing drew. Marks the pixels whose
	// 4-neighbour is farther by more than depthJump of that neighbour's depth, or uncovered.
	static void findContours(const cv::Mat& depth, float depthJump, cv::Mat& outContour);

	// The contour runs of a depth map with their prompts. A run without a usable point is dropped.
	static void findRuns(const cv::Mat& depth, const OcclusionPromptSettings& settings,
						 std::vector<OcclusionPromptRun>& outRuns);

	// The matte labels (CV_8UC1: 0 removed, 1 keep, 2 added) from the segmenter's mask for each run
	// (CV_8UC1 at the depth map's size, nonzero where it says occluder; an empty mask skips the run).
	// Everything is keep except inside a run's band where its own mask and the mesh's near side
	// disagree. Each run judges its band by its own mask: another run's mask may well cover this
	// run's band with the surface it was prompted on. A disagreement only ever moves the
	// silhouette: a removed region has to reach the far side of the mesh and an added region has
	// to reach the mesh, so a hole the mask leaves inside an object, or a blob it adds beside one,
	// stays keep. A mask removes only outside its closed form, so a notch into it narrower than
	// the band over maskNotchDivisor stays keep too.
	static void computeLabels(const cv::Mat& depth, const std::vector<OcclusionPromptRun>& runs,
							  const std::vector<cv::Mat>& runMasks, const OcclusionPromptSettings& settings,
							  cv::Mat& outLabels);

	static constexpr uint8_t k_labelRemoved= 0;
	static constexpr uint8_t k_labelKeep= 1;
	static constexpr uint8_t k_labelAdded= 2;
};
