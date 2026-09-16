#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

// Single-owner writer of a constant frame rate movie from BGR frames, the file
// MovieDecoder reads back. Frames go in on whichever thread owns the writer.
//
// The container is picked by trying the backends this OpenCV build ships, in
// the order most likely to yield H.264 in an mp4: the Media Foundation plugin
// first, then the ffmpeg DLL's own H.264 route (which needs an OpenH264 DLL that
// does not ship here, so it is expected to fail), then ffmpeg MPEG-4 part 2 in
// an mp4, and last MJPG in an avi. The chosen extension is appended to the stem
// the caller passes, so the caller learns the final path from getPath(). The
// compositor's eSupportedCodec table in CompositorConstants.h is the other
// fourcc list in the tree; it serves the compositor output and is left alone.
class MovieWriter
{
public:
	MovieWriter()= default;
	~MovieWriter();
	MovieWriter(const MovieWriter&)= delete;
	MovieWriter& operator=(const MovieWriter&)= delete;

	// stemPath is the destination without an extension. Logs and returns false
	// when no backend opens.
	bool open(const std::filesystem::path& stemPath, int width, int height, double fps);
	// Appends one CV_8UC3 BGR frame of the opened size
	bool write(const cv::Mat& bgr);
	// Finalizes the container. Returns false when nothing was written or the
	// file did not land.
	bool close();
	bool isOpen() const;

	const std::filesystem::path& getPath() const { return m_path; }
	const std::string& getBackendName() const { return m_backendName; }
	int64_t getFrameCount() const { return m_frameCount; }
	double getFps() const { return m_fps; }

private:
	cv::VideoWriter m_writer;
	std::filesystem::path m_path;
	std::string m_backendName;
	int m_width= 0;
	int m_height= 0;
	double m_fps= 0.0;
	int64_t m_frameCount= 0;
};
