#pragma once

#include <opencv2/core.hpp>

#include <array>
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

// The byte a decoded record holds for each eMatteLabel: removed, keep, added
using MatteLabelValues= std::array<uint8_t, 3>;

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
// <stem>.matte.bin starts with an index of recordCount entries, each a little
// endian uint64 offset from the start of the file and a uint32 size, followed by
// the encoded records. A size of 0 is a record never written, which reads as all
// keep, and several entries may share one record.
//
// A record is the labels row-major as one sequence of runs covering every pixel, runs
// crossing row ends. A run's first byte holds the label (eMatteLabel) in bits 7-6,
// a continue flag in bit 5 and the low 5 bits of length - 1 in bits 4-0. While
// the continue flag is set, a byte follows with its own continue flag in bit 7
// and the next 7 bits of length - 1 in bits 6-0.
class MatteTrack
{
public:
	inline static const std::string k_formatName= "mikan-matte-track";
	static constexpr int k_version= 2;
	// Bytes per index entry: a uint64 offset and a uint32 size
	static constexpr size_t k_indexEntryBytes= 12;
	// Decodes to the labels themselves
	static constexpr MatteLabelValues k_labelValues= {(uint8_t)eMatteLabel::removed, (uint8_t)eMatteLabel::keep,
													  (uint8_t)eMatteLabel::added};

	// The json path of the sidecar for <dir>/<stem>.<ext>: <dir>/<stem>.matte.json
	static std::filesystem::path sidecarPathForCapture(const std::filesystem::path& capturePath);
	// The bin path that goes with a json path: <stem>.matte.json to <stem>.matte.bin
	static std::filesystem::path binPathForJson(const std::filesystem::path& jsonPath);

	static size_t getIndexBytes(size_t recordCount);

	// Checks the fields the file layout depends on. Says which one failed in
	// outError.
	static bool validateHeader(const MatteTrackHeader& header, std::string& outError);

	// Encodes a CV_8UC1 label image of values 0, 1, 2 into one record. A value
	// above 2 encodes as keep. Leaves outEncoded empty when labels is not a
	// non-empty CV_8UC1 image.
	static void encodeRecord(const cv::Mat& labels, std::vector<uint8_t>& outEncoded);
	// Decodes one record into a CV_8UC1 height x width image holding
	// labelValues[label] per pixel. False when the record is malformed: an
	// invalid label, a truncated run, or runs that do not cover the image
	// exactly.
	static bool decodeRecord(const uint8_t* encoded, size_t encodedBytes, int width, int height,
							 const MatteLabelValues& labelValues, cv::Mat& outLabels);
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

	// Writes <json>.tmp and <bin>.tmp, the bin starting with an index of
	// records never written. created is filled in here. Logs and returns false
	// on failure.
	bool open(const std::filesystem::path& jsonPath, const MatteTrackHeader& header);
	bool isOpen() const { return m_bIsOpen; }
	const MatteTrackHeader& getHeader() const { return m_header; }

	// labels is CV_8UC1 matteHeight x matteWidth with values 0, 1, 2. Records
	// may arrive in any order and a record may be rewritten. A record equal to
	// the one appended last shares its bytes.
	bool writeRecord(size_t recordIndex, const cv::Mat& labels);
	// Writes the index, then renames the bin and then the json into place. True
	// on success, else the temporary files are removed.
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
	struct IndexEntry
	{
		uint64_t offset= 0;
		uint32_t size= 0;
	};

	std::fstream m_binFile;
	std::vector<IndexEntry> m_index;
	// The bin's size, where the next record is appended
	uint64_t m_binBytes= 0;
	std::vector<uint8_t> m_encoded;
	// The record appended last and where it sits
	std::vector<uint8_t> m_lastEncoded;
	IndexEntry m_lastEntry;
	bool m_bIsOpen= false;
};

// Holds a whole matte track in memory, encoded, and decodes a record per read.
// Not thread safe.
class MatteTrackReader
{
public:
	// Rejects a missing or malformed header, another format or version, a
	// session other than expectedSessionId, a record count other than
	// expectedRecordCount, and a bin file too short for its index or with an
	// index entry outside it. Reads the bin whole and closes it. outError
	// says which. Failures are reported through outError only, so the caller
	// decides whether to log them.
	bool open(const std::filesystem::path& jsonPath, const std::string& expectedSessionId, size_t expectedRecordCount,
			  std::string& outError);
	void close();
	bool isOpen() const { return m_bIsOpen; }

	const MatteTrackHeader& getHeader() const { return m_header; }

	// outLabels is CV_8UC1 matteHeight x matteWidth holding labelValues[label]
	// per pixel. Returns false on a malformed record or an index past the end.
	bool readRecord(size_t recordIndex, cv::Mat& outLabels,
					const MatteLabelValues& labelValues= MatteTrack::k_labelValues) const;

private:
	MatteTrackHeader m_header;
	std::vector<uint8_t> m_data;
	bool m_bIsOpen= false;
};
