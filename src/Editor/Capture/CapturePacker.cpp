#include "CapturePacker.h"
#include "CaptureArchiveWriter.h"
#include "Logger.h"
#include "MovieDecoder.h"

#include <system_error>

namespace
{
std::filesystem::path findMediaByStem(const std::filesystem::path& workingDir, const char* stem, bool bMovie)
{
	std::error_code ec;
	for (const auto& entry : std::filesystem::directory_iterator(workingDir, ec))
	{
		if (!entry.is_regular_file(ec))
			continue;

		const std::filesystem::path& path= entry.path();
		if (path.stem().string() != stem)
			continue;
		if (bMovie ? MovieDecoder::isMovieExtension(path) : MovieDecoder::isImageExtension(path))
			return path;
	}

	return std::filesystem::path();
}

std::filesystem::path findNamed(const std::filesystem::path& workingDir, const char* name)
{
	std::error_code ec;
	const std::filesystem::path path= workingDir / name;
	return std::filesystem::is_regular_file(path, ec) ? path : std::filesystem::path();
}
} // namespace

std::filesystem::path CapturePacker::findMemberFile(const std::filesystem::path& workingDir, eCaptureMemberRole role)
{
	switch (role)
	{
	case eCaptureMemberRole::plate_image:
		return findMediaByStem(workingDir, k_plateStem, false);
	case eCaptureMemberRole::marker_image:
		return findMediaByStem(workingDir, k_markerStem, false);
	case eCaptureMemberRole::marker_pose:
		return findNamed(workingDir, "marker.pose.json");
	case eCaptureMemberRole::movie:
		return findMediaByStem(workingDir, k_movieStem, true);
	case eCaptureMemberRole::movie_pose:
		return findNamed(workingDir, "movie.pose.json");
	case eCaptureMemberRole::depth:
		return findNamed(workingDir, "movie.depth.bin");
	case eCaptureMemberRole::confidence:
		return findNamed(workingDir, "movie.conf.bin");
	default:
		return std::filesystem::path();
	}
}

bool CapturePacker::pack(const CapturePackRequest& request, std::string& outError)
{
	CaptureManifest manifest;
	manifest.kind= request.kind;
	manifest.sessionId= request.sessionId;
	manifest.poseSpace= request.poseSpace;
	manifest.createdUtc= CaptureManifest::makeUtcTimestampNow();
	manifest.producer= request.producer;
	manifest.bInterrupted= request.bInterrupted;

	std::filesystem::path memberFiles[(int)eCaptureMemberRole::COUNT];
	for (int index= 0; index < (int)eCaptureMemberRole::COUNT; ++index)
	{
		const eCaptureMemberRole role= (eCaptureMemberRole)index;
		memberFiles[index]= findMemberFile(request.workingDir, role);
		if (!memberFiles[index].empty())
		{
			manifest.members[role]= memberFiles[index].filename().string();
		}
	}

	// A pose track or a depth member that outlived its media says nothing on its own
	if (!manifest.hasMember(eCaptureMemberRole::marker_image))
		manifest.members.erase(eCaptureMemberRole::marker_pose);
	if (!manifest.hasMember(eCaptureMemberRole::movie))
	{
		manifest.members.erase(eCaptureMemberRole::movie_pose);
		manifest.members.erase(eCaptureMemberRole::depth);
		manifest.members.erase(eCaptureMemberRole::confidence);
	}

	// The manifest's own validation names the missing required member
	CaptureArchiveWriter writer;
	if (!writer.open(request.outputPath, manifest, outError))
		return false;

	for (const auto& entry : manifest.members)
	{
		const std::filesystem::path& file= memberFiles[(int)entry.first];
		if (!writer.addMemberFromFile(entry.second, file, outError))
			return false;
	}

	if (!writer.finish(outError))
		return false;

	MIKAN_MT_LOG_INFO("CapturePacker") << "Packed " << request.outputPath.filename() << " with "
									   << manifest.members.size() << " member(s)";
	return true;
}
