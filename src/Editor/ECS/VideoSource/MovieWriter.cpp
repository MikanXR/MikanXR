#include "MovieWriter.h"
#include "Logger.h"

#include <system_error>

namespace
{
struct MovieWriterBackend
{
	const char* name;
	int apiPreference;
	int fourcc;
	const char* extension;
};

// Most likely H.264 in an mp4 first. The ffmpeg DLL lists libopenh264 ahead of its
// Media Foundation encoder, so its avc1 route fails without an OpenH264 DLL beside
// the executable and falls through to MPEG-4 part 2.
const MovieWriterBackend k_backends[]= {
	{"msmf-h264", cv::CAP_MSMF, cv::VideoWriter::fourcc('H', '2', '6', '4'), ".mp4"},
	{"ffmpeg-avc1", cv::CAP_FFMPEG, cv::VideoWriter::fourcc('a', 'v', 'c', '1'), ".mp4"},
	{"ffmpeg-mp4v", cv::CAP_FFMPEG, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), ".mp4"},
	{"ffmpeg-mjpg", cv::CAP_FFMPEG, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), ".avi"},
};
} // namespace

MovieWriter::~MovieWriter() { close(); }

bool MovieWriter::open(const std::filesystem::path& stemPath, int width, int height, double fps)
{
	close();

	if (width <= 0 || height <= 0 || fps <= 0.0)
	{
		MIKAN_LOG_ERROR("MovieWriter::open") << "Bad movie size or rate: " << width << "x" << height << " @ " << fps;
		return false;
	}

	for (const MovieWriterBackend& backend : k_backends)
	{
		std::filesystem::path path= stemPath;
		path+= backend.extension;

		// A leftover from a failed earlier attempt with another extension is not
		// this movie; only the path this backend writes is cleared
		std::error_code ec;
		std::filesystem::remove(path, ec);

		if (m_writer.open(path.string(), backend.apiPreference, backend.fourcc, fps, cv::Size(width, height), true)
			&& m_writer.isOpened())
		{
			m_path= path;
			m_backendName= backend.name;
			m_width= width;
			m_height= height;
			m_fps= fps;
			m_frameCount= 0;
			return true;
		}

		MIKAN_LOG_INFO("MovieWriter::open") << "Backend " << backend.name << " declined " << path.filename();
		std::filesystem::remove(path, ec);
	}

	MIKAN_LOG_ERROR("MovieWriter::open") << "No video writer backend could open " << stemPath;
	return false;
}

bool MovieWriter::write(const cv::Mat& bgr)
{
	if (!isOpen())
		return false;

	if (bgr.type() != CV_8UC3 || bgr.cols != m_width || bgr.rows != m_height)
	{
		MIKAN_LOG_ERROR("MovieWriter::write") << "Frame " << bgr.cols << "x" << bgr.rows << " type " << bgr.type()
											  << " does not match the movie " << m_width << "x" << m_height;
		return false;
	}

	m_writer.write(bgr);
	++m_frameCount;
	return true;
}

bool MovieWriter::close()
{
	if (!m_writer.isOpened())
		return false;

	m_writer.release();

	std::error_code ec;
	const bool bLanded=
		m_frameCount > 0 && std::filesystem::exists(m_path, ec) && std::filesystem::file_size(m_path, ec) > 0;
	if (!bLanded)
	{
		MIKAN_LOG_ERROR("MovieWriter::close")
			<< "Movie did not land: " << m_path << " after " << m_frameCount << " frame(s) through " << m_backendName;
	}

	return bLanded;
}

bool MovieWriter::isOpen() const { return m_writer.isOpened(); }
