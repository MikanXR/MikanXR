#pragma once

#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

#include <cstdint>
#include <filesystem>

// Synchronous, single-owner reader over a movie file or a still image. A still
// is treated as a one-frame movie so callers have a single path: it reports
// a nominal frame rate, a frame count of one, and hands back the same frame on
// every read. Frames come out as continuous CV_8UC3 BGR.
class MovieDecoder
{
public:
	static constexpr double k_stillNominalFps= 30.0;

	static bool isImageExtension(const std::filesystem::path& path);
	static bool isMovieExtension(const std::filesystem::path& path);

	MovieDecoder()= default;
	~MovieDecoder();
	MovieDecoder(const MovieDecoder&)= delete;
	MovieDecoder& operator=(const MovieDecoder&)= delete;

	// Picks the image or the movie backend by extension. Logs and returns false
	// when the file cannot be read.
	bool open(const std::filesystem::path& absolutePath);
	void close();
	bool isOpen() const;
	bool isStill() const;

	int getWidth() const;
	int getHeight() const;
	// The container's frame rate, or k_stillNominalFps for a still or a
	// container that does not report one
	double getFps() const;
	// frameCount / fps for a movie, zero for a still
	double getDurationSeconds() const;
	int64_t getFrameCount() const;

	// The next readNext returns the first frame at or after timeSeconds. A still
	// ignores it and returns true.
	bool seek(double timeSeconds);
	// Sequential read. Returns false at the end of the stream. A still returns
	// its frame on every call with a pts of zero.
	bool readNext(cv::Mat& outBgr, int64_t& outPtsUs);

private:
	bool openStill(const std::filesystem::path& absolutePath);
	bool openMovie(const std::filesystem::path& absolutePath);

	static void convertToBgr(const cv::Mat& source, cv::Mat& outBgr);

	cv::VideoCapture m_capture;
	cv::Mat m_stillFrame;
	bool m_bIsOpen= false;
	bool m_bIsStill= false;
	int m_width= 0;
	int m_height= 0;
	double m_fps= 0.0;
	int64_t m_frameCount= 0;
};
