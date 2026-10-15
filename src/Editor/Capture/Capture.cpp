#include "Capture.h"
#include "Logger.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <system_error>

bool Capture::open(const std::filesystem::path& path, std::string& outError)
{
	close();

	const eCaptureKind extensionKind= CaptureKindUtils::kindFromExtension(path);
	if (extensionKind == eCaptureKind::INVALID)
	{
		outError= path.filename().string() + " does not have a capture extension";
		MIKAN_LOG_ERROR("Capture::open") << outError;
		return false;
	}

	if (!m_reader.open(path, outError))
		return false;

	std::string manifestJson;
	if (!m_reader.readMemberString(CaptureManifest::k_manifestMemberName, manifestJson, outError)
		|| !m_manifest.loadFromJsonString(manifestJson, outError))
	{
		MIKAN_LOG_ERROR("Capture::open") << path.string() << ": " << outError;
		close();
		return false;
	}

	if (m_manifest.kind != extensionKind)
	{
		outError= std::string("The manifest says ") + CaptureKindUtils::kindToString(m_manifest.kind)
				  + " but the extension says " + CaptureKindUtils::kindToString(extensionKind);
		MIKAN_LOG_ERROR("Capture::open") << path.string() << ": " << outError;
		close();
		return false;
	}

	for (const std::string& name : m_manifest.getMemberNames())
	{
		if (m_reader.findMember(name) == nullptr)
		{
			outError= "The manifest names " + name + " but the archive has no such member";
			MIKAN_LOG_ERROR("Capture::open") << path.string() << ": " << outError;
			close();
			return false;
		}
	}

	if (!loadPoseTrack(eCaptureMemberRole::movie_pose, m_moviePoseTrack, outError)
		|| !loadPoseTrack(eCaptureMemberRole::marker_pose, m_markerPoseTrack, outError))
	{
		MIKAN_LOG_ERROR("Capture::open") << path.string() << ": " << outError;
		close();
		return false;
	}

	return true;
}

bool Capture::loadPoseTrack(eCaptureMemberRole role, PoseTrack& outTrack, std::string& outError)
{
	outTrack.clear();
	if (!m_manifest.hasMember(role))
		return true;

	const std::string memberName= m_manifest.getMemberName(role);
	std::string json;
	if (!m_reader.readMemberString(memberName, json, outError))
		return false;

	try
	{
		const configuru::Config config= configuru::parse_string(json.c_str(), configuru::JSON, memberName.c_str());
		if (!outTrack.loadFromJSON(config))
		{
			outError= "Member " + memberName + " is not a valid pose track";
			return false;
		}
	}
	catch (std::exception& e)
	{
		outError= "Member " + memberName + " is not valid JSON: " + e.what();
		return false;
	}

	// One session for every track in the archive, so an offset solved from the
	// marker track applies to the movie track
	if (outTrack.getSessionId() != m_manifest.sessionId)
	{
		outError= "Member " + memberName + " was recorded in session " + outTrack.getSessionId()
				  + ", not the manifest's " + m_manifest.sessionId;
		outTrack.clear();
		return false;
	}

	return true;
}

void Capture::close()
{
	m_reader.close();
	m_manifest.clear();
	m_moviePoseTrack.clear();
	m_markerPoseTrack.clear();
}

bool Capture::hasMember(eCaptureMemberRole role) const { return m_manifest.hasMember(role); }

const CaptureArchiveMember* Capture::findMember(eCaptureMemberRole role) const
{
	if (!m_manifest.hasMember(role))
		return nullptr;

	return m_reader.findMember(m_manifest.getMemberName(role));
}

bool Capture::readMemberBytes(eCaptureMemberRole role, std::vector<uint8_t>& outBytes, std::string& outError)
{
	const CaptureArchiveMember* member= findMember(role);
	if (member == nullptr)
	{
		outError= std::string("Capture has no ") + CaptureKindUtils::roleToString(role) + " member";
		return false;
	}

	return m_reader.readMember(*member, outBytes, outError);
}

const PoseTrack& Capture::getPoseTrack(eCaptureMemberRole role) const
{
	static const PoseTrack s_emptyTrack;

	switch (role)
	{
	case eCaptureMemberRole::movie_pose:
		return m_moviePoseTrack;
	case eCaptureMemberRole::marker_pose:
		return m_markerPoseTrack;
	default:
		return s_emptyTrack;
	}
}

bool Capture::decodeImage(eCaptureMemberRole role, cv::Mat& outBgr, std::string& outError)
{
	std::vector<uint8_t> bytes;
	if (!readMemberBytes(role, bytes, outError))
		return false;

	const cv::Mat image= cv::imdecode(cv::Mat(1, (int)bytes.size(), CV_8UC1, bytes.data()), cv::IMREAD_COLOR);
	if (image.empty())
	{
		outError= std::string("Member ") + m_manifest.getMemberName(role) + " did not decode as an image";
		MIKAN_LOG_ERROR("Capture::decodeImage") << getPath().string() << ": " << outError;
		return false;
	}

	if (image.channels() == 3 && image.depth() == CV_8U && image.isContinuous())
	{
		outBgr= image;
	}
	else
	{
		cv::Mat bgr;
		if (image.channels() == 1)
			cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR);
		else if (image.channels() == 4)
			cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);
		else
			bgr= image;
		if (bgr.depth() != CV_8U)
			bgr.convertTo(bgr, CV_8U);
		bgr.copyTo(outBgr);
	}

	return true;
}

std::string Capture::makeSubfileUrl(eCaptureMemberRole role) const
{
	const CaptureArchiveMember* member= findMember(role);
	if (member == nullptr)
		return std::string();

	return makeSubfileUrl(getPath(), *member);
}

std::string Capture::makeSubfileUrl(const std::filesystem::path& archivePath, const CaptureArchiveMember& member)
{
	// ffmpeg's subfile protocol: the byte range comes first as options, then
	// the path after the lone colon. The end offset is exclusive.
	std::error_code ignored;
	std::filesystem::path absolutePath= std::filesystem::absolute(archivePath, ignored);
	if (absolutePath.empty())
		absolutePath= archivePath;

	return "subfile,,start," + std::to_string(member.dataOffset) + ",end,"
		   + std::to_string(member.dataOffset + member.size) + ",,:" + absolutePath.string();
}
