#pragma once

#include "glm/ext/matrix_float4x4.hpp"

#include <configuru.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct PoseTrackFrame
{
	// Position of the record in the file's frames array, which is also its
	// record index in the depth sidecars. Survives the sort by time.
	size_t fileIndex= 0;
	// Relative to the first frame, matching the container's presentation time
	int64_t timeUs= 0;
	// The phone's absolute clock
	int64_t captureTimestampUs= 0;
	// Column-major, already transposed from the file's row-major layout
	glm::mat4 cameraToWorld= glm::mat4(1.f);
	double fx= 0.0;
	double fy= 0.0;
	double cx= 0.0;
	double cy= 0.0;
};

// The per-frame camera pose and pinhole intrinsics a phone writes in a JSON
// sidecar next to a recorded movie or photo
class PoseTrack
{
public:
	inline static const std::string k_formatName= "mikan-pose-track";
	// Version 2 added the optional depth sidecar header; the frame records are
	// unchanged, so both versions load through the same path
	static constexpr int k_minSupportedVersion= 1;
	static constexpr int k_maxSupportedVersion= 2;

	bool loadFromFile(const std::filesystem::path& path);
	// Validates the header and every frame, and sorts the frames by time. On
	// any structural error the track is left empty and false is returned.
	bool loadFromJSON(const configuru::Config& pt);
	bool isLoaded() const;
	void clear();

	const std::string& getSessionId() const;
	int getImageWidth() const;
	int getImageHeight() const;
	// Whether <stem>.depth.bin and <stem>.conf.bin were recorded beside the
	// media: one record per frame, indexed like the frames, sized as below
	bool hasDepthTrack() const;
	int getDepthWidth() const;
	int getDepthHeight() const;
	// Millimetres per depth unit in the uint16 depth records
	double getDepthUnitMm() const;
	size_t getFrameCount() const;
	const PoseTrackFrame& getFrame(size_t index) const;

	// The frame nearest timeUs, or false when it is further than toleranceUs
	// away or the track is empty
	bool findNearestFrame(int64_t timeUs, int64_t toleranceUs, size_t& outIndex) const;
	// Zero when there are fewer than two frames
	int64_t getMedianFramePeriodUs() const;

private:
	static bool readFrame(const configuru::Config& frameConfig, size_t frameIndex, PoseTrackFrame& outFrame);

	std::string m_sessionId;
	int m_imageWidth= 0;
	int m_imageHeight= 0;
	int m_depthWidth= 0;
	int m_depthHeight= 0;
	double m_depthUnitMm= 0.0;
	std::vector<PoseTrackFrame> m_frames;
	bool m_bIsLoaded= false;
};

// Accumulates frames and writes the sidecar PoseTrack reads, with the same keys
// the MikanARStreamer app writes. The transform goes out row-major, the reverse
// of the reader's transpose. Not thread safe: one writer per recording, owned by
// whichever thread appends.
class PoseTrackWriter
{
public:
	PoseTrackWriter(const std::string& sessionId, int imageWidth, int imageHeight);

	// timeUs is relative to the first frame and equals the container's
	// presentation time; captureTimestampUs is the recorder's own clock
	void append(int64_t timeUs, int64_t captureTimestampUs, const glm::mat4& cameraToWorld, double fx, double fy,
				double cx, double cy);
	size_t getFrameCount() const { return m_frames.size(); }
	// Declare the depth sidecars that sit beside the media, which makes the
	// track a version 2 track. Frames must be appended in sidecar record order.
	void setDepthTrack(int depthWidth, int depthHeight, double depthUnitMm);

	configuru::Config toConfig() const;
	// Writes to a sibling temporary file and renames it into place, so a reader
	// never sees a partial track. Logs and returns false on failure.
	bool writeToFile(const std::filesystem::path& path) const;

private:
	std::string m_sessionId;
	int m_imageWidth;
	int m_imageHeight;
	int m_depthWidth= 0;
	int m_depthHeight= 0;
	double m_depthUnitMm= 0.0;
	std::vector<PoseTrackFrame> m_frames;
};
