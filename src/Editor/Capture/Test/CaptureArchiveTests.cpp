#include "CaptureArchiveTests.h"
#include "unit_test.h"

#include "Capture.h"
#include "CaptureArchiveReader.h"
#include "CaptureArchiveWriter.h"
#include "CaptureManifest.h"
#include "CaptureUstarHeader.h"
#include "MovieDecoder.h"
#include "PoseTrack.h"

#include "glm/ext/matrix_float4x4.hpp"

#include "opencv2/opencv.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace CaptureArchiveTestsInternal
{
// A throwaway folder for the archive fixtures, removed on destruction
struct FixtureDir
{
	std::filesystem::path root;

	FixtureDir()
	{
		root= std::filesystem::temp_directory_path() / "mikan_capture_archive_tests";
		std::filesystem::remove_all(root);
		std::filesystem::create_directories(root);
	}

	~FixtureDir()
	{
		std::error_code ignored;
		std::filesystem::remove_all(root, ignored);
	}
};

const std::string k_sessionA= "7c1d1a2e-1111-4a4a-9b9b-0123456789ab";
const std::string k_sessionB= "7c1d1a2e-2222-4a4a-9b9b-0123456789ab";

const int k_movieWidth= 64;
const int k_movieHeight= 48;
const int k_movieFrameCount= 12;
const double k_movieFps= 24.0;

cv::Scalar movieFrameColor(int frameIndex) { return cv::Scalar(20 * frameIndex, 100, 255 - 20 * frameIndex); }

bool writeMovieFixture(const std::filesystem::path& path)
{
	cv::VideoWriter writer(path.string(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), k_movieFps,
						   cv::Size(k_movieWidth, k_movieHeight), true);
	if (!writer.isOpened())
		return false;

	for (int frameIndex= 0; frameIndex < k_movieFrameCount; ++frameIndex)
	{
		const cv::Mat frame(k_movieHeight, k_movieWidth, CV_8UC3, movieFrameColor(frameIndex));
		writer.write(frame);
	}

	writer.release();
	return true;
}

// MJPG is lossy, so the centre pixel is compared against the written colour
// with a tolerance
bool centrePixelMatches(const cv::Mat& frame, const cv::Scalar& expected, int tolerance)
{
	if (frame.empty() || frame.type() != CV_8UC3)
		return false;

	const cv::Vec3b pixel= frame.at<cv::Vec3b>(frame.rows / 2, frame.cols / 2);
	for (int channel= 0; channel < 3; ++channel)
	{
		if (std::abs((int)pixel[channel] - (int)expected[channel]) > tolerance)
			return false;
	}

	return true;
}

std::string makePoseTrackJson(const std::string& sessionId, int frameCount)
{
	PoseTrackWriter writer(sessionId, 1920, 1440);
	for (int frame= 0; frame < frameCount; ++frame)
	{
		glm::mat4 cameraToWorld(1.f);
		cameraToWorld[3]= glm::vec4(1.f, 2.f, 3.f + (float)frame, 1.f);
		writer.append((int64_t)frame * 33333, 123456789 + (int64_t)frame * 33333, cameraToWorld, 1450.0, 1450.0, 960.0,
					  720.0);
	}
	return configuru::dump_string(writer.toConfig(), configuru::JSON);
}

std::vector<uint8_t> encodePng(const cv::Scalar& color)
{
	std::vector<uint8_t> bytes;
	cv::imencode(".png", cv::Mat(16, 32, CV_8UC3, color), bytes);
	return bytes;
}

CaptureManifest makeManifest(eCaptureKind kind, const std::string& sessionId= k_sessionA)
{
	CaptureManifest manifest;
	manifest.kind= kind;
	manifest.sessionId= sessionId;
	manifest.poseSpace= eCapturePoseSpace::arkit_world;
	manifest.createdUtc= CaptureManifest::makeUtcTimestampNow();
	manifest.producer= CaptureManifest::makeEditorProducer();
	return manifest;
}

// A raw member for the hand-built archives the rejection tests use
struct RawMember
{
	std::string name;
	std::string data;
	// Applied to the header block after it is written, for corrupting one field
	std::function<void(char*)> mutateHeader;
};

// Writes a tar from raw members, with the end-of-archive blocks unless told
// not to, so each rejection test can produce one shape of bad archive
bool writeRawArchive(const std::filesystem::path& path, const std::vector<RawMember>& members,
					 bool bWriteEndBlocks= true)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
		return false;

	for (const RawMember& member : members)
	{
		char header[CaptureUstarHeader::k_blockSize];
		CaptureUstarHeader::write(header, member.name, member.data.size(), 0);
		if (member.mutateHeader)
			member.mutateHeader(header);
		file.write(header, sizeof(header));
		file.write(member.data.data(), (std::streamsize)member.data.size());

		const size_t padding= (CaptureUstarHeader::k_blockSize - member.data.size() % CaptureUstarHeader::k_blockSize)
							  % CaptureUstarHeader::k_blockSize;
		const std::string zeros(padding, '\0');
		file.write(zeros.data(), (std::streamsize)zeros.size());
	}

	if (bWriteEndBlocks)
	{
		const std::string zeros(2 * CaptureUstarHeader::k_blockSize, '\0');
		file.write(zeros.data(), (std::streamsize)zeros.size());
	}

	file.close();
	return (bool)file;
}

// Recomputes the checksum after a mutation so only the mutated field is wrong
void recomputeChecksum(char* header)
{
	uint32_t sum= 0;
	for (size_t index= 0; index < CaptureUstarHeader::k_blockSize; ++index)
	{
		if (index >= 148 && index < 156)
			sum+= (uint32_t)' ';
		else
			sum+= (uint32_t)(unsigned char)header[index];
	}
	char field[8];
	std::snprintf(field, sizeof(field), "%06o", sum);
	field[6]= '\0';
	field[7]= ' ';
	std::memcpy(header + 148, field, 8);
}

bool expectRejected(const std::filesystem::path& path, const char* what)
{
	CaptureArchiveReader reader;
	std::string error;
	if (reader.open(path, error))
	{
		fprintf(stdout, "    expected rejection of %s, but the archive opened\n", what);
		return false;
	}
	return true;
}
} // namespace CaptureArchiveTestsInternal

bool run_capture_archive_tests()
{
	UNIT_TEST_MODULE_BEGIN("capture_archive")
	UNIT_TEST_MODULE_CALL_TEST(capture_archive_test_manifest);
	UNIT_TEST_MODULE_CALL_TEST(capture_archive_test_writer_reader_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(capture_archive_test_reader_rejections);
	UNIT_TEST_MODULE_CALL_TEST(capture_archive_test_capture_open);
	UNIT_TEST_MODULE_CALL_TEST(capture_archive_test_movie_member_plays_in_place);
	UNIT_TEST_MODULE_END()
}

bool capture_archive_test_manifest()
{
	UNIT_TEST_BEGIN("the manifest round-trips through JSON and validates kinds and members")
	using namespace CaptureArchiveTestsInternal;

	// Kind, extension, and role names map both ways
	for (int index= 0; index < (int)eCaptureKind::COUNT; ++index)
	{
		const eCaptureKind kind= (eCaptureKind)index;
		success&= CaptureKindUtils::kindFromString(CaptureKindUtils::kindToString(kind)) == kind;
		success&=
			CaptureKindUtils::kindFromExtension(std::string("take") + CaptureKindUtils::kindToExtension(kind)) == kind;
	}
	success&= CaptureKindUtils::kindFromExtension("take.TRACKCAMERA") == eCaptureKind::trackcamera;
	success&= CaptureKindUtils::kindFromExtension("take.mp4") == eCaptureKind::INVALID;
	success&= !CaptureKindUtils::isCaptureExtension("take.pose.json");
	for (int index= 0; index < (int)eCaptureMemberRole::COUNT; ++index)
	{
		const eCaptureMemberRole role= (eCaptureMemberRole)index;
		success&= CaptureKindUtils::roleFromString(CaptureKindUtils::roleToString(role)) == role;
	}

	// A full trackcamera manifest survives the JSON round trip
	CaptureManifest manifest= makeManifest(eCaptureKind::trackcamera);
	manifest.bInterrupted= true;
	manifest.members[eCaptureMemberRole::marker_image]= "marker.jpg";
	manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
	manifest.members[eCaptureMemberRole::movie]= "movie.mp4";
	manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
	manifest.members[eCaptureMemberRole::depth]= "movie.depth.bin";
	manifest.members[eCaptureMemberRole::confidence]= "movie.conf.bin";
	std::string error;
	success&= manifest.validate(error);

	CaptureManifest loaded;
	success&= loaded.loadFromJsonString(manifest.toJsonString(), error);
	if (!success)
		fprintf(stdout, "    round trip failed: %s\n", error.c_str());
	success&= loaded.kind == eCaptureKind::trackcamera;
	success&= loaded.sessionId == k_sessionA;
	success&= loaded.poseSpace == eCapturePoseSpace::arkit_world;
	success&= loaded.createdUtc == manifest.createdUtc && !loaded.createdUtc.empty();
	success&= loaded.producer == manifest.producer;
	success&= loaded.bInterrupted;
	success&= loaded.members == manifest.members;
	success&= loaded.getMemberName(eCaptureMemberRole::depth) == "movie.depth.bin";
	success&= loaded.getMemberName(eCaptureMemberRole::plate_image).empty();

	// Each kind rejects a manifest missing one of its required members
	{
		CaptureManifest stillPhoto= makeManifest(eCaptureKind::stillphoto);
		stillPhoto.members[eCaptureMemberRole::marker_image]= "marker.jpg";
		success&= !stillPhoto.validate(error);
		stillPhoto.members[eCaptureMemberRole::plate_image]= "plate.jpg";
		success&= stillPhoto.validate(error);

		CaptureManifest stillCamera= makeManifest(eCaptureKind::stillcamera);
		stillCamera.members[eCaptureMemberRole::plate_image]= "plate.jpg";
		success&= !stillCamera.validate(error);
		stillCamera.members[eCaptureMemberRole::movie]= "movie.mp4";
		success&= stillCamera.validate(error);

		CaptureManifest trackCamera= makeManifest(eCaptureKind::trackcamera);
		trackCamera.members[eCaptureMemberRole::movie]= "movie.mp4";
		success&= !trackCamera.validate(error);
		trackCamera.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
		success&= trackCamera.validate(error);

		// Depth without confidence, two roles on one name, and the reserved
		// manifest name are all rejected
		trackCamera.members[eCaptureMemberRole::depth]= "movie.depth.bin";
		success&= !trackCamera.validate(error);
		trackCamera.members[eCaptureMemberRole::confidence]= "movie.depth.bin";
		success&= !trackCamera.validate(error);
		trackCamera.members[eCaptureMemberRole::confidence]= "movie.conf.bin";
		success&= trackCamera.validate(error);
		trackCamera.members[eCaptureMemberRole::plate_image]= CaptureManifest::k_manifestMemberName;
		success&= !trackCamera.validate(error);
	}

	// The parser rejects a wrong format, version, kind, pose space, or role
	{
		CaptureManifest parsed;
		std::string json= manifest.toJsonString();
		success&= !parsed.loadFromJsonString("not json", error);
		success&= !parsed.loadFromJsonString("{\"format\": \"other\"}", error);

		configuru::Config badVersion= manifest.toConfig();
		badVersion["version"]= static_cast<int64_t>(99);
		success&= !parsed.loadFromConfig(badVersion, error);

		configuru::Config badKind= manifest.toConfig();
		badKind["kind"]= "handheld";
		success&= !parsed.loadFromConfig(badKind, error);

		configuru::Config badSpace= manifest.toConfig();
		badSpace["pose_space"]= "world";
		success&= !parsed.loadFromConfig(badSpace, error);

		configuru::Config badRole= manifest.toConfig();
		badRole["members"]["thumbnail"]= "thumb.jpg";
		success&= !parsed.loadFromConfig(badRole, error);

		// A parse failure leaves the manifest cleared
		success&= parsed.kind == eCaptureKind::INVALID && parsed.members.empty();
	}

	UNIT_TEST_COMPLETE()
}

bool capture_archive_test_writer_reader_round_trip()
{
	UNIT_TEST_BEGIN("the writer's archive reads back with aligned offsets and intact members")
	using namespace CaptureArchiveTestsInternal;
	FixtureDir fixture;

	// A file member larger than the writer's copy chunk, with an odd size so
	// the padding is exercised, plus two members from memory
	const std::filesystem::path largePath= fixture.root / "large.bin";
	const size_t largeSize= (1 << 20) + 777;
	std::string largeData(largeSize, '\0');
	for (size_t index= 0; index < largeSize; ++index)
		largeData[index]= (char)(index * 31 + 7);
	{
		std::ofstream file(largePath, std::ios::binary);
		file.write(largeData.data(), (std::streamsize)largeData.size());
	}

	const std::string trackJson= makePoseTrackJson(k_sessionA, 3);
	const std::vector<uint8_t> plate= encodePng(cv::Scalar(10, 20, 30));

	CaptureManifest manifest= makeManifest(eCaptureKind::stillcamera);
	manifest.members[eCaptureMemberRole::plate_image]= "plate.png";
	manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
	manifest.members[eCaptureMemberRole::movie]= "movie.bin";

	const std::filesystem::path archivePath= fixture.root / "take.stillcamera";
	std::string error;
	{
		CaptureArchiveWriter writer;
		success&= writer.open(archivePath, manifest, error);
		if (!success)
			fprintf(stdout, "    open failed: %s\n", error.c_str());

		// A name the manifest does not declare is refused
		success&= !writer.addMemberFromMemory("extra.bin", "x", 1, error);

		success&= writer.addMemberFromMemory("plate.png", plate.data(), plate.size(), error);
		success&= writer.addMemberFromMemory("marker.pose.json", trackJson.data(), trackJson.size(), error);
		// The same name twice is refused
		success&= !writer.addMemberFromMemory("plate.png", plate.data(), plate.size(), error);
		success&= writer.addMemberFromFile("movie.bin", largePath, error);
		success&= writer.finish(error);
		if (!success)
			fprintf(stdout, "    write failed: %s\n", error.c_str());

		// The temporary file is gone and the archive is in place
		success&= !std::filesystem::exists(archivePath.string() + ".tmp");
		success&= std::filesystem::exists(archivePath);
	}

	{
		CaptureArchiveReader reader;
		success&= reader.open(archivePath, error);
		if (!success)
			fprintf(stdout, "    read failed: %s\n", error.c_str());

		const std::vector<CaptureArchiveMember>& members= reader.getMembers();
		success&= members.size() == 4;
		if (members.size() == 4)
		{
			// The manifest is first, at offset 512, and every member's data
			// starts on a block boundary
			success&= members[0].name == CaptureManifest::k_manifestMemberName && members[0].dataOffset == 512;
			success&= members[1].name == "plate.png" && members[2].name == "marker.pose.json"
					  && members[3].name == "movie.bin";
			for (const CaptureArchiveMember& member : members)
				success&= member.dataOffset % 512 == 0;
			success&= members[3].size == largeSize;
		}

		std::string manifestJson;
		success&= reader.readMemberString(CaptureManifest::k_manifestMemberName, manifestJson, error);
		CaptureManifest readManifest;
		success&= readManifest.loadFromJsonString(manifestJson, error);
		success&= readManifest.kind == eCaptureKind::stillcamera && readManifest.members == manifest.members;

		std::vector<uint8_t> bytes;
		const CaptureArchiveMember* plateMember= reader.findMember("plate.png");
		success&= plateMember != nullptr && reader.readMember(*plateMember, bytes, error) && bytes == plate;

		const CaptureArchiveMember* movieMember= reader.findMember("movie.bin");
		success&= movieMember != nullptr && reader.readMember(*movieMember, bytes, error);
		success&= bytes.size() == largeSize && std::memcmp(bytes.data(), largeData.data(), largeSize) == 0;

		// The data really sits at the reported offset in the file
		if (movieMember != nullptr)
		{
			std::ifstream file(archivePath, std::ios::binary);
			file.seekg((std::streamoff)movieMember->dataOffset);
			char probe[16];
			file.read(probe, sizeof(probe));
			success&= std::memcmp(probe, largeData.data(), sizeof(probe)) == 0;
		}

		success&= reader.findMember("missing.bin") == nullptr;
	}

	// A finish with a declared member never added fails and leaves no file
	{
		const std::filesystem::path incompletePath= fixture.root / "incomplete.stillcamera";
		CaptureArchiveWriter writer;
		success&= writer.open(incompletePath, manifest, error);
		success&= writer.addMemberFromMemory("plate.png", plate.data(), plate.size(), error);
		success&= !writer.finish(error);
		success&=
			!std::filesystem::exists(incompletePath) && !std::filesystem::exists(incompletePath.string() + ".tmp");
	}

	// The writer refuses an extension that disagrees with the kind, an invalid
	// manifest, and a member name outside the accepted set
	{
		CaptureArchiveWriter writer;
		success&= !writer.open(fixture.root / "wrong.trackcamera", manifest, error);
		CaptureManifest invalid= makeManifest(eCaptureKind::stillcamera);
		success&= !writer.open(fixture.root / "invalid.stillcamera", invalid, error);
		CaptureManifest badName= manifest;
		badName.members[eCaptureMemberRole::movie]= "sub/movie.bin";
		success&= !writer.open(fixture.root / "badname.stillcamera", badName, error);
		success&= !writer.isOpen();
	}

	// Member name rules
	success&= CaptureArchiveReader::isValidMemberName("movie.mp4");
	success&= CaptureArchiveReader::isValidMemberName(std::string(100, 'a'));
	success&= !CaptureArchiveReader::isValidMemberName(std::string(101, 'a'));
	success&= !CaptureArchiveReader::isValidMemberName("");
	success&= !CaptureArchiveReader::isValidMemberName(".");
	success&= !CaptureArchiveReader::isValidMemberName("..");
	success&= !CaptureArchiveReader::isValidMemberName("a/b");
	success&= !CaptureArchiveReader::isValidMemberName("a\\b");
	success&= !CaptureArchiveReader::isValidMemberName("caf\xc3\xa9.jpg");
	success&= !CaptureArchiveReader::isValidMemberName("tab\tname");

	UNIT_TEST_COMPLETE()
}

bool capture_archive_test_reader_rejections()
{
	UNIT_TEST_BEGIN("the reader rejects every header outside the accepted ustar subset")
	using namespace CaptureArchiveTestsInternal;
	FixtureDir fixture;

	const std::string manifestJson= makeManifest(eCaptureKind::stillphoto).toJsonString();
	const RawMember manifestMember{CaptureManifest::k_manifestMemberName, manifestJson, nullptr};
	const RawMember plateMember{"plate.png", std::string(700, 'p'), nullptr};
	const std::filesystem::path path= fixture.root / "raw.tar";

	// The control: a well-formed raw archive opens
	{
		success&= writeRawArchive(path, {manifestMember, plateMember});
		CaptureArchiveReader reader;
		std::string error;
		success&= reader.open(path, error) && reader.getMembers().size() == 2;
		if (!success)
			fprintf(stdout, "    control archive failed: %s\n", error.c_str());
	}

	// A mutator that sets one field and fixes the checksum so only that field
	// is at fault
	auto setTypeFlag= [](char flag)
	{
		return [flag](char* header)
		{
			header[156]= flag;
			recomputeChecksum(header);
		};
	};

	success&= writeRawArchive(path, {manifestMember, {"dir", "", setTypeFlag('5')}});
	success&= expectRejected(path, "a directory member");

	success&= writeRawArchive(path, {manifestMember, {"link", "", setTypeFlag('1')}});
	success&= expectRejected(path, "a hard link member");

	success&= writeRawArchive(path, {manifestMember, {"link", "", setTypeFlag('2')}});
	success&= expectRejected(path, "a symbolic link member");

	success&= writeRawArchive(path, {manifestMember, {"PaxHeader", "path=x", setTypeFlag('x')}});
	success&= expectRejected(path, "a pax extended header");

	success&= writeRawArchive(path, {manifestMember, {"PaxHeader", "path=x", setTypeFlag('g')}});
	success&= expectRejected(path, "a pax global header");

	success&= writeRawArchive(path, {manifestMember, {"././@LongLink", "longname", setTypeFlag('L')}});
	success&= expectRejected(path, "a GNU long name header");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  // GNU magic "ustar  \0"
										  std::memcpy(header + 257, "ustar  ", 8);
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "the GNU magic");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  std::memcpy(header + 345, "subdir", 6);
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "a path prefix");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  std::memcpy(header + 157, "target", 6);
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "a link name");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  // Base-256 size encoding
										  header[124]= (char)0x80;
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "a base-256 size");

	success&= writeRawArchive(path, {manifestMember, {"plate.png", "p", [](char* header) { header[148]= '7'; }}});
	success&= expectRejected(path, "a wrong checksum");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  std::memcpy(header, "sub/plate.png", 13);
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "a member name with a path separator");

	success&= writeRawArchive(path, {plateMember, manifestMember});
	success&= expectRejected(path, "a manifest that is not the first member");

	success&= writeRawArchive(path, {manifestMember, plateMember, plateMember});
	success&= expectRejected(path, "a duplicate member name");

	success&= writeRawArchive(path, {manifestMember, plateMember}, false);
	success&= expectRejected(path, "a missing end-of-archive block");

	success&= writeRawArchive(path, {manifestMember,
									 {"plate.png", "p", [](char* header)
									  {
										  // Claims more data than the file holds
										  std::memcpy(header + 124, "00000010000", 11);
										  recomputeChecksum(header);
									  }}});
	success&= expectRejected(path, "a member running past the end of the file");

	success&= writeRawArchive(path, {});
	success&= expectRejected(path, "an empty archive");

	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file << "not a tar at all";
	}
	success&= expectRejected(path, "a file that is not a tar");

	success&= expectRejected(fixture.root / "missing.tar", "a file that does not exist");

	UNIT_TEST_COMPLETE()
}

bool capture_archive_test_capture_open()
{
	UNIT_TEST_BEGIN("a capture opens with its tracks and rejects a kind or session mismatch")
	using namespace CaptureArchiveTestsInternal;
	FixtureDir fixture;
	std::string error;

	const std::string movieTrack= makePoseTrackJson(k_sessionA, 5);
	const std::string markerTrackSameSession= makePoseTrackJson(k_sessionA, 1);
	const std::string markerTrackOtherSession= makePoseTrackJson(k_sessionB, 1);
	const std::vector<uint8_t> markerPng= encodePng(cv::Scalar(200, 100, 50));
	const std::string movieBytes(1000, 'm');

	auto writeTrackCamera= [&](const std::filesystem::path& path, const std::string& markerTrack)
	{
		CaptureManifest manifest= makeManifest(eCaptureKind::trackcamera);
		manifest.members[eCaptureMemberRole::marker_image]= "marker.png";
		manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
		manifest.members[eCaptureMemberRole::movie]= "movie.mp4";
		manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";

		CaptureArchiveWriter writer;
		bool bOk= writer.open(path, manifest, error);
		bOk&= writer.addMemberFromMemory("marker.png", markerPng.data(), markerPng.size(), error);
		bOk&= writer.addMemberFromMemory("marker.pose.json", markerTrack.data(), markerTrack.size(), error);
		bOk&= writer.addMemberFromMemory("movie.mp4", movieBytes.data(), movieBytes.size(), error);
		bOk&= writer.addMemberFromMemory("movie.pose.json", movieTrack.data(), movieTrack.size(), error);
		bOk&= writer.finish(error);
		return bOk;
	};

	// A well-formed trackcamera opens with both tracks loaded
	{
		const std::filesystem::path path= fixture.root / "take.trackcamera";
		success&= writeTrackCamera(path, markerTrackSameSession);

		Capture capture;
		success&= capture.open(path, error);
		if (!success)
			fprintf(stdout, "    open failed: %s\n", error.c_str());
		success&= capture.isOpen() && capture.getKind() == eCaptureKind::trackcamera && !capture.isStageSpace();
		success&= capture.hasMember(eCaptureMemberRole::movie) && !capture.hasMember(eCaptureMemberRole::depth);
		success&= capture.findMember(eCaptureMemberRole::plate_image) == nullptr;

		const PoseTrack& track= capture.getPoseTrack(eCaptureMemberRole::movie_pose);
		success&= track.isLoaded() && track.getFrameCount() == 5 && track.getSessionId() == k_sessionA;
		const PoseTrack& markerTrack= capture.getPoseTrack(eCaptureMemberRole::marker_pose);
		success&= markerTrack.isLoaded() && markerTrack.getFrameCount() == 1;
		success&= !capture.getPoseTrack(eCaptureMemberRole::movie).isLoaded();

		cv::Mat marker;
		success&= capture.decodeImage(eCaptureMemberRole::marker_image, marker, error);
		success&= marker.cols == 32 && marker.rows == 16 && marker.type() == CV_8UC3
				  && centrePixelMatches(marker, cv::Scalar(200, 100, 50), 0);
		success&= !capture.decodeImage(eCaptureMemberRole::plate_image, marker, error);

		const CaptureArchiveMember* movie= capture.findMember(eCaptureMemberRole::movie);
		success&= movie != nullptr && movie->size == movieBytes.size();
		const std::string url= capture.makeSubfileUrl(eCaptureMemberRole::movie);
		if (movie != nullptr)
		{
			const std::string expected= "subfile,,start," + std::to_string(movie->dataOffset) + ",end,"
										+ std::to_string(movie->dataOffset + movie->size) + ",,:";
			success&= url.compare(0, expected.size(), expected) == 0;
			success&= url.find(path.filename().string()) != std::string::npos;
		}
		success&= capture.makeSubfileUrl(eCaptureMemberRole::plate_image).empty();

		capture.close();
		success&= !capture.isOpen() && capture.getKind() == eCaptureKind::INVALID;
	}

	// A marker track from another session is rejected
	{
		const std::filesystem::path path= fixture.root / "mixed.trackcamera";
		success&= writeTrackCamera(path, markerTrackOtherSession);
		Capture capture;
		success&= !capture.open(path, error) && !capture.isOpen();
	}

	// The manifest's own session disagreeing with the movie track is rejected
	{
		const std::filesystem::path path= fixture.root / "manifest_session.trackcamera";
		CaptureManifest manifest= makeManifest(eCaptureKind::trackcamera, k_sessionB);
		manifest.members[eCaptureMemberRole::movie]= "movie.mp4";
		manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
		CaptureArchiveWriter writer;
		success&= writer.open(path, manifest, error);
		success&= writer.addMemberFromMemory("movie.mp4", movieBytes.data(), movieBytes.size(), error);
		success&= writer.addMemberFromMemory("movie.pose.json", movieTrack.data(), movieTrack.size(), error);
		success&= writer.finish(error);
		Capture capture;
		success&= !capture.open(path, error);
	}

	// A kind that disagrees with the extension is rejected
	{
		const std::filesystem::path path= fixture.root / "renamed.trackcamera";
		success&= writeTrackCamera(path, markerTrackSameSession);
		const std::filesystem::path renamed= fixture.root / "renamed.stillcamera";
		std::filesystem::rename(path, renamed);
		Capture capture;
		success&= !capture.open(renamed, error);
		success&= !capture.open(fixture.root / "renamed.tar", error);
	}

	// A manifest naming a member the archive lacks is rejected
	{
		const std::filesystem::path path= fixture.root / "missing_member.stillphoto";
		CaptureManifest manifest= makeManifest(eCaptureKind::stillphoto);
		manifest.members[eCaptureMemberRole::plate_image]= "plate.png";
		success&= writeRawArchive(path, {{CaptureManifest::k_manifestMemberName, manifest.toJsonString(), nullptr}});
		Capture capture;
		success&= !capture.open(path, error);
	}

	// A member pose track that is not a pose track is rejected
	{
		const std::filesystem::path path= fixture.root / "bad_track.trackcamera";
		CaptureManifest manifest= makeManifest(eCaptureKind::trackcamera);
		manifest.members[eCaptureMemberRole::movie]= "movie.mp4";
		manifest.members[eCaptureMemberRole::movie_pose]= "movie.pose.json";
		CaptureArchiveWriter writer;
		success&= writer.open(path, manifest, error);
		success&= writer.addMemberFromMemory("movie.mp4", movieBytes.data(), movieBytes.size(), error);
		const std::string notATrack= "{\"format\": \"something-else\"}";
		success&= writer.addMemberFromMemory("movie.pose.json", notATrack.data(), notATrack.size(), error);
		success&= writer.finish(error);
		Capture capture;
		success&= !capture.open(path, error);
	}

	// A stage-space capture reports itself as such
	{
		const std::filesystem::path path= fixture.root / "editor.stillphoto";
		CaptureManifest manifest= makeManifest(eCaptureKind::stillphoto, "0123456789abcdef0123456789abcdef");
		manifest.poseSpace= eCapturePoseSpace::stage;
		manifest.members[eCaptureMemberRole::plate_image]= "plate.png";
		CaptureArchiveWriter writer;
		success&= writer.open(path, manifest, error);
		success&= writer.addMemberFromMemory("plate.png", markerPng.data(), markerPng.size(), error);
		success&= writer.finish(error);
		Capture capture;
		success&= capture.open(path, error) && capture.isStageSpace();
		success&= !capture.getPoseTrack(eCaptureMemberRole::movie_pose).isLoaded();
	}

	UNIT_TEST_COMPLETE()
}

bool capture_archive_test_movie_member_plays_in_place()
{
	UNIT_TEST_BEGIN("a movie member plays through the decoder's subfile URL without extraction")
	using namespace CaptureArchiveTestsInternal;
	FixtureDir fixture;
	std::string error;

	const std::filesystem::path moviePath= fixture.root / "movie.avi";
	success&= writeMovieFixture(moviePath);
	if (!success)
		fprintf(stdout, "    could not write the movie fixture\n");

	// The movie is the third member, so its data offset is well past zero
	const std::vector<uint8_t> plate= encodePng(cv::Scalar(1, 2, 3));
	const std::string markerTrack= makePoseTrackJson(k_sessionA, 1);
	CaptureManifest manifest= makeManifest(eCaptureKind::stillcamera);
	manifest.members[eCaptureMemberRole::plate_image]= "plate.png";
	manifest.members[eCaptureMemberRole::marker_pose]= "marker.pose.json";
	manifest.members[eCaptureMemberRole::movie]= "movie.avi";

	const std::filesystem::path archivePath= fixture.root / "take.stillcamera";
	{
		CaptureArchiveWriter writer;
		success&= writer.open(archivePath, manifest, error);
		success&= writer.addMemberFromMemory("plate.png", plate.data(), plate.size(), error);
		success&= writer.addMemberFromMemory("marker.pose.json", markerTrack.data(), markerTrack.size(), error);
		success&= writer.addMemberFromFile("movie.avi", moviePath, error);
		success&= writer.finish(error);
	}

	Capture capture;
	success&= capture.open(archivePath, error);
	if (!success)
		fprintf(stdout, "    open failed: %s\n", error.c_str());

	const CaptureArchiveMember* movieMember= capture.findMember(eCaptureMemberRole::movie);
	success&= movieMember != nullptr && movieMember->dataOffset > 1024;

	// The loose file and the member report the same stream
	MovieDecoder loose;
	success&= loose.open(moviePath);
	MovieDecoder packed;
	success&= packed.openMovieUrl(capture.makeSubfileUrl(eCaptureMemberRole::movie));
	if (!success)
		fprintf(stdout, "    subfile open failed\n");
	success&= packed.isOpen() && !packed.isStill();
	success&= packed.getWidth() == k_movieWidth && packed.getHeight() == k_movieHeight;
	success&= packed.getFrameCount() == loose.getFrameCount() && packed.getFrameCount() == k_movieFrameCount;
	success&= std::abs(packed.getFps() - loose.getFps()) < 0.01;

	// Every frame reads, in order, with the written colour
	int framesRead= 0;
	cv::Mat frame;
	int64_t ptsUs= 0;
	while (packed.readNext(frame, ptsUs))
	{
		success&= centrePixelMatches(frame, movieFrameColor(framesRead), 12);
		++framesRead;
	}
	success&= framesRead == k_movieFrameCount;
	if (framesRead != k_movieFrameCount)
		fprintf(stdout, "    read %d frames, expected %d\n", framesRead, k_movieFrameCount);

	// A seek lands on the same frame in both
	const double seekSeconds= 6.0 / k_movieFps;
	success&= packed.seek(seekSeconds) && loose.seek(seekSeconds);
	cv::Mat packedFrame;
	cv::Mat looseFrame;
	int64_t packedPts= 0;
	int64_t loosePts= 0;
	success&= packed.readNext(packedFrame, packedPts) && loose.readNext(looseFrame, loosePts);
	success&= packedPts == loosePts;
	success&= centrePixelMatches(packedFrame, movieFrameColor(6), 12);

	// An image member decodes through the same decoder as a still
	std::vector<uint8_t> plateBytes;
	success&= capture.readMemberBytes(eCaptureMemberRole::plate_image, plateBytes, error);
	MovieDecoder still;
	success&=
		still.openEncodedStill(plateBytes) && still.isStill() && still.getWidth() == 32 && still.getHeight() == 16;
	success&= still.readNext(frame, ptsUs) && centrePixelMatches(frame, cv::Scalar(1, 2, 3), 0);
	success&= !still.openEncodedStill(std::vector<uint8_t>{1, 2, 3});

	UNIT_TEST_COMPLETE()
}
