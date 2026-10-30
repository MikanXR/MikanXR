#pragma once

#include "opencv2/core.hpp"

#include <array>
#include <cstdint>

// A video source that carries an occlusion matte per frame: a label map (0 removed,
// 1 keep, 2 added) saying where the frame's true silhouettes differ from the
// occlusion mesh's. A file source reads it from the take's matte track; a live
// source would fill it from a segmenter running on its frames.
class IOcclusionMatteSource
{
public:
	virtual ~IOcclusionMatteSource() {}

	// The matte of the frame the source delivered last as a CV_8UC1 image holding
	// labelValues[label] per pixel, with a key that changes when that frame does,
	// so the caller can tell a re-delivered frame from a new one. False when the
	// source has no matte for it.
	virtual bool tryGetLatestFrameMatte(const std::array<uint8_t, 3>& labelValues, cv::Mat& outLabels,
										int64_t& outFrameKey) const= 0;
};
