#include "MovieDecoder.h"
#include "Logger.h"

#include "opencv2/opencv.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace
{
std::string lowerExtension(const std::filesystem::path& path)
{
	std::string extension= path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	return extension;
}
} // namespace

bool MovieDecoder::isImageExtension(const std::filesystem::path& path)
{
	const std::string extension= lowerExtension(path);
	return extension == ".jpg" || extension == ".jpeg" || extension == ".png" || extension == ".bmp"
		   || extension == ".tga";
}

bool MovieDecoder::isMovieExtension(const std::filesystem::path& path)
{
	const std::string extension= lowerExtension(path);
	return extension == ".mp4" || extension == ".mov" || extension == ".m4v" || extension == ".mkv"
		   || extension == ".avi";
}

MovieDecoder::~MovieDecoder() { close(); }

bool MovieDecoder::open(const std::filesystem::path& absolutePath)
{
	close();

	bool bSuccess= false;
	if (isImageExtension(absolutePath))
	{
		bSuccess= openStill(absolutePath);
	}
	else if (isMovieExtension(absolutePath))
	{
		bSuccess= openMovie(absolutePath);
	}
	else
	{
		MIKAN_LOG_ERROR("MovieDecoder::open") << "Unsupported file extension: " << absolutePath;
	}

	if (!bSuccess)
	{
		close();
	}

	return bSuccess;
}

bool MovieDecoder::openStill(const std::filesystem::path& absolutePath)
{
	const cv::Mat image= cv::imread(absolutePath.string(), cv::IMREAD_COLOR);
	if (image.empty())
	{
		MIKAN_LOG_ERROR("MovieDecoder::open") << "Failed to read image: " << absolutePath;
		return false;
	}

	convertToBgr(image, m_stillFrame);
	m_width= m_stillFrame.cols;
	m_height= m_stillFrame.rows;
	m_fps= k_stillNominalFps;
	m_frameCount= 1;
	m_bIsStill= true;
	m_bIsOpen= true;
	return true;
}

bool MovieDecoder::openMovie(const std::filesystem::path& absolutePath)
{
	if (!m_capture.open(absolutePath.string(), cv::CAP_FFMPEG))
	{
		MIKAN_LOG_ERROR("MovieDecoder::open") << "Failed to open movie: " << absolutePath;
		return false;
	}

	// Honour the rotation a phone stores in the container so a portrait
	// recording comes out upright
	m_capture.set(cv::CAP_PROP_ORIENTATION_AUTO, 1);

	m_width= (int)m_capture.get(cv::CAP_PROP_FRAME_WIDTH);
	m_height= (int)m_capture.get(cv::CAP_PROP_FRAME_HEIGHT);
	m_fps= m_capture.get(cv::CAP_PROP_FPS);
	if (!(m_fps > 0.0) || !std::isfinite(m_fps))
	{
		m_fps= k_stillNominalFps;
	}
	m_frameCount= (int64_t)std::llround(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
	if (m_frameCount < 0)
	{
		m_frameCount= 0;
	}

	if (m_width <= 0 || m_height <= 0)
	{
		MIKAN_LOG_ERROR("MovieDecoder::open") << "Movie reports no frame size: " << absolutePath;
		return false;
	}

	m_bIsStill= false;
	m_bIsOpen= true;
	return true;
}

void MovieDecoder::close()
{
	if (m_capture.isOpened())
	{
		m_capture.release();
	}
	m_stillFrame.release();
	m_bIsOpen= false;
	m_bIsStill= false;
	m_width= 0;
	m_height= 0;
	m_fps= 0.0;
	m_frameCount= 0;
}

bool MovieDecoder::isOpen() const { return m_bIsOpen; }

bool MovieDecoder::isStill() const { return m_bIsOpen && m_bIsStill; }

int MovieDecoder::getWidth() const { return m_width; }

int MovieDecoder::getHeight() const { return m_height; }

double MovieDecoder::getFps() const { return m_fps; }

double MovieDecoder::getDurationSeconds() const
{
	if (!m_bIsOpen || m_bIsStill || m_fps <= 0.0)
		return 0.0;

	return (double)m_frameCount / m_fps;
}

int64_t MovieDecoder::getFrameCount() const { return m_frameCount; }

bool MovieDecoder::seek(double timeSeconds)
{
	if (!m_bIsOpen)
		return false;

	if (m_bIsStill)
		return true;

	return m_capture.set(cv::CAP_PROP_POS_MSEC, std::max(timeSeconds, 0.0) * 1000.0);
}

bool MovieDecoder::readNext(cv::Mat& outBgr, int64_t& outPtsUs)
{
	if (!m_bIsOpen)
		return false;

	if (m_bIsStill)
	{
		outBgr= m_stillFrame;
		outPtsUs= 0;
		return true;
	}

	if (!m_capture.grab())
		return false;

	// The ffmpeg backend reports the presentation time of the frame grab() just
	// fetched, so it has to be read here, before the next grab moves it on
	outPtsUs= (int64_t)std::llround(m_capture.get(cv::CAP_PROP_POS_MSEC) * 1000.0);

	cv::Mat frame;
	if (!m_capture.retrieve(frame) || frame.empty())
		return false;

	convertToBgr(frame, outBgr);
	return true;
}

void MovieDecoder::convertToBgr(const cv::Mat& source, cv::Mat& outBgr)
{
	cv::Mat bgr;
	switch (source.channels())
	{
	case 1:
		cv::cvtColor(source, bgr, cv::COLOR_GRAY2BGR);
		break;
	case 4:
		cv::cvtColor(source, bgr, cv::COLOR_BGRA2BGR);
		break;
	default:
		bgr= source;
		break;
	}

	if (bgr.depth() != CV_8U)
	{
		bgr.convertTo(bgr, CV_8U);
	}

	if (bgr.isContinuous())
	{
		outBgr= bgr;
	}
	else
	{
		bgr.copyTo(outBgr);
	}
}
