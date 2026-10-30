#include "MatteTrackTests.h"
#include "MatteTrack.h"
#include "unit_test.h"

#include <opencv2/core.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>
#include <vector>

namespace MatteTrackTestsInternal
{
// Not a multiple of 4, so every row ends in a partly used byte
const int k_matteWidth= 37;
const int k_matteHeight= 11;
const std::string k_sessionId= "session-matte-tests";

// A fresh empty directory for one test
std::filesystem::path makeTestDirectory(const std::string& testName)
{
	const std::filesystem::path directory=
		std::filesystem::temp_directory_path() / "mikan_matte_track_tests" / testName;
	std::error_code ec;
	std::filesystem::remove_all(directory, ec);
	std::filesystem::create_directories(directory, ec);
	return directory;
}

void removeTestDirectory(const std::filesystem::path& directory)
{
	std::error_code ec;
	std::filesystem::remove_all(directory, ec);
}

MatteTrackHeader makeHeader(size_t recordCount)
{
	MatteTrackHeader header;
	header.sessionId= k_sessionId;
	header.imageWidth= 1920;
	header.imageHeight= 1080;
	header.matteWidth= k_matteWidth;
	header.matteHeight= k_matteHeight;
	header.bandPx= 6;
	header.frameStep= 2;
	header.recordCount= recordCount;
	header.model= "test-model";
	header.stencils.push_back({"Table", "models/table.glb"});
	header.stencils.push_back({"Sofa", "models/sofa.glb"});
	return header;
}

cv::Mat makeRandomLabels(std::mt19937& generator)
{
	cv::Mat labels(k_matteHeight, k_matteWidth, CV_8UC1);
	for (int y= 0; y < labels.rows; ++y)
	{
		for (int x= 0; x < labels.cols; ++x)
			labels.at<uint8_t>(y, x)= (uint8_t)(generator() % 3);
	}
	return labels;
}

bool labelsEqual(const cv::Mat& a, const cv::Mat& b)
{
	return a.type() == b.type() && a.rows == b.rows && a.cols == b.cols && cv::countNonZero(a != b) == 0;
}

bool isAllKeep(const cv::Mat& labels)
{
	return labels.type() == CV_8UC1 && labels.rows == k_matteHeight && labels.cols == k_matteWidth
		   && cv::countNonZero(labels != (int)eMatteLabel::keep) == 0;
}

// Writes a finished track of recordCount random records, returning them
bool writeTrack(const std::filesystem::path& jsonPath, size_t recordCount, std::vector<cv::Mat>& outRecords)
{
	std::mt19937 generator(12345);
	MatteTrackWriter writer;
	if (!writer.open(jsonPath, makeHeader(recordCount)))
		return false;

	outRecords.clear();
	for (size_t index= 0; index < recordCount; ++index)
	{
		outRecords.push_back(makeRandomLabels(generator));
		if (!writer.writeRecord(index, outRecords.back()))
			return false;
	}

	return writer.finish();
}
} // namespace MatteTrackTestsInternal

bool run_matte_track_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("matte_track")
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_header_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_records_round_trip);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_open_rejects_mismatches);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_sidecar_paths);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_temp_files);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_pack_layout);
	UNIT_TEST_MODULE_END()
}

bool matte_track_test_header_round_trip()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("every header field survives a write and a read")
	const std::filesystem::path directory= makeTestDirectory("header_round_trip");
	const std::filesystem::path jsonPath= directory / "take.matte.json";
	std::vector<cv::Mat> records;
	success&= writeTrack(jsonPath, 3, records);

	MatteTrackReader reader;
	std::string error;
	success&= reader.open(jsonPath, k_sessionId, 3, error);
	if (success)
	{
		const MatteTrackHeader expected= makeHeader(3);
		const MatteTrackHeader& header= reader.getHeader();
		success&= header.sessionId == expected.sessionId;
		success&= header.imageWidth == expected.imageWidth;
		success&= header.imageHeight == expected.imageHeight;
		success&= header.matteWidth == expected.matteWidth;
		success&= header.matteHeight == expected.matteHeight;
		success&= header.bandPx == expected.bandPx;
		success&= header.frameStep == expected.frameStep;
		success&= header.recordCount == expected.recordCount;
		success&= header.model == expected.model;
		success&= !header.created.empty();
		success&= header.stencils.size() == 2;
		if (header.stencils.size() == 2)
		{
			success&= header.stencils[0].name == "Table" && header.stencils[0].modelPath == "models/table.glb";
			success&= header.stencils[1].name == "Sofa" && header.stencils[1].modelPath == "models/sofa.glb";
		}
	}
	reader.close();
	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_records_round_trip()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("records read back equal, in any write order, and an unwritten record reads as keep")
	const std::filesystem::path directory= makeTestDirectory("records_round_trip");

	// Three random records written in the order 2, 0, then 1
	{
		const std::filesystem::path jsonPath= directory / "ordered.matte.json";
		std::mt19937 generator(12345);
		std::vector<cv::Mat> records;
		for (int index= 0; index < 3; ++index)
			records.push_back(makeRandomLabels(generator));

		MatteTrackWriter writer;
		success&= writer.open(jsonPath, makeHeader(3));
		success&= writer.writeRecord(2, records[2]);
		success&= writer.writeRecord(0, records[0]);
		success&= writer.writeRecord(1, records[1]);
		success&= !writer.writeRecord(3, records[0]);
		success&= writer.finish();

		MatteTrackReader reader;
		std::string error;
		success&= reader.open(jsonPath, k_sessionId, 3, error);
		for (size_t index= 0; index < 3; ++index)
		{
			cv::Mat labels;
			success&= reader.readRecord(index, labels);
			success&= labelsEqual(labels, records[index]);
		}
		cv::Mat past;
		success&= !reader.readRecord(3, past);
	}

	// Only the middle of four records is written
	{
		const std::filesystem::path jsonPath= directory / "sparse.matte.json";
		std::mt19937 generator(777);
		const cv::Mat written= makeRandomLabels(generator);

		MatteTrackWriter writer;
		success&= writer.open(jsonPath, makeHeader(4));
		success&= writer.writeRecord(1, written);
		// A record of the wrong size is refused
		success&= !writer.writeRecord(2, cv::Mat(k_matteHeight, k_matteWidth + 1, CV_8UC1, cv::Scalar(0)));
		success&= writer.finish();

		MatteTrackReader reader;
		std::string error;
		success&= reader.open(jsonPath, k_sessionId, 4, error);
		for (size_t index= 0; index < 4; ++index)
		{
			cv::Mat labels;
			success&= reader.readRecord(index, labels);
			if (index == 1)
				success&= labelsEqual(labels, written);
			else
				success&= isAllKeep(labels);
		}
	}

	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_open_rejects_mismatches()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("open rejects another session, another record count, and a truncated bin")
	const std::filesystem::path directory= makeTestDirectory("open_rejects");
	const std::filesystem::path jsonPath= directory / "take.matte.json";
	std::vector<cv::Mat> records;
	success&= writeTrack(jsonPath, 3, records);

	MatteTrackReader reader;
	std::string error;
	success&= !reader.open(jsonPath, "another-session", 3, error);
	success&= !error.empty();
	success&= !reader.isOpen();

	error.clear();
	success&= !reader.open(jsonPath, k_sessionId, 4, error);
	success&= !error.empty();

	error.clear();
	success&= !reader.open(directory / "missing.matte.json", k_sessionId, 3, error);
	success&= !error.empty();

	// The right session and count still open
	error.clear();
	success&= reader.open(jsonPath, k_sessionId, 3, error);
	success&= error.empty();
	reader.close();

	const std::filesystem::path binPath= MatteTrack::binPathForJson(jsonPath);
	std::error_code ec;
	const uintmax_t binBytes= std::filesystem::file_size(binPath, ec);
	success&= binBytes == 3 * MatteTrack::getRecordBytes(k_matteWidth, k_matteHeight);
	std::filesystem::resize_file(binPath, binBytes - 1, ec);
	success&= !ec;

	error.clear();
	success&= !reader.open(jsonPath, k_sessionId, 3, error);
	success&= !error.empty();
	success&= !reader.isOpen();

	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_sidecar_paths()
{
	UNIT_TEST_BEGIN("the sidecar paths follow the capture's stem")
	const std::filesystem::path capturePath= std::filesystem::path("root") / "captures" / "take_x.trackcamera";
	const std::filesystem::path jsonPath= MatteTrack::sidecarPathForCapture(capturePath);
	success&= jsonPath == std::filesystem::path("root") / "captures" / "take_x.matte.json";
	success&= MatteTrack::binPathForJson(jsonPath) == std::filesystem::path("root") / "captures" / "take_x.matte.bin";
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_temp_files()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("discard leaves nothing and finish leaves only the two final files")
	const std::filesystem::path directory= makeTestDirectory("temp_files");
	const std::filesystem::path jsonPath= directory / "take.matte.json";
	const std::filesystem::path binPath= MatteTrack::binPathForJson(jsonPath);
	const std::filesystem::path jsonTempPath= jsonPath.string() + ".tmp";
	const std::filesystem::path binTempPath= binPath.string() + ".tmp";

	MatteTrackWriter writer;
	success&= writer.open(jsonPath, makeHeader(2));
	success&= std::filesystem::exists(jsonTempPath) && std::filesystem::exists(binTempPath);
	success&= !std::filesystem::exists(jsonPath) && !std::filesystem::exists(binPath);
	writer.discard();
	success&= !writer.isOpen();
	success&= !std::filesystem::exists(jsonTempPath) && !std::filesystem::exists(binTempPath);
	success&= !std::filesystem::exists(jsonPath) && !std::filesystem::exists(binPath);

	success&= writer.open(jsonPath, makeHeader(2));
	success&= writer.finish();
	success&= !writer.isOpen();
	success&= !std::filesystem::exists(jsonTempPath) && !std::filesystem::exists(binTempPath);
	success&= std::filesystem::exists(jsonPath) && std::filesystem::exists(binPath);

	size_t fileCount= 0;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
	{
		(void)entry;
		++fileCount;
	}
	success&= fileCount == 2;

	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_pack_layout()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("pack and unpack round trip, with two bits per pixel least significant first")

	// Labels 0, 1, 2, 1 in one byte: 00 | 01 << 2 | 10 << 4 | 01 << 6
	cv::Mat row(1, 4, CV_8UC1);
	row.at<uint8_t>(0, 0)= 0;
	row.at<uint8_t>(0, 1)= 1;
	row.at<uint8_t>(0, 2)= 2;
	row.at<uint8_t>(0, 3)= 1;
	std::vector<uint8_t> packed;
	MatteTrack::packRecord(row, packed);
	success&= packed.size() == 1;
	success&= !packed.empty() && packed[0] == 0x64;

	cv::Mat unpacked;
	MatteTrack::unpackRecord(packed.data(), 4, 1, unpacked);
	success&= labelsEqual(unpacked, row);

	// A random record whose width is not a multiple of 4
	std::mt19937 generator(4242);
	const cv::Mat labels= makeRandomLabels(generator);
	MatteTrack::packRecord(labels, packed);
	success&= MatteTrack::getRowBytes(k_matteWidth) == 10;
	success&= packed.size() == MatteTrack::getRecordBytes(k_matteWidth, k_matteHeight);
	success&= packed.size() == 10 * (size_t)k_matteHeight;
	MatteTrack::unpackRecord(packed.data(), k_matteWidth, k_matteHeight, unpacked);
	success&= labelsEqual(unpacked, labels);

	// An all-keep record is the byte pattern the writer pre-sizes with
	MatteTrack::packRecord(cv::Mat(k_matteHeight, k_matteWidth, CV_8UC1, cv::Scalar((int)eMatteLabel::keep)), packed);
	bool bAllKeepBytes= !packed.empty();
	for (uint8_t value : packed)
		bAllKeepBytes&= value == MatteTrack::k_keepByte;
	success&= bAllKeepBytes;

	// A label past 2 packs as keep, and a non-label image packs to nothing
	MatteTrack::packRecord(cv::Mat(1, 4, CV_8UC1, cv::Scalar(7)), packed);
	success&= packed.size() == 1 && packed[0] == MatteTrack::k_keepByte;
	MatteTrack::packRecord(cv::Mat(1, 4, CV_32FC1, cv::Scalar(0)), packed);
	success&= packed.empty();
	UNIT_TEST_COMPLETE()
}
