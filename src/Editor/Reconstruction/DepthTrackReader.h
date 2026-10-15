#pragma once

#include "CaptureArchiveReader.h"

#include <opencv2/core.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>

// Reads the two depth members of a trackcamera capture: the depth member holds
// one width x height block of little-endian uint16 depth units per movie
// frame, the confidence member one block of uint8 confidence (0 low, 1 medium,
// 2 high). Record index equals the frame's file index in the movie's pose
// track, which declares the width, height, and unit.
class DepthTrackReader
{
public:
	// Opens the archive and checks both members' sizes against recordCount
	// records of width x height. Logs and returns false on any mismatch.
	bool open(const std::filesystem::path& archivePath, const CaptureArchiveMember& depthMember,
			  const CaptureArchiveMember& confidenceMember, int width, int height, double depthUnitMm,
			  size_t recordCount);
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
	std::ifstream m_file;
	uint64_t m_depthOffset= 0;
	uint64_t m_confidenceOffset= 0;
	int m_width= 0;
	int m_height= 0;
	double m_depthUnitMm= 1.0;
	size_t m_recordCount= 0;
	bool m_bIsOpen= false;
};
