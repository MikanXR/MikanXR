#pragma once

#include <opencv2/core.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// What the occlusion matte does to one pixel of the live video
enum class eMatteLabel : uint8_t
{
	removed= 0, // a stencil covers it but the take saw nothing there
	keep= 1,    // the stencil does not change it
	added= 2,   // the take saw something the stencil does not cover
};

// One stencil model the matte was built against
struct MatteTrackStencilRef
{
	std::string name;      // the stencil component's name
	std::string modelPath; // the model asset the stencil draws
};

// The matte track's JSON header
struct MatteTrackHeader
{
	std::string sessionId;
	// The video size the matte belongs to
	int imageWidth= 0;
	int imageHeight= 0;
	// The size of one record
	int matteWidth= 0;
	int matteHeight= 0;
	// Width in matte pixels of the band around a stencil edge the labels cover
	int bandPx= 0;
	// Frames between records built from a new frame
	int frameStep= 1;
	// Equals the pose track's frame count: record i belongs to pose frame i
	size_t recordCount= 0;
	// The model that produced the labels
	std::string model;
	// ISO 8601 local time, filled in by MatteTrackWriter::open
	std::string created;
	std::vector<MatteTrackStencilRef> stencils;
};

// The per-frame label map of an occlusion matte, stored beside a capture as two
// regenerable sidecar files. <stem>.matte.json carries the MatteTrackHeader.
// <stem>.matte.bin holds recordCount headerless records of matteHeight rows of
// rowBytes bytes each, so record i starts at i * recordBytes. A pixel is two
// bits, least significant first: pixel x of a row is in byte x / 4 at bit
// (x % 4) * 2. Values are eMatteLabel, and a record never written reads as all
// keep.
class MatteTrack
{
public:
	inline static const std::string k_formatName= "mikan-matte-track";
	static constexpr int k_version= 1;
	// Four pixels of keep: 01 01 01 01
	static constexpr uint8_t k_keepByte= 0x55;

	// The json path of the sidecar for <dir>/<stem>.<ext>: <dir>/<stem>.matte.json
	static std::filesystem::path sidecarPathForCapture(const std::filesystem::path& capturePath);
	// The bin path that goes with a json path: <stem>.matte.json to <stem>.matte.bin
	static std::filesystem::path binPathForJson(const std::filesystem::path& jsonPath);

	static size_t getRowBytes(int matteWidth);
	static size_t getRecordBytes(int matteWidth, int matteHeight);

	// Checks the fields the file layout depends on. Says which one failed in
	// outError.
	static bool validateHeader(const MatteTrackHeader& header, std::string& outError);

	// Packs a CV_8UC1 label image of values 0, 1, 2 into one record of
	// getRecordBytes(width, height) bytes. A value above 2 packs as keep.
	// Leaves outPacked empty when labels is not a non-empty CV_8UC1 image.
	static void packRecord(const cv::Mat& labels, std::vector<uint8_t>& outPacked);
	// Unpacks one record into a CV_8UC1 height x width image of values 0, 1, 2.
	// The unused value 3 unpacks as keep.
	static void unpackRecord(const uint8_t* packed, int width, int height, cv::Mat& outLabels);
};

// Writes a matte track to temporary files and renames them into place on
// finish, so a reader never sees a partial track. Not thread safe: one writer
// per track, owned by whichever thread stores the records. A writer destroyed
// without finish discards its temporary files.
class MatteTrackWriter
{
public:
	MatteTrackWriter()= default;
	~MatteTrackWriter();
	MatteTrackWriter(const MatteTrackWriter&)= delete;
	MatteTrackWriter& operator=(const MatteTrackWriter&)= delete;

	// Writes <json>.tmp and <bin>.tmp, the bin pre-sized to recordCount records
	// of keep. created is filled in here. Logs and returns false on failure.
	bool open(const std::filesystem::path& jsonPath, const MatteTrackHeader& header);
	bool isOpen() const { return m_bIsOpen; }
	const MatteTrackHeader& getHeader() const { return m_header; }

	// labels is CV_8UC1 matteHeight x matteWidth with values 0, 1, 2. Records
	// may arrive in any order and a record may be rewritten.
	bool writeRecord(size_t recordIndex, const cv::Mat& labels);
	// Flushes, then renames the bin and then the json into place. True on
	// success, else the temporary files are removed.
	bool finish();
	// Closes and removes the temporary files
	void discard();

private:
	void closeFile();
	void removeTempFiles();

	MatteTrackHeader m_header;
	std::filesystem::path m_jsonPath;
	std::filesystem::path m_binPath;
	std::filesystem::path m_jsonTempPath;
	std::filesystem::path m_binTempPath;
	std::fstream m_binFile;
	std::vector<uint8_t> m_packed;
	size_t m_recordBytes= 0;
	bool m_bIsOpen= false;
};

// Reads records of a matte track by one seek each. Not thread safe.
class MatteTrackReader
{
public:
	// Rejects a missing or malformed header, another format or version, a
	// session other than expectedSessionId, a record count other than
	// expectedRecordCount, and a bin file whose size does not match. outError
	// says which. Failures are reported through outError only, so the caller
	// decides whether to log them.
	bool open(const std::filesystem::path& jsonPath, const std::string& expectedSessionId, size_t expectedRecordCount,
			  std::string& outError);
	void close();
	bool isOpen() const { return m_bIsOpen; }

	const MatteTrackHeader& getHeader() const { return m_header; }

	// outLabels is CV_8UC1 matteHeight x matteWidth, values 0, 1, 2. Returns
	// false on a read failure or an index past the end.
	bool readRecord(size_t recordIndex, cv::Mat& outLabels) const;

private:
	MatteTrackHeader m_header;
	mutable std::ifstream m_binFile;
	mutable std::vector<uint8_t> m_packed;
	size_t m_recordBytes= 0;
	bool m_bIsOpen= false;
};
