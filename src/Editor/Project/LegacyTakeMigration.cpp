#include "LegacyTakeMigration.h"
#include "CaptureArchiveWriter.h"
#include "Logger.h"
#include "MovieDecoder.h"
#include "PathUtils.h"
#include "PoseTrack.h"
#include "TakeNaming.h"

#include "opencv2/imgcodecs.hpp"

#include <configuru.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include <system_error>
#include <vector>

namespace LegacyTakeMigration
{
namespace
{
const char* k_poseTrackSuffix= ".pose.json";
const char* k_refinedTrackSuffix= ".refined.pose.json";
const char* k_depthSuffix= ".depth.bin";
const char* k_confidenceSuffix= ".conf.bin";
const char* k_legacyFolder= "legacy_takes";
const char* k_capturesFolder= "captures";
const char* k_takeFolders[]= {"movies", "textures"};

// The loose files that share one stem
struct TakeFiles
{
	std::string stem;
	std::filesystem::path media;
	std::filesystem::path poseTrack;
	std::filesystem::path refinedTrack;
	std::filesystem::path depth;
	std::filesystem::path confidence;

	bool hasSidecar() const
	{
		return !poseTrack.empty() || !refinedTrack.empty() || !depth.empty() || !confidence.empty();
	}
	std::vector<std::filesystem::path> allFiles() const
	{
		std::vector<std::filesystem::path> files;
		for (const std::filesystem::path* path : {&media, &poseTrack, &refinedTrack, &depth, &confidence})
		{
			if (!path->empty())
				files.push_back(*path);
		}
		return files;
	}
};

bool endsWith(const std::string& text, const char* suffix)
{
	const size_t suffixLength= std::strlen(suffix);
	return text.size() >= suffixLength && text.compare(text.size() - suffixLength, suffixLength, suffix) == 0;
}

std::string stripSuffix(const std::string& text, const char* suffix)
{
	return text.substr(0, text.size() - std::strlen(suffix));
}

// Groups the top-level files of movies/ and textures/ by stem. A file that is
// neither media nor a sidecar is left alone.
std::map<std::string, TakeFiles> collectTakeFiles(const std::filesystem::path& projectDir)
{
	std::map<std::string, TakeFiles> takes;
	std::error_code ec;

	for (const char* folder : k_takeFolders)
	{
		const std::filesystem::path folderPath= projectDir / folder;
		if (!std::filesystem::is_directory(folderPath, ec))
			continue;

		for (const auto& entry : std::filesystem::directory_iterator(folderPath, ec))
		{
			if (!entry.is_regular_file(ec))
				continue;

			const std::filesystem::path& path= entry.path();
			const std::string filename= path.filename().string();
			if (filename.empty() || filename[0] == '.')
				continue;

			if (endsWith(filename, k_refinedTrackSuffix))
			{
				const std::string stem= stripSuffix(filename, k_refinedTrackSuffix);
				takes[stem].stem= stem;
				takes[stem].refinedTrack= path;
			}
			else if (endsWith(filename, k_poseTrackSuffix))
			{
				const std::string stem= stripSuffix(filename, k_poseTrackSuffix);
				takes[stem].stem= stem;
				takes[stem].poseTrack= path;
			}
			else if (endsWith(filename, k_depthSuffix))
			{
				const std::string stem= stripSuffix(filename, k_depthSuffix);
				takes[stem].stem= stem;
				takes[stem].depth= path;
			}
			else if (endsWith(filename, k_confidenceSuffix))
			{
				const std::string stem= stripSuffix(filename, k_confidenceSuffix);
				takes[stem].stem= stem;
				takes[stem].confidence= path;
			}
			else if (MovieDecoder::isMovieExtension(path) || MovieDecoder::isImageExtension(path))
			{
				const std::string stem= path.stem().string();
				takes[stem].stem= stem;
				if (takes[stem].media.empty())
					takes[stem].media= path;
			}
		}
	}

	return takes;
}

std::string makeStoredPath(const std::filesystem::path& projectDir, const std::filesystem::path& path)
{
	return std::filesystem::relative(path, projectDir).generic_string();
}

std::string makeUtcTimestamp(const std::filesystem::path& path)
{
	std::error_code ec;
	const std::filesystem::file_time_type fileTime= std::filesystem::last_write_time(path, ec);
	if (ec)
		return CaptureManifest::makeUtcTimestampNow();

	const auto systemTime= std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
	const std::time_t seconds= std::chrono::system_clock::to_time_t(systemTime);
	std::tm utcTime{};
#if defined(_WIN32)
	gmtime_s(&utcTime, &seconds);
#else
	gmtime_r(&seconds, &utcTime);
#endif
	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
	return buffer;
}

// Moves a file into legacy_takes/<folder>/<name>, creating the folder
bool moveToLegacy(const std::filesystem::path& projectDir, const std::filesystem::path& path)
{
	if (path.empty())
		return true;

	const std::filesystem::path relative= std::filesystem::relative(path, projectDir);
	const std::filesystem::path destination= projectDir / k_legacyFolder / relative;
	std::error_code ec;
	std::filesystem::create_directories(destination.parent_path(), ec);
	std::filesystem::rename(path, destination, ec);
	if (ec)
	{
		MIKAN_LOG_WARNING("LegacyTakeMigration")
			<< "Could not move " << path.string() << " to " << destination.string() << ": " << ec.message();
		return false;
	}
	return true;
}

// The first frame of a marker movie as a JPEG, and the pose of that frame as
// a one-frame track, so the marker image stands alone in the capture
bool extractMarkerFrame(const std::filesystem::path& moviePath, const PoseTrack& track, std::vector<uint8_t>& outJpeg,
						std::string& outTrackJson)
{
	MovieDecoder decoder;
	if (!decoder.open(moviePath))
		return false;

	cv::Mat frame;
	int64_t ptsUs= 0;
	if (!decoder.readNext(frame, ptsUs) || frame.empty())
		return false;

	if (!cv::imencode(".jpg", frame, outJpeg))
		return false;

	outTrackJson.clear();
	if (track.isLoaded() && track.getFrameCount() > 0)
	{
		const PoseTrackFrame& first= track.getFrame(0);
		PoseTrackWriter writer(track.getSessionId(), track.getImageWidth(), track.getImageHeight());
		writer.append(0, first.captureTimestampUs, first.cameraToWorld, first.fx, first.fy, first.cx, first.cy);
		outTrackJson= configuru::dump_string(writer.toConfig(), configuru::JSON);
	}

	return true;
}

// What a take becomes and which files it consumes
struct PackPlan
{
	eCaptureKind kind= eCaptureKind::INVALID;
	const TakeFiles* main= nullptr;
	const TakeFiles* marker= nullptr;
	PoseTrack mainTrack;
	PoseTrack markerTrack;
	bool bWithDepth= false;
};

bool planTake(const TakeFiles& take, const TakeFiles* marker, PackPlan& outPlan)
{
	outPlan= PackPlan();
	outPlan.main= &take;

	if (!take.poseTrack.empty() && !outPlan.mainTrack.loadFromFile(take.poseTrack))
	{
		MIKAN_LOG_WARNING("LegacyTakeMigration")
			<< "Leaving " << take.media.string() << " loose: its pose track could not be loaded";
		return false;
	}

	if (marker != nullptr && !marker->poseTrack.empty() && !outPlan.markerTrack.loadFromFile(marker->poseTrack))
	{
		MIKAN_LOG_WARNING("LegacyTakeMigration")
			<< "Ignoring the marker take " << marker->media.string() << ": its pose track could not be loaded";
		marker= nullptr;
	}

	// The marker only aligns the take when both were recorded in one session
	if (marker != nullptr && outPlan.mainTrack.isLoaded() && outPlan.markerTrack.isLoaded()
		&& outPlan.mainTrack.getSessionId() != outPlan.markerTrack.getSessionId())
	{
		MIKAN_LOG_WARNING("LegacyTakeMigration")
			<< "Ignoring the marker take " << marker->media.string() << ": it was recorded in session "
			<< outPlan.markerTrack.getSessionId() << ", not the take's " << outPlan.mainTrack.getSessionId();
		marker= nullptr;
	}
	outPlan.marker= marker;

	const bool bIsImage= MovieDecoder::isImageExtension(take.media);
	const bool bHasTrack= outPlan.mainTrack.isLoaded();
	if (bIsImage)
	{
		if (!bHasTrack && marker == nullptr)
			return false; // a raw image stays
		outPlan.kind= eCaptureKind::stillphoto;
	}
	else if (bHasTrack)
	{
		outPlan.kind= eCaptureKind::trackcamera;
		outPlan.bWithDepth= outPlan.mainTrack.hasDepthTrack() && !take.depth.empty() && !take.confidence.empty();
		if (outPlan.mainTrack.hasDepthTrack() && !outPlan.bWithDepth)
		{
			MIKAN_LOG_WARNING("LegacyTakeMigration") << take.media.string()
													 << ": the pose track declares depth but the depth tracks are "
														"not both beside it, so the capture carries none";
		}
	}
	else if (marker != nullptr)
	{
		outPlan.kind= eCaptureKind::stillcamera;
	}
	else
	{
		return false; // a raw movie stays
	}

	return true;
}

std::string lowerExtension(const std::filesystem::path& path)
{
	std::string extension= path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	return extension;
}

// Writes the capture for a plan. Returns the capture path, empty on failure.
std::filesystem::path packTake(const std::filesystem::path& projectDir, const PackPlan& plan)
{
	const TakeFiles& take= *plan.main;

	CaptureManifest manifest;
	manifest.kind= plan.kind;
	manifest.createdUtc= makeUtcTimestamp(take.media);
	manifest.producer= CaptureManifest::makeEditorProducer();

	// The session comes from whichever track exists. A take with no track at
	// all can only have come from the editor's recorder with no camera, so it
	// is stage space under its own stem.
	if (plan.mainTrack.isLoaded())
		manifest.sessionId= plan.mainTrack.getSessionId();
	else if (plan.markerTrack.isLoaded())
		manifest.sessionId= plan.markerTrack.getSessionId();
	else
		manifest.sessionId= take.stem;
	manifest.poseSpace= poseSpaceForSessionId(manifest.sessionId);

	// Members are named by role so the archive reads the same whatever the take
	// was called, keeping the media's own extension
	const std::string mediaExtension= lowerExtension(take.media);
	std::vector<uint8_t> markerJpeg;
	std::string markerTrackJson;
	std::string markerImageName;
	if (plan.kind == eCaptureKind::stillphoto)
	{
		manifest.members[eCaptureMemberRole::plate_image]= "plate" + mediaExtension;
	}
	else
	{
		manifest.members[eCaptureMemberRole::movie]= "movie" + mediaExtension;
	}
	if (plan.kind == eCaptureKind::trackcamera)
	{
		manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
		if (plan.bWithDepth)
		{
			manifest.members[eCaptureMemberRole::depth]= "movie.depth.bin";
			manifest.members[eCaptureMemberRole::confidence]= "movie.conf.bin";
		}
	}
	if (plan.marker != nullptr)
	{
		if (MovieDecoder::isImageExtension(plan.marker->media))
		{
			markerImageName= "marker" + lowerExtension(plan.marker->media);
			manifest.members[eCaptureMemberRole::marker_image]= markerImageName;
			if (plan.markerTrack.isLoaded())
				manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
		}
		else if (extractMarkerFrame(plan.marker->media, plan.markerTrack, markerJpeg, markerTrackJson))
		{
			markerImageName= "marker.jpg";
			manifest.members[eCaptureMemberRole::marker_image]= markerImageName;
			if (!markerTrackJson.empty())
				manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
		}
		else
		{
			MIKAN_LOG_WARNING("LegacyTakeMigration")
				<< "Could not read the first frame of the marker take " << plan.marker->media.string()
				<< ", so the capture carries no marker";
		}
	}

	const std::filesystem::path capturePath=
		projectDir / k_capturesFolder / (take.stem + CaptureKindUtils::kindToExtension(plan.kind));
	std::string error;
	CaptureArchiveWriter writer;
	bool bOk= writer.open(capturePath, manifest, error);
	if (bOk && plan.kind == eCaptureKind::stillphoto)
		bOk= writer.addMemberFromFile(manifest.getMemberName(eCaptureMemberRole::plate_image), take.media, error);
	if (bOk && plan.kind != eCaptureKind::stillphoto)
		bOk= writer.addMemberFromFile(manifest.getMemberName(eCaptureMemberRole::movie), take.media, error);
	if (bOk && manifest.hasMember(eCaptureMemberRole::movie_pose))
		bOk= writer.addMemberFromFile("movie.pose.json", take.poseTrack, error);
	if (bOk && manifest.hasMember(eCaptureMemberRole::depth))
		bOk= writer.addMemberFromFile("movie.depth.bin", take.depth, error)
			 && writer.addMemberFromFile("movie.conf.bin", take.confidence, error);
	if (bOk && manifest.hasMember(eCaptureMemberRole::marker_image))
	{
		if (markerJpeg.empty())
			bOk= writer.addMemberFromFile(markerImageName, plan.marker->media, error);
		else
			bOk= writer.addMemberFromMemory(markerImageName, markerJpeg.data(), markerJpeg.size(), error);
	}
	if (bOk && manifest.hasMember(eCaptureMemberRole::marker_pose))
	{
		if (markerTrackJson.empty())
			bOk= writer.addMemberFromFile("marker.pose.json", plan.marker->poseTrack, error);
		else
			bOk= writer.addMemberFromMemory("marker.pose.json", markerTrackJson.data(), markerTrackJson.size(), error);
	}
	if (bOk)
		bOk= writer.finish(error);

	if (!bOk)
	{
		MIKAN_LOG_ERROR("LegacyTakeMigration") << "Failed to pack " << take.media.string() << ": " << error;
		return std::filesystem::path();
	}

	return capturePath;
}

// Repoints the file video sources in the project file. mediaPaths maps the
// stored path of a packed take or its marker take to the capture; trackPaths
// maps a moved refined track to its new stored path. Any other track under a
// repointed source is cleared, since the capture carries it now.
bool repointProjectFile(configuru::Config& node, const std::map<std::string, std::string>& mediaPaths,
						const std::map<std::string, std::string>& trackPaths)
{
	bool bChanged= false;

	if (node.is_object())
	{
		if (node.has_key("media_path") && node["media_path"].is_object() && node["media_path"].has_key("asset_path"))
		{
			const std::string mediaPath= node["media_path"].get_or<std::string>("asset_path", "");
			auto mediaIt= mediaPaths.find(mediaPath);
			if (mediaIt != mediaPaths.end())
			{
				node["media_path"]["asset_path"]= mediaIt->second;
				bChanged= true;

				if (node.has_key("pose_track_path"))
				{
					const std::string trackPath= node["pose_track_path"].is_object()
													 ? node["pose_track_path"].get_or<std::string>("asset_path", "")
													 : std::string();
					auto trackIt= trackPaths.find(trackPath);
					if (trackIt != trackPaths.end())
						node["pose_track_path"]["asset_path"]= trackIt->second;
					else
						node.erase("pose_track_path");
				}
				node.erase("marker_media_path");
				node.erase("marker_pose_track_path");
			}
		}

		for (auto& entry : node.as_object())
		{
			bChanged|= repointProjectFile(entry.value(), mediaPaths, trackPaths);
		}
	}
	else if (node.is_array())
	{
		for (configuru::Config& element : node.as_array())
		{
			bChanged|= repointProjectFile(element, mediaPaths, trackPaths);
		}
	}

	return bChanged;
}
} // namespace

eCapturePoseSpace poseSpaceForSessionId(const std::string& sessionId)
{
	const bool bRecorderId=
		sessionId.size() == 32
		&& std::all_of(sessionId.begin(), sessionId.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
	return bRecorderId ? eCapturePoseSpace::stage : eCapturePoseSpace::arkit_world;
}

bool projectHasLooseTakes(const std::filesystem::path& projectDir)
{
	for (const auto& entry : collectTakeFiles(projectDir))
	{
		if (entry.second.hasSidecar())
			return true;
	}
	return false;
}

bool migrateProject(const std::filesystem::path& projectDir, const std::filesystem::path& projectFilePath)
{
	if (!projectHasLooseTakes(projectDir))
		return false;

	const std::map<std::string, TakeFiles> takes= collectTakeFiles(projectDir);
	std::set<std::string> consumed;
	std::map<std::string, std::string> mediaPaths;
	std::map<std::string, std::string> trackPaths;
	int packedCount= 0;
	int orphanCount= 0;

	// A marker take is packed with its main take when that take has media and
	// accepts it, so the main takes go first and any marker take left over,
	// orphaned or refused, is packed as an ordinary take of its own
	std::vector<const TakeFiles*> orderedTakes;
	for (const auto& entry : takes)
	{
		if (!entry.second.media.empty() && !TakeNaming::isMarkerName(entry.second.stem))
			orderedTakes.push_back(&entry.second);
	}
	for (const auto& entry : takes)
	{
		if (!entry.second.media.empty() && TakeNaming::isMarkerName(entry.second.stem))
			orderedTakes.push_back(&entry.second);
	}

	for (const TakeFiles* takePtr : orderedTakes)
	{
		const TakeFiles& take= *takePtr;
		if (consumed.count(take.stem) != 0)
			continue;

		const TakeFiles* marker= nullptr;
		auto markerIt= takes.find(TakeNaming::makeMarkerName(take.stem));
		if (markerIt != takes.end() && !markerIt->second.media.empty())
			marker= &markerIt->second;

		PackPlan plan;
		if (!planTake(take, marker, plan))
			continue;

		const std::filesystem::path capturePath= packTake(projectDir, plan);
		if (capturePath.empty())
			continue;

		const std::string captureStored= makeStoredPath(projectDir, capturePath);
		mediaPaths[makeStoredPath(projectDir, take.media)]= captureStored;
		consumed.insert(take.stem);
		if (plan.marker != nullptr)
		{
			mediaPaths[makeStoredPath(projectDir, plan.marker->media)]= captureStored;
			consumed.insert(plan.marker->stem);
		}

		// The refined track moves beside the capture rather than into it, since
		// it is a regenerable output
		if (!take.refinedTrack.empty())
		{
			const std::filesystem::path refinedDestination= capturePath.parent_path() / take.refinedTrack.filename();
			std::error_code ec;
			std::filesystem::rename(take.refinedTrack, refinedDestination, ec);
			if (ec)
			{
				MIKAN_LOG_WARNING("LegacyTakeMigration") << "Could not move " << take.refinedTrack.string() << " to "
														 << refinedDestination.string() << ": " << ec.message();
			}
			else
			{
				trackPaths[makeStoredPath(projectDir, take.refinedTrack)]=
					makeStoredPath(projectDir, refinedDestination);
			}
		}

		// The originals move aside, including depth tracks the capture left out
		for (const std::filesystem::path& path : take.allFiles())
		{
			if (path != take.refinedTrack)
				moveToLegacy(projectDir, path);
		}
		if (plan.marker != nullptr)
		{
			for (const std::filesystem::path& path : plan.marker->allFiles())
				moveToLegacy(projectDir, path);
		}

		++packedCount;
		MIKAN_LOG_INFO("LegacyTakeMigration")
			<< "Packed " << makeStoredPath(projectDir, take.media)
			<< (plan.marker != nullptr ? " with " + makeStoredPath(projectDir, plan.marker->media) : std::string())
			<< " into " << captureStored;
	}

	// Sidecars with no take to join move aside too, so the pack never reruns
	for (const auto& entry : takes)
	{
		const TakeFiles& take= entry.second;
		if (consumed.count(take.stem) != 0 || !take.hasSidecar())
			continue;

		for (const std::filesystem::path& path : take.allFiles())
		{
			if (path != take.media && moveToLegacy(projectDir, path))
				++orphanCount;
		}
	}

	// The stored paths in the project file
	std::error_code ec;
	if (!mediaPaths.empty() && std::filesystem::is_regular_file(projectFilePath, ec))
	{
		try
		{
			configuru::Config projectJson= configuru::parse_file(projectFilePath.string(), configuru::JSON);
			if (repointProjectFile(projectJson, mediaPaths, trackPaths))
			{
				configuru::dump_file(projectFilePath.string(), projectJson, configuru::JSON);
				MIKAN_LOG_INFO("LegacyTakeMigration") << "Repointed file video sources in " << projectFilePath.string();
			}
		}
		catch (const std::exception& e)
		{
			MIKAN_LOG_ERROR("LegacyTakeMigration")
				<< "Failed to repoint file video sources in " << projectFilePath.string() << ": " << e.what();
		}
	}

	MIKAN_LOG_INFO("LegacyTakeMigration")
		<< "Packed " << packedCount << " take(s) into " << k_capturesFolder << "/ and moved " << orphanCount
		<< " orphan sidecar(s); originals are in " << (projectDir / k_legacyFolder).string();
	return packedCount > 0 || orphanCount > 0;
}
} // namespace LegacyTakeMigration
