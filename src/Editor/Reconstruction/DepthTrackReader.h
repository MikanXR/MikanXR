#pragma once

#include <opencv2/core.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>

// Reads the two binary depth sidecars a phone writes beside a recorded movie:
// <stem>.depth.bin holds one width x height block of little-endian uint16
// depth units per movie frame, <stem>.conf.bin one block of uint8 confidence
// (0 low, 1 medium, 2 high). Record index equals the frame's file index in
// the pose track, which declares the width, height, and unit.
class DepthTrackReader
{
public:
	// The sidecars sit beside the media and share its stem, so a repointed
	// pose track path never changes where they are found
	static std::filesystem::path depthSidecarPathForMedia(const std::filesystem::path& mediaPath);
	static std::filesystem::path confidenceSidecarPathForMedia(const std::filesystem::path& mediaPath);

	// Opens both files and checks their sizes against recordCount records of
	// width x height. Logs and returns false on any mismatch.
	bool open(const std::filesystem::path& depthPath, const std::filesystem::path& confidencePath, int width,
			  int height, double depthUnitMm, size_t recordCount);
	void close();
	bool isOpen() const { return m_bIsOpen; }

	int getWidth() const { return m_width; }
	int getHeight() const { return m_height; }
	size_t getRecordCount() const { return m_recordCount; }

	// outDepthMeters is CV_32FC1 in metres, 0 where the sensor reported
	// nothing; outConfidence is CV_8UC1. Returns false on a read failure or
	// an index past the end.
	bool readRecord(size_t recordIndex, cv::Mat& outDepthMeters, cv::Mat& outConfidence);

	// Confidence 0, 1, 2 to fusion weight 0, 0.5, 1 (CV_32FC1)
	static void confidenceToWeight(const cv::Mat& confidence, cv::Mat& outWeight);

private:
	std::ifstream m_depthFile;
	std::ifstream m_confidenceFile;
	int m_width= 0;
	int m_height= 0;
	double m_depthUnitMm= 1.0;
	size_t m_recordCount= 0;
	bool m_bIsOpen= false;
};
