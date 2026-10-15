#include "LegacyTakeMigrationTests.h"
#include "unit_test.h"

#include "Capture.h"
#include "LegacyTakeMigration.h"
#include "PathUtils.h"
#include "PoseTrack.h"

#include "glm/ext/matrix_float4x4.hpp"

#include "opencv2/opencv.hpp"

#include <configuru.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace LegacyTakeMigrationTestsInternal
{
const std::string k_phoneSession= "7c1d1a2e-1111-4a4a-9b9b-0123456789ab";
const std::string k_otherPhoneSession= "7c1d1a2e-2222-4a4a-9b9b-0123456789ab";
const std::string k_recorderSession= "0123456789abcdef0123456789abcdef";

void writeFile(const std::filesystem::path& path, const std::string& content)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream file(path, std::ios::binary);
	file << content;
}

void writePoseTrack(const std::filesystem::path& path, const std::string& sessionId, int frameCount, bool bWithDepth)
{
	PoseTrackWriter writer(sessionId, 64, 48);
	if (bWithDepth)
		writer.setDepthTrack(4, 3, 1.0);
	for (int frame= 0; frame < frameCount; ++frame)
	{
		glm::mat4 cameraToWorld(1.f);
		cameraToWorld[3]= glm::vec4((float)frame, 2.f, 3.f, 1.f);
		writer.append((int64_t)frame * 41667, 1000 + (int64_t)frame * 41667, cameraToWorld, 50.0, 50.0, 32.0, 24.0);
	}
	writer.writeToFile(path);
}

bool writeMovie(const std::filesystem::path& path, int frameCount)
{
	cv::VideoWriter writer(path.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 24.0, cv::Size(64, 48), true);
	if (!writer.isOpened())
		return false;
	for (int frame= 0; frame < frameCount; ++frame)
		writer.write(cv::Mat(48, 64, CV_8UC3, cv::Scalar(10 * frame, 200, 30)));
	writer.release();
	return true;
}

// A project with every grouping case the packer handles
struct TakeMigrationProject
{
	std::filesystem::path root;
	std::filesystem::path projectDir;
	std::filesystem::path projectFilePath;
	std::filesystem::path savedProjectDir;
	bool bFixturesWritten= true;

	TakeMigrationProject()
	{
		savedProjectDir= PathUtils::getProjectDirectory();

		root= std::filesystem::temp_directory_path() / "mikan_legacy_take_migration_tests";
		std::filesystem::remove_all(root);
		projectDir= root / "project";
		projectFilePath= projectDir / "project.mikanproj";
		std::filesystem::create_directories(projectDir / "movies");
		std::filesystem::create_directories(projectDir / "textures");
		std::filesystem::create_directories(projectDir / "captures");

		// A LiDAR phone take with its marker photo and a refined track from a build
		writeFile(projectDir / "movies" / "trk.mp4", "movie bytes");
		writePoseTrack(projectDir / "movies" / "trk.pose.json", k_phoneSession, 3, true);
		writeFile(projectDir / "movies" / "trk.depth.bin", std::string(3 * 4 * 3 * 2, 'd'));
		writeFile(projectDir / "movies" / "trk.conf.bin", std::string(3 * 4 * 3, 'c'));
		writePoseTrack(projectDir / "movies" / "trk.refined.pose.json", k_phoneSession, 3, true);
		writeFile(projectDir / "textures" / "trk_marker.jpg", "marker jpeg bytes");
		writePoseTrack(projectDir / "textures" / "trk_marker.pose.json", k_phoneSession, 1, false);

		// A tripod movie with no track whose marker take is a movie
		bFixturesWritten&= writeMovie(projectDir / "movies" / "still.avi", 4);
		bFixturesWritten&= writeMovie(projectDir / "movies" / "still_marker.avi", 4);
		writePoseTrack(projectDir / "movies" / "still_marker.pose.json", k_phoneSession, 4, false);

		// An editor photo with a one-frame stage-space track
		writeFile(projectDir / "textures" / "photo.jpg", "photo bytes");
		writePoseTrack(projectDir / "textures" / "photo.pose.json", k_recorderSession, 1, false);

		// A phone movie whose marker take came from another session
		writeFile(projectDir / "movies" / "mixed.mp4", "movie bytes");
		writePoseTrack(projectDir / "movies" / "mixed.pose.json", k_phoneSession, 2, false);
		writeFile(projectDir / "textures" / "mixed_marker.jpg", "marker jpeg bytes");
		writePoseTrack(projectDir / "textures" / "mixed_marker.pose.json", k_otherPhoneSession, 1, false);

		// Raw media and an orphan sidecar
		writeFile(projectDir / "movies" / "raw.mp4", "raw movie");
		writeFile(projectDir / "textures" / "plain.png", "texture");
		writePoseTrack(projectDir / "movies" / "orphan.pose.json", k_phoneSession, 1, false);

		writeFile(projectFilePath, R"({
	"FileVideoSourceSystem": {
		"components": [
			{ "component_id": 1,
			  "media_path": { "class_name": "MovieAssetReference", "asset_path": "movies/trk.mp4" },
			  "marker_media_path": { "class_name": "MovieAssetReference", "asset_path": "textures/trk_marker.jpg" },
			  "pose_track_path": { "class_name": "PoseTrackAssetReference", "asset_path": "movies/trk.refined.pose.json" },
			  "marker_pose_track_path": { "class_name": "PoseTrackAssetReference", "asset_path": "textures/trk_marker.pose.json" },
			  "pose_offset": [1,0,0,0, 0,1,0,0, 0,0,1,0, 4,5,6,1] },
			{ "component_id": 2,
			  "media_path": { "class_name": "MovieAssetReference", "asset_path": "movies/still.avi" },
			  "marker_media_path": { "class_name": "MovieAssetReference", "asset_path": "movies/still_marker.avi" },
			  "pose_track_path": { "class_name": "PoseTrackAssetReference", "asset_path": "" } },
			{ "component_id": 3,
			  "media_path": { "class_name": "MovieAssetReference", "asset_path": "movies/raw.mp4" } },
			{ "component_id": 4,
			  "media_path": { "class_name": "MovieAssetReference", "asset_path": "textures/trk_marker.jpg" },
			  "pose_track_path": { "class_name": "PoseTrackAssetReference", "asset_path": "textures/trk_marker.pose.json" } }
		]
	}
})");

		PathUtils::setProjectDirectory(projectDir);
	}

	~TakeMigrationProject()
	{
		PathUtils::setProjectDirectory(savedProjectDir);
		std::error_code ec;
		std::filesystem::remove_all(root, ec);
	}

	configuru::Config component(int index) const
	{
		const configuru::Config projectJson= configuru::parse_file(projectFilePath.string(), configuru::JSON);
		return projectJson["FileVideoSourceSystem"]["components"][index];
	}
};

std::string assetPath(const configuru::Config& component, const char* key)
{
	if (!component.has_key(key) || !component[key].is_object())
		return std::string();
	return component[key].get_or<std::string>("asset_path", "");
}
} // namespace LegacyTakeMigrationTestsInternal

bool run_legacy_take_migration_tests()
{
	UNIT_TEST_MODULE_BEGIN("legacy_take_migration")
	UNIT_TEST_MODULE_CALL_TEST(legacy_take_migration_test_pose_space_rule);
	UNIT_TEST_MODULE_CALL_TEST(legacy_take_migration_test_packs_and_repoints);
	UNIT_TEST_MODULE_END()
}

bool legacy_take_migration_test_pose_space_rule()
{
	UNIT_TEST_BEGIN("a recorder session id means stage space, a phone UUID means ARKit world")
	using namespace LegacyTakeMigrationTestsInternal;

	success&= LegacyTakeMigration::poseSpaceForSessionId(k_recorderSession) == eCapturePoseSpace::stage;
	success&= LegacyTakeMigration::poseSpaceForSessionId(k_phoneSession) == eCapturePoseSpace::arkit_world;
	success&= LegacyTakeMigration::poseSpaceForSessionId("take_20260915") == eCapturePoseSpace::arkit_world;
	success&= LegacyTakeMigration::poseSpaceForSessionId(std::string(32, 'g')) == eCapturePoseSpace::arkit_world;

	UNIT_TEST_COMPLETE()
}

bool legacy_take_migration_test_packs_and_repoints()
{
	UNIT_TEST_BEGIN("loose takes pack to their kinds, sources are repointed, and a second load does nothing")
	using namespace LegacyTakeMigrationTestsInternal;

	TakeMigrationProject project;
	success&= project.bFixturesWritten;
	if (!success)
		fprintf(stdout, "    could not write the movie fixtures\n");

	success&= LegacyTakeMigration::projectHasLooseTakes(project.projectDir);
	success&= LegacyTakeMigration::migrateProject(project.projectDir, project.projectFilePath);

	const std::filesystem::path captures= project.projectDir / "captures";
	const std::filesystem::path legacy= project.projectDir / "legacy_takes";
	std::string error;

	// The LiDAR take: trackcamera with depth, the marker photo and its track,
	// the refined track moved beside it
	{
		Capture capture;
		success&= capture.open(captures / "trk.trackcamera", error);
		if (!success)
			fprintf(stdout, "    trk.trackcamera: %s\n", error.c_str());
		success&= capture.getKind() == eCaptureKind::trackcamera && !capture.isStageSpace();
		success&= capture.getManifest().sessionId == k_phoneSession;
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::movie) == "movie.mp4";
		success&= capture.hasMember(eCaptureMemberRole::depth) && capture.hasMember(eCaptureMemberRole::confidence);
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::marker_image) == "marker.jpg";
		success&= capture.getPoseTrack(eCaptureMemberRole::movie_pose).getFrameCount() == 3;
		success&= capture.getPoseTrack(eCaptureMemberRole::marker_pose).getFrameCount() == 1;
		const CaptureArchiveMember* depth= capture.findMember(eCaptureMemberRole::depth);
		success&= depth != nullptr && depth->size == 3 * 4 * 3 * 2;
		std::vector<uint8_t> marker;
		success&= capture.readMemberBytes(eCaptureMemberRole::marker_image, marker, error)
				  && std::string(marker.begin(), marker.end()) == "marker jpeg bytes";

		success&= std::filesystem::exists(captures / "trk.refined.pose.json");
		for (const char* moved : {"movies/trk.mp4", "movies/trk.pose.json", "movies/trk.depth.bin",
								  "movies/trk.conf.bin", "textures/trk_marker.jpg", "textures/trk_marker.pose.json"})
		{
			success&= std::filesystem::exists(legacy / moved) && !std::filesystem::exists(project.projectDir / moved);
		}
	}

	// The tripod movie: stillcamera whose marker is the first frame of the
	// marker movie with that frame's pose
	{
		Capture capture;
		success&= capture.open(captures / "still.stillcamera", error);
		if (!success)
			fprintf(stdout, "    still.stillcamera: %s\n", error.c_str());
		success&= capture.getKind() == eCaptureKind::stillcamera;
		success&= capture.getManifest().sessionId == k_phoneSession;
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::movie) == "movie.avi";
		success&= !capture.hasMember(eCaptureMemberRole::movie_pose);
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::marker_image) == "marker.jpg";
		const PoseTrack& markerTrack= capture.getPoseTrack(eCaptureMemberRole::marker_pose);
		success&= markerTrack.isLoaded() && markerTrack.getFrameCount() == 1 && markerTrack.getFrame(0).timeUs == 0;
		cv::Mat markerImage;
		success&= capture.decodeImage(eCaptureMemberRole::marker_image, markerImage, error) && markerImage.cols == 64
				  && markerImage.rows == 48;
		success&= std::filesystem::exists(legacy / "movies" / "still_marker.avi");
	}

	// The editor photo: stillphoto in stage space
	{
		Capture capture;
		success&= capture.open(captures / "photo.stillphoto", error);
		if (!success)
			fprintf(stdout, "    photo.stillphoto: %s\n", error.c_str());
		success&= capture.getKind() == eCaptureKind::stillphoto && capture.isStageSpace();
		success&= capture.getManifest().getMemberName(eCaptureMemberRole::plate_image) == "plate.jpg";
		success&= !capture.hasMember(eCaptureMemberRole::marker_image);
	}

	// The mismatched marker is left out of its take and packed on its own
	{
		Capture capture;
		success&= capture.open(captures / "mixed.trackcamera", error);
		success&= !capture.hasMember(eCaptureMemberRole::marker_image);
		Capture markerCapture;
		success&= markerCapture.open(captures / "mixed_marker.stillphoto", error);
		success&= markerCapture.getManifest().sessionId == k_otherPhoneSession;
	}

	// Raw media stays, the orphan sidecar moves aside, and nothing else is left
	success&= std::filesystem::exists(project.projectDir / "movies" / "raw.mp4");
	success&= std::filesystem::exists(project.projectDir / "textures" / "plain.png");
	success&= std::filesystem::exists(legacy / "movies" / "orphan.pose.json");
	success&= !std::filesystem::exists(project.projectDir / "movies" / "orphan.pose.json");
	success&= !LegacyTakeMigration::projectHasLooseTakes(project.projectDir);

	// The sources: repointed to the captures, the refined override moved, the
	// loose track and the marker keys gone, the offset kept, the raw source untouched
	{
		const configuru::Config trk= project.component(0);
		success&= assetPath(trk, "media_path") == "captures/trk.trackcamera";
		success&= assetPath(trk, "pose_track_path") == "captures/trk.refined.pose.json";
		success&= !trk.has_key("marker_media_path") && !trk.has_key("marker_pose_track_path");
		success&= trk.has_key("pose_offset") && trk["pose_offset"][12].as_float() == 4.f;

		const configuru::Config still= project.component(1);
		success&= assetPath(still, "media_path") == "captures/still.stillcamera";
		success&= !still.has_key("pose_track_path") && !still.has_key("marker_media_path");

		const configuru::Config raw= project.component(2);
		success&= assetPath(raw, "media_path") == "movies/raw.mp4";

		// A source that played the marker take itself follows it into the capture
		const configuru::Config markerSource= project.component(3);
		success&= assetPath(markerSource, "media_path") == "captures/trk.trackcamera";
		success&= !markerSource.has_key("pose_track_path");
	}

	// A second load finds nothing to do
	success&= !LegacyTakeMigration::migrateProject(project.projectDir, project.projectFilePath);
	success&= assetPath(project.component(0), "media_path") == "captures/trk.trackcamera";

	UNIT_TEST_COMPLETE()
}
