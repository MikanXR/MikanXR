#include "PoseTrack.h"
#include "Logger.h"
#include "PathUtils.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

bool PoseTrack::loadFromFile(const std::filesystem::path& path)
{
	clear();

	const std::filesystem::path absPath= PathUtils::resolveProjectResource(path);
	if (absPath.empty())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromFile") << "Pose track file does not exist: " << path;
		return false;
	}

	try
	{
		const configuru::Config pt= configuru::parse_file(absPath.string(), configuru::JSON);
		return loadFromJSON(pt);
	}
	catch (std::exception& e)
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromFile") << "Failed to parse pose track: " << absPath << " - " << e.what();
		clear();
		return false;
	}
}

bool PoseTrack::loadFromJSON(const configuru::Config& pt)
{
	clear();

	if (!pt.is_object())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track root is not an object";
		return false;
	}

	if (!pt.has_key("format") || !pt["format"].is_string() || pt["format"].as_string() != k_formatName)
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track format is missing or not " << k_formatName;
		return false;
	}

	if (!pt.has_key("version") || !pt["version"].is_int() || pt["version"].as_integer<int>() != k_supportedVersion)
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track version is missing or not " << k_supportedVersion;
		return false;
	}

	if (!pt.has_key("session_id") || !pt["session_id"].is_string() || pt["session_id"].as_string().empty())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track session_id is missing";
		return false;
	}

	if (!pt.has_key("image_width") || !pt["image_width"].is_int() || !pt.has_key("image_height")
		|| !pt["image_height"].is_int())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track image size is missing";
		return false;
	}

	const int imageWidth= pt["image_width"].as_integer<int>();
	const int imageHeight= pt["image_height"].as_integer<int>();
	if (imageWidth <= 0 || imageHeight <= 0)
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON")
			<< "Pose track image size is not positive: " << imageWidth << "x" << imageHeight;
		return false;
	}

	if (!pt.has_key("frames") || !pt["frames"].is_array())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Pose track frames array is missing";
		return false;
	}

	std::vector<PoseTrackFrame> frames;
	frames.reserve(pt["frames"].array_size());
	for (const configuru::Config& frameConfig : pt["frames"].as_array())
	{
		PoseTrackFrame frame;
		if (!readFrame(frameConfig, frames.size(), frame))
		{
			return false;
		}

		frames.push_back(frame);
	}

	const bool bSorted=
		std::is_sorted(frames.begin(), frames.end(),
					   [](const PoseTrackFrame& a, const PoseTrackFrame& b) { return a.timeUs < b.timeUs; });
	if (!bSorted)
	{
		MIKAN_LOG_WARNING("PoseTrack::loadFromJSON") << "Pose track frames were not in time order; sorting";
		std::stable_sort(frames.begin(), frames.end(),
						 [](const PoseTrackFrame& a, const PoseTrackFrame& b) { return a.timeUs < b.timeUs; });
	}

	m_sessionId= pt["session_id"].as_string();
	m_imageWidth= imageWidth;
	m_imageHeight= imageHeight;
	m_frames= std::move(frames);
	m_bIsLoaded= true;
	return true;
}

bool PoseTrack::readFrame(const configuru::Config& frameConfig, size_t frameIndex, PoseTrackFrame& outFrame)
{
	if (!frameConfig.is_object())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Frame " << frameIndex << " is not an object";
		return false;
	}

	if (!frameConfig.has_key("time_us") || !frameConfig["time_us"].is_int())
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Frame " << frameIndex << " has no time_us";
		return false;
	}

	if (!frameConfig.has_key("transform") || !frameConfig["transform"].is_array()
		|| frameConfig["transform"].array_size() != 16)
	{
		MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Frame " << frameIndex << " transform is not 16 numbers";
		return false;
	}

	for (const char* key : {"fx", "fy", "cx", "cy"})
	{
		if (!frameConfig.has_key(key) || !frameConfig[key].is_number())
		{
			MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Frame " << frameIndex << " has no " << key;
			return false;
		}
	}

	float t[16];
	for (int index= 0; index < 16; ++index)
	{
		const configuru::Config& element= frameConfig["transform"][index];
		if (!element.is_number())
		{
			MIKAN_LOG_ERROR("PoseTrack::loadFromJSON") << "Frame " << frameIndex << " transform is not 16 numbers";
			return false;
		}

		t[index]= element.as_float();
	}

	outFrame.timeUs= frameConfig["time_us"].as_integer<int64_t>();
	outFrame.captureTimestampUs=
		frameConfig.has_key("capture_timestamp_us") && frameConfig["capture_timestamp_us"].is_int()
			? frameConfig["capture_timestamp_us"].as_integer<int64_t>()
			: 0;
	// The file stores the camera-to-world transform row-major (t[0..3] is the
	// first row) while glm::mat4 is built column by column, so the matrix is
	// transposed here rather than copied
	outFrame.cameraToWorld=
		glm::mat4(t[0], t[4], t[8], t[12], t[1], t[5], t[9], t[13], t[2], t[6], t[10], t[14], t[3], t[7], t[11], t[15]);
	outFrame.fx= frameConfig["fx"].as_double();
	outFrame.fy= frameConfig["fy"].as_double();
	outFrame.cx= frameConfig["cx"].as_double();
	outFrame.cy= frameConfig["cy"].as_double();
	return true;
}

bool PoseTrack::isLoaded() const { return m_bIsLoaded; }

void PoseTrack::clear()
{
	m_sessionId.clear();
	m_imageWidth= 0;
	m_imageHeight= 0;
	m_frames.clear();
	m_bIsLoaded= false;
}

const std::string& PoseTrack::getSessionId() const { return m_sessionId; }

int PoseTrack::getImageWidth() const { return m_imageWidth; }

int PoseTrack::getImageHeight() const { return m_imageHeight; }

size_t PoseTrack::getFrameCount() const { return m_frames.size(); }

const PoseTrackFrame& PoseTrack::getFrame(size_t index) const { return m_frames[index]; }

bool PoseTrack::findNearestFrame(int64_t timeUs, int64_t toleranceUs, size_t& outIndex) const
{
	if (m_frames.empty())
		return false;

	// The first frame at or after the time, then the earlier neighbour if it
	// is closer
	auto upper= std::lower_bound(m_frames.begin(), m_frames.end(), timeUs,
								 [](const PoseTrackFrame& frame, int64_t value) { return frame.timeUs < value; });
	size_t candidate= (size_t)std::distance(m_frames.begin(), upper);
	if (candidate == m_frames.size())
	{
		candidate= m_frames.size() - 1;
	}
	else if (candidate > 0)
	{
		const int64_t afterDelta= m_frames[candidate].timeUs - timeUs;
		const int64_t beforeDelta= timeUs - m_frames[candidate - 1].timeUs;
		if (beforeDelta <= afterDelta)
		{
			candidate-= 1;
		}
	}

	const int64_t delta= std::llabs(m_frames[candidate].timeUs - timeUs);
	if (delta > toleranceUs)
		return false;

	outIndex= candidate;
	return true;
}

int64_t PoseTrack::getMedianFramePeriodUs() const
{
	if (m_frames.size() < 2)
		return 0;

	std::vector<int64_t> periods;
	periods.reserve(m_frames.size() - 1);
	for (size_t index= 1; index < m_frames.size(); ++index)
	{
		periods.push_back(m_frames[index].timeUs - m_frames[index - 1].timeUs);
	}

	const size_t middle= periods.size() / 2;
	std::nth_element(periods.begin(), periods.begin() + middle, periods.end());
	return periods[middle];
}
