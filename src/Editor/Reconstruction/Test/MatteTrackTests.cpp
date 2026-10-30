#include "MatteTrackTests.h"
#include "MatteTrack.h"
#include "unit_test.h"

#include <opencv2/core.hpp>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_encode_layout);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_label_values);
	UNIT_TEST_MODULE_CALL_TEST(matte_track_test_dedup);
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

	UNIT_TEST_BEGIN("open rejects another session, another record count, a bin short of its index, and a record "
					"placed outside the bin")
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
	success&= binBytes > MatteTrack::getIndexBytes(3);

	// The last record's offset moved past the end of the bin
	{
		std::fstream binFile(binPath, std::ios::in | std::ios::out | std::ios::binary);
		binFile.seekp((std::streamoff)(2 * MatteTrack::k_indexEntryBytes), std::ios::beg);
		const uint8_t farOffset[8]= {0, 0, 0, 0, 0, 0, 1, 0};
		binFile.write((const char*)farOffset, sizeof(farOffset));
		success&= (bool)binFile;
	}

	error.clear();
	success&= !reader.open(jsonPath, k_sessionId, 3, error);
	success&= !error.empty();
	success&= !reader.isOpen();

	std::filesystem::resize_file(binPath, MatteTrack::getIndexBytes(3) - 1, ec);
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

bool matte_track_test_encode_layout()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("encode and decode round trip, with the run bytes the format defines")

	// Four runs of one pixel each: label in bits 7-6, no continue, length - 1 of 0
	cv::Mat row(1, 4, CV_8UC1);
	row.at<uint8_t>(0, 0)= 0;
	row.at<uint8_t>(0, 1)= 1;
	row.at<uint8_t>(0, 2)= 2;
	row.at<uint8_t>(0, 3)= 1;
	std::vector<uint8_t> encoded;
	MatteTrack::encodeRecord(row, encoded);
	success&= encoded == std::vector<uint8_t>({0x00, 0x40, 0x80, 0x40});

	cv::Mat decoded;
	success&= MatteTrack::decodeRecord(encoded.data(), encoded.size(), 4, 1, MatteTrack::k_labelValues, decoded);
	success&= labelsEqual(decoded, row);

	// One run of 100 keep: length - 1 = 99 = 3 << 5 | 3, so 01 1 00011 then 0 0000011
	MatteTrack::encodeRecord(cv::Mat(1, 100, CV_8UC1, cv::Scalar((int)eMatteLabel::keep)), encoded);
	success&= encoded == std::vector<uint8_t>({0x63, 0x03});

	// A run crosses row ends: a whole record of added is one run
	MatteTrack::encodeRecord(cv::Mat(k_matteHeight, k_matteWidth, CV_8UC1, cv::Scalar((int)eMatteLabel::added)),
							 encoded);
	success&= encoded.size() == 2;
	success&= MatteTrack::decodeRecord(encoded.data(), encoded.size(), k_matteWidth, k_matteHeight,
									   MatteTrack::k_labelValues, decoded);
	success&= decoded.rows == k_matteHeight && decoded.cols == k_matteWidth
			  && cv::countNonZero(decoded != (int)eMatteLabel::added) == 0;

	// A random record round trips
	std::mt19937 generator(4242);
	const cv::Mat labels= makeRandomLabels(generator);
	MatteTrack::encodeRecord(labels, encoded);
	success&= MatteTrack::decodeRecord(encoded.data(), encoded.size(), k_matteWidth, k_matteHeight,
									   MatteTrack::k_labelValues, decoded);
	success&= labelsEqual(decoded, labels);

	// A destination that is a view of a larger image is replaced by an image of its own
	cv::Mat larger(k_matteHeight + 2, k_matteWidth + 3, CV_8UC1, cv::Scalar(9));
	cv::Mat view= larger(cv::Rect(1, 1, k_matteWidth, k_matteHeight));
	success&= MatteTrack::decodeRecord(encoded.data(), encoded.size(), k_matteWidth, k_matteHeight,
									   MatteTrack::k_labelValues, view);
	success&= labelsEqual(view, labels);

	// A label past 2 encodes as keep, and a non-label image encodes to nothing
	MatteTrack::encodeRecord(cv::Mat(1, 4, CV_8UC1, cv::Scalar(7)), encoded);
	success&= encoded == std::vector<uint8_t>({0x43});
	MatteTrack::encodeRecord(cv::Mat(1, 4, CV_32FC1, cv::Scalar(0)), encoded);
	success&= encoded.empty();

	// Malformed records are refused: label 3, a run whose length is cut off, runs
	// short of the image, and runs past it
	const uint8_t labelThree[]= {0xC3};
	success&= !MatteTrack::decodeRecord(labelThree, sizeof(labelThree), 4, 1, MatteTrack::k_labelValues, decoded);
	const uint8_t cutOff[]= {0x63};
	success&= !MatteTrack::decodeRecord(cutOff, sizeof(cutOff), 100, 1, MatteTrack::k_labelValues, decoded);
	const uint8_t shortRuns[]= {0x42};
	success&= !MatteTrack::decodeRecord(shortRuns, sizeof(shortRuns), 4, 1, MatteTrack::k_labelValues, decoded);
	const uint8_t longRuns[]= {0x44};
	success&= !MatteTrack::decodeRecord(longRuns, sizeof(longRuns), 4, 1, MatteTrack::k_labelValues, decoded);
	const uint8_t extraRun[]= {0x43, 0x40};
	success&= !MatteTrack::decodeRecord(extraRun, sizeof(extraRun), 4, 1, MatteTrack::k_labelValues, decoded);
	success&= !MatteTrack::decodeRecord(nullptr, 0, 4, 1, MatteTrack::k_labelValues, decoded);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_label_values()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("a read writes the asked value per label, an unwritten record included")
	const std::filesystem::path directory= makeTestDirectory("label_values");
	const std::filesystem::path jsonPath= directory / "take.matte.json";
	std::mt19937 generator(99);
	const cv::Mat written= makeRandomLabels(generator);

	MatteTrackWriter writer;
	success&= writer.open(jsonPath, makeHeader(2));
	success&= writer.writeRecord(0, written);
	success&= writer.finish();

	const MatteLabelValues spread= {0, 128, 255};
	MatteTrackReader reader;
	std::string error;
	success&= reader.open(jsonPath, k_sessionId, 2, error);

	cv::Mat values;
	success&= reader.readRecord(0, values, spread);
	cv::Mat expected(written.size(), CV_8UC1);
	for (int y= 0; y < written.rows; ++y)
	{
		for (int x= 0; x < written.cols; ++x)
			expected.at<uint8_t>(y, x)= spread[written.at<uint8_t>(y, x)];
	}
	success&= labelsEqual(values, expected);

	success&= reader.readRecord(1, values, spread);
	success&= values.rows == k_matteHeight && values.cols == k_matteWidth && cv::countNonZero(values != 128) == 0;

	reader.close();
	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}

bool matte_track_test_dedup()
{
	using namespace MatteTrackTestsInternal;

	UNIT_TEST_BEGIN("a record equal to the one appended last shares its bytes")
	const std::filesystem::path directory= makeTestDirectory("dedup");
	const std::filesystem::path jsonPath= directory / "take.matte.json";
	std::mt19937 generator(31);
	const cv::Mat held= makeRandomLabels(generator);
	const cv::Mat other= makeRandomLabels(generator);
	std::vector<uint8_t> heldEncoded;
	std::vector<uint8_t> otherEncoded;
	MatteTrack::encodeRecord(held, heldEncoded);
	MatteTrack::encodeRecord(other, otherEncoded);

	// Records 0 to 2 hold one set of labels, record 3 another
	MatteTrackWriter writer;
	success&= writer.open(jsonPath, makeHeader(4));
	success&= writer.writeRecord(0, held);
	success&= writer.writeRecord(1, held);
	success&= writer.writeRecord(2, held);
	success&= writer.writeRecord(3, other);
	success&= writer.finish();

	std::error_code ec;
	const uintmax_t binBytes= std::filesystem::file_size(MatteTrack::binPathForJson(jsonPath), ec);
	success&= binBytes == MatteTrack::getIndexBytes(4) + heldEncoded.size() + otherEncoded.size();

	MatteTrackReader reader;
	std::string error;
	success&= reader.open(jsonPath, k_sessionId, 4, error);
	for (size_t index= 0; index < 4; ++index)
	{
		cv::Mat labels;
		success&= reader.readRecord(index, labels);
		success&= labelsEqual(labels, index < 3 ? held : other);
	}

	reader.close();
	removeTestDirectory(directory);
	UNIT_TEST_COMPLETE()
}
