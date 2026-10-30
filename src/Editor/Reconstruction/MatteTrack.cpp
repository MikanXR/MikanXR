#include "MatteTrack.h"
#include "Logger.h"

#include <configuru.hpp>

#include <chrono>
#include <cstring>
#include <ctime>
#include <limits>
#include <system_error>
#include <utility>

namespace MatteTrackInternal
{
// A run's first byte: the label above the continue flag above 5 bits of length
const int k_runLabelShift= 6;
const uint8_t k_runFirstContinueBit= 0x20;
const uint8_t k_runFirstLengthMask= 0x1f;
const int k_runFirstLengthBits= 5;
// The bytes after it: a continue flag above 7 bits of length
const uint8_t k_runNextContinueBit= 0x80;
const uint8_t k_runNextLengthMask= 0x7f;
const int k_runNextLengthBits= 7;

void appendRun(uint8_t label, uint64_t length, std::vector<uint8_t>& outEncoded)
{
	uint64_t remaining= length - 1;
	uint8_t byte= (uint8_t)((label << k_runLabelShift) | (remaining & k_runFirstLengthMask));
	remaining>>= k_runFirstLengthBits;
	if (remaining != 0)
		byte|= k_runFirstContinueBit;
	outEncoded.push_back(byte);

	while (remaining != 0)
	{
		byte= (uint8_t)(remaining & k_runNextLengthMask);
		remaining>>= k_runNextLengthBits;
		if (remaining != 0)
			byte|= k_runNextContinueBit;
		outEncoded.push_back(byte);
	}
}

void putLittleEndian(uint64_t value, size_t byteCount, uint8_t* outBytes)
{
	for (size_t byteIndex= 0; byteIndex < byteCount; ++byteIndex)
		outBytes[byteIndex]= (uint8_t)(value >> (8 * byteIndex));
}

uint64_t getLittleEndian(const uint8_t* bytes, size_t byteCount)
{
	uint64_t value= 0;
	for (size_t byteIndex= 0; byteIndex < byteCount; ++byteIndex)
		value|= (uint64_t)bytes[byteIndex] << (8 * byteIndex);
	return value;
}

// Reads entry recordIndex of the index at the start of a bin
void readIndexEntry(const uint8_t* bin, size_t recordIndex, uint64_t& outOffset, uint32_t& outSize)
{
	const uint8_t* entry= bin + recordIndex * MatteTrack::k_indexEntryBytes;
	outOffset= getLittleEndian(entry, 8);
	outSize= (uint32_t)getLittleEndian(entry + 8, 4);
}

std::string makeCreatedString()
{
	const std::time_t now= std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm localTime= {};
	if (localtime_s(&localTime, &now) != 0)
		return std::string();

	char buffer[32];
	const size_t length= std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &localTime);
	return std::string(buffer, length);
}

configuru::Config headerToConfig(const MatteTrackHeader& header)
{
	configuru::Config pt= configuru::Config::object();
	pt["format"]= MatteTrack::k_formatName;
	pt["version"]= static_cast<int64_t>(MatteTrack::k_version);
	pt["session_id"]= header.sessionId;
	pt["image_width"]= static_cast<int64_t>(header.imageWidth);
	pt["image_height"]= static_cast<int64_t>(header.imageHeight);
	pt["matte_width"]= static_cast<int64_t>(header.matteWidth);
	pt["matte_height"]= static_cast<int64_t>(header.matteHeight);
	pt["band_px"]= static_cast<int64_t>(header.bandPx);
	pt["frame_step"]= static_cast<int64_t>(header.frameStep);
	pt["record_count"]= static_cast<int64_t>(header.recordCount);
	pt["model"]= header.model;
	pt["created"]= header.created;

	configuru::Config stencils= configuru::Config::array();
	for (const MatteTrackStencilRef& stencil : header.stencils)
	{
		configuru::Config stencilConfig= configuru::Config::object();
		stencilConfig["name"]= stencil.name;
		stencilConfig["model_path"]= stencil.modelPath;
		stencils.push_back(stencilConfig);
	}
	pt["stencils"]= stencils;

	return pt;
}

bool readInt64(const configuru::Config& pt, const char* key, int64_t& outValue, std::string& outError)
{
	if (!pt.has_key(key) || !pt[key].is_int())
	{
		outError= std::string("Matte track header has no integer ") + key;
		return false;
	}

	outValue= pt[key].as_integer<int64_t>();
	return true;
}

bool readInt(const configuru::Config& pt, const char* key, int& outValue, std::string& outError)
{
	int64_t value= 0;
	if (!readInt64(pt, key, value, outError))
		return false;

	if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
	{
		outError= std::string("Matte track header ") + key + " is out of range";
		return false;
	}

	outValue= (int)value;
	return true;
}

bool readString(const configuru::Config& pt, const char* key, std::string& outValue, std::string& outError)
{
	if (!pt.has_key(key) || !pt[key].is_string())
	{
		outError= std::string("Matte track header has no string ") + key;
		return false;
	}

	outValue= pt[key].as_string();
	return true;
}

// Parses and validates everything but the format and version, which the caller
// has checked
bool configToHeader(const configuru::Config& pt, MatteTrackHeader& outHeader, std::string& outError)
{
	MatteTrackHeader header;
	int64_t recordCount= 0;
	if (!readString(pt, "session_id", header.sessionId, outError)
		|| !readInt(pt, "image_width", header.imageWidth, outError)
		|| !readInt(pt, "image_height", header.imageHeight, outError)
		|| !readInt(pt, "matte_width", header.matteWidth, outError)
		|| !readInt(pt, "matte_height", header.matteHeight, outError)
		|| !readInt(pt, "band_px", header.bandPx, outError) || !readInt(pt, "frame_step", header.frameStep, outError)
		|| !readInt64(pt, "record_count", recordCount, outError) || !readString(pt, "model", header.model, outError)
		|| !readString(pt, "created", header.created, outError))
	{
		return false;
	}

	if (recordCount < 0)
	{
		outError= "Matte track header record_count is negative";
		return false;
	}
	header.recordCount= (size_t)recordCount;

	if (!pt.has_key("stencils") || !pt["stencils"].is_array())
	{
		outError= "Matte track header has no stencils array";
		return false;
	}

	for (const configuru::Config& stencilConfig : pt["stencils"].as_array())
	{
		if (!stencilConfig.is_object())
		{
			outError= "Matte track header stencil is not an object";
			return false;
		}

		MatteTrackStencilRef stencil;
		if (!readString(stencilConfig, "name", stencil.name, outError)
			|| !readString(stencilConfig, "model_path", stencil.modelPath, outError))
		{
			return false;
		}
		header.stencils.push_back(std::move(stencil));
	}

	if (!MatteTrack::validateHeader(header, outError))
		return false;

	outHeader= std::move(header);
	return true;
}
} // namespace MatteTrackInternal

// -- MatteTrack -----
std::filesystem::path MatteTrack::sidecarPathForCapture(const std::filesystem::path& capturePath)
{
	return capturePath.parent_path() / (capturePath.stem().string() + ".matte.json");
}

std::filesystem::path MatteTrack::binPathForJson(const std::filesystem::path& jsonPath)
{
	std::filesystem::path binPath= jsonPath;
	binPath.replace_extension(".bin");
	return binPath;
}

size_t MatteTrack::getIndexBytes(size_t recordCount) { return recordCount * k_indexEntryBytes; }

bool MatteTrack::validateHeader(const MatteTrackHeader& header, std::string& outError)
{
	if (header.sessionId.empty())
	{
		outError= "Matte track header session_id is empty";
		return false;
	}

	if (header.imageWidth <= 0 || header.imageHeight <= 0)
	{
		outError= "Matte track header image size is not positive";
		return false;
	}

	if (header.matteWidth <= 0 || header.matteHeight <= 0)
	{
		outError= "Matte track header matte size is not positive";
		return false;
	}

	if (header.bandPx < 0)
	{
		outError= "Matte track header band_px is negative";
		return false;
	}

	if (header.frameStep < 1)
	{
		outError= "Matte track header frame_step is less than 1";
		return false;
	}

	return true;
}

void MatteTrack::encodeRecord(const cv::Mat& labels, std::vector<uint8_t>& outEncoded)
{
	outEncoded.clear();
	if (labels.empty() || labels.type() != CV_8UC1)
		return;

	const uint8_t keep= (uint8_t)eMatteLabel::keep;
	const uint8_t added= (uint8_t)eMatteLabel::added;
	uint8_t runLabel= keep;
	uint64_t runLength= 0;
	for (int y= 0; y < labels.rows; ++y)
	{
		const uint8_t* labelRow= labels.ptr<uint8_t>(y);
		for (int x= 0; x < labels.cols; ++x)
		{
			const uint8_t label= labelRow[x] > added ? keep : labelRow[x];
			if (runLength > 0 && label == runLabel)
			{
				++runLength;
				continue;
			}

			if (runLength > 0)
				MatteTrackInternal::appendRun(runLabel, runLength, outEncoded);
			runLabel= label;
			runLength= 1;
		}
	}
	MatteTrackInternal::appendRun(runLabel, runLength, outEncoded);
}

bool MatteTrack::decodeRecord(const uint8_t* encoded, size_t encodedBytes, int width, int height,
							  const MatteLabelValues& labelValues, cv::Mat& outLabels)
{
	if (width <= 0 || height <= 0)
		return false;

	// Runs cross row ends, so they fill the image as one block
	outLabels.create(height, width, CV_8UC1);
	if (!outLabels.isContinuous())
		outLabels= cv::Mat(height, width, CV_8UC1);

	uint8_t* labels= outLabels.data;
	const uint64_t pixelCount= (uint64_t)width * (uint64_t)height;
	uint64_t pixelsWritten= 0;
	size_t position= 0;
	while (position < encodedBytes)
	{
		const uint8_t first= encoded[position++];
		const uint8_t label= (uint8_t)(first >> MatteTrackInternal::k_runLabelShift);
		if (label > (uint8_t)eMatteLabel::added)
			return false;

		uint64_t lengthMinusOne= first & MatteTrackInternal::k_runFirstLengthMask;
		bool bContinue= (first & MatteTrackInternal::k_runFirstContinueBit) != 0;
		int shift= MatteTrackInternal::k_runFirstLengthBits;
		while (bContinue)
		{
			if (position >= encodedBytes || shift >= 64)
				return false;

			const uint8_t next= encoded[position++];
			lengthMinusOne|= (uint64_t)(next & MatteTrackInternal::k_runNextLengthMask) << shift;
			shift+= MatteTrackInternal::k_runNextLengthBits;
			bContinue= (next & MatteTrackInternal::k_runNextContinueBit) != 0;
		}

		if (lengthMinusOne >= pixelCount - pixelsWritten)
			return false;

		std::memset(labels + pixelsWritten, labelValues[label], (size_t)lengthMinusOne + 1);
		pixelsWritten+= lengthMinusOne + 1;
	}

	return pixelsWritten == pixelCount;
}

// -- MatteTrackWriter -----
MatteTrackWriter::~MatteTrackWriter()
{
	if (m_bIsOpen)
		discard();
}

bool MatteTrackWriter::open(const std::filesystem::path& jsonPath, const MatteTrackHeader& header)
{
	if (m_bIsOpen)
		discard();

	std::string error;
	if (!MatteTrack::validateHeader(header, error))
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::open") << error;
		return false;
	}

	if (header.recordCount == 0)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Matte track has no records";
		return false;
	}

	m_header= header;
	m_header.created= MatteTrackInternal::makeCreatedString();
	m_jsonPath= jsonPath;
	m_binPath= MatteTrack::binPathForJson(jsonPath);
	m_jsonTempPath= jsonPath.string() + ".tmp";
	m_binTempPath= m_binPath.string() + ".tmp";
	m_index.assign(header.recordCount, IndexEntry());
	m_binBytes= (uint64_t)MatteTrack::getIndexBytes(header.recordCount);
	m_lastEncoded.clear();
	m_lastEntry= IndexEntry();

	std::error_code ec;
	std::filesystem::create_directories(jsonPath.parent_path(), ec);

	{
		std::ofstream jsonFile(m_jsonTempPath, std::ios::binary | std::ios::trunc);
		if (!jsonFile.is_open())
		{
			MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Could not open " << m_jsonTempPath;
			removeTempFiles();
			return false;
		}

		jsonFile << configuru::dump_string(MatteTrackInternal::headerToConfig(m_header), configuru::JSON);
		jsonFile.close();
		if (!jsonFile)
		{
			MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Could not write " << m_jsonTempPath;
			removeTempFiles();
			return false;
		}
	}

	m_binFile.open(m_binTempPath, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
	if (!m_binFile.is_open())
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Could not open " << m_binTempPath;
		removeTempFiles();
		return false;
	}

	// Every record starts unwritten, so finish writes the index over zeros
	const std::vector<char> zeroIndex((size_t)m_binBytes, 0);
	m_binFile.write(zeroIndex.data(), (std::streamsize)zeroIndex.size());
	if (!m_binFile)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Could not size " << m_binTempPath;
		removeTempFiles();
		return false;
	}

	m_bIsOpen= true;
	return true;
}

bool MatteTrackWriter::writeRecord(size_t recordIndex, const cv::Mat& labels)
{
	if (!m_bIsOpen || recordIndex >= m_header.recordCount)
		return false;

	if (labels.type() != CV_8UC1 || labels.cols != m_header.matteWidth || labels.rows != m_header.matteHeight)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::writeRecord") << "Record " << (uint64_t)recordIndex << " is not CV_8UC1 "
														 << m_header.matteWidth << "x" << m_header.matteHeight;
		return false;
	}

	MatteTrack::encodeRecord(labels, m_encoded);
	if (m_encoded.empty() || m_encoded.size() > (size_t)std::numeric_limits<uint32_t>::max())
		return false;

	// A frame step holds the last labels over the frames between, so a record
	// often repeats the one before it
	if (m_lastEntry.size != 0 && m_encoded == m_lastEncoded)
	{
		m_index[recordIndex]= m_lastEntry;
		return true;
	}

	m_binFile.clear();
	m_binFile.seekp((std::streamoff)m_binBytes, std::ios::beg);
	m_binFile.write((const char*)m_encoded.data(), (std::streamsize)m_encoded.size());
	if (!m_binFile)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::writeRecord") << "Failed to write record " << (uint64_t)recordIndex;
		return false;
	}

	m_lastEntry.offset= m_binBytes;
	m_lastEntry.size= (uint32_t)m_encoded.size();
	m_index[recordIndex]= m_lastEntry;
	m_binBytes+= m_encoded.size();
	std::swap(m_lastEncoded, m_encoded);
	return true;
}

bool MatteTrackWriter::finish()
{
	if (!m_bIsOpen)
		return false;

	std::vector<uint8_t> indexBytes(MatteTrack::getIndexBytes(m_index.size()));
	for (size_t recordIndex= 0; recordIndex < m_index.size(); ++recordIndex)
	{
		uint8_t* entry= indexBytes.data() + recordIndex * MatteTrack::k_indexEntryBytes;
		MatteTrackInternal::putLittleEndian(m_index[recordIndex].offset, 8, entry);
		MatteTrackInternal::putLittleEndian(m_index[recordIndex].size, 4, entry + 8);
	}

	m_binFile.clear();
	m_binFile.seekp(0, std::ios::beg);
	m_binFile.write((const char*)indexBytes.data(), (std::streamsize)indexBytes.size());
	m_binFile.flush();
	const bool bBinOk= (bool)m_binFile;
	closeFile();
	if (!bBinOk)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::finish") << "Could not write " << m_binTempPath;
		discard();
		return false;
	}

	// The bin goes first so that a json in place always has its bin
	std::error_code ec;
	std::filesystem::rename(m_binTempPath, m_binPath, ec);
	if (ec)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::finish")
			<< "Could not move " << m_binTempPath << " into place: " << ec.message();
		discard();
		return false;
	}

	std::filesystem::rename(m_jsonTempPath, m_jsonPath, ec);
	if (ec)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::finish")
			<< "Could not move " << m_jsonTempPath << " into place: " << ec.message();
		std::filesystem::remove(m_binPath, ec);
		discard();
		return false;
	}

	m_bIsOpen= false;
	return true;
}

void MatteTrackWriter::discard()
{
	removeTempFiles();
	m_bIsOpen= false;
}

void MatteTrackWriter::closeFile()
{
	if (m_binFile.is_open())
		m_binFile.close();
	m_binFile.clear();
	m_index.clear();
	m_encoded.clear();
	m_lastEncoded.clear();
}

void MatteTrackWriter::removeTempFiles()
{
	closeFile();

	std::error_code ec;
	if (!m_jsonTempPath.empty())
		std::filesystem::remove(m_jsonTempPath, ec);
	if (!m_binTempPath.empty())
		std::filesystem::remove(m_binTempPath, ec);
}

// -- MatteTrackReader -----
bool MatteTrackReader::open(const std::filesystem::path& jsonPath, const std::string& expectedSessionId,
							size_t expectedRecordCount, std::string& outError)
{
	close();
	outError.clear();

	std::error_code ec;
	if (!std::filesystem::is_regular_file(jsonPath, ec))
	{
		outError= "Matte track header does not exist: " + jsonPath.string();
		return false;
	}

	MatteTrackHeader header;
	try
	{
		const configuru::Config pt= configuru::parse_file(jsonPath.string(), configuru::JSON);
		if (!pt.is_object())
		{
			outError= "Matte track header root is not an object";
			return false;
		}

		if (!pt.has_key("format") || !pt["format"].is_string() || pt["format"].as_string() != MatteTrack::k_formatName)
		{
			outError= "Matte track format is missing or not " + MatteTrack::k_formatName;
			return false;
		}

		if (!pt.has_key("version") || !pt["version"].is_int())
		{
			outError= "Matte track version is missing";
			return false;
		}

		const int64_t version= pt["version"].as_integer<int64_t>();
		if (version != (int64_t)MatteTrack::k_version)
		{
			outError= "Matte track version " + std::to_string(version) + " is not "
					  + std::to_string((int64_t)MatteTrack::k_version);
			return false;
		}

		if (!MatteTrackInternal::configToHeader(pt, header, outError))
			return false;
	}
	catch (std::exception& e)
	{
		outError= "Failed to parse matte track header " + jsonPath.string() + ": " + e.what();
		return false;
	}

	if (header.sessionId != expectedSessionId)
	{
		outError= "Matte track belongs to session " + header.sessionId + ", expected " + expectedSessionId;
		return false;
	}

	if (header.recordCount != expectedRecordCount)
	{
		outError= "Matte track has " + std::to_string(header.recordCount) + " records, expected "
				  + std::to_string(expectedRecordCount);
		return false;
	}

	const std::filesystem::path binPath= MatteTrack::binPathForJson(jsonPath);
	if (!std::filesystem::is_regular_file(binPath, ec))
	{
		outError= "Matte track data file is missing: " + binPath.string();
		return false;
	}

	std::vector<uint8_t> data;
	{
		std::ifstream binFile(binPath, std::ios::in | std::ios::binary | std::ios::ate);
		if (!binFile.is_open())
		{
			outError= "Failed to open " + binPath.string();
			return false;
		}

		const std::streamoff binBytes= binFile.tellg();
		data.resize(binBytes > 0 ? (size_t)binBytes : 0);
		binFile.seekg(0, std::ios::beg);
		binFile.read((char*)data.data(), (std::streamsize)data.size());
		if (!binFile)
		{
			outError= "Failed to read " + binPath.string();
			return false;
		}
	}

	const size_t indexBytes= MatteTrack::getIndexBytes(header.recordCount);
	if (data.size() < indexBytes)
	{
		outError= "Matte track data file " + binPath.string() + " is " + std::to_string(data.size())
				  + " bytes, shorter than its " + std::to_string(indexBytes) + " byte index";
		return false;
	}

	for (size_t recordIndex= 0; recordIndex < header.recordCount; ++recordIndex)
	{
		uint64_t offset= 0;
		uint32_t size= 0;
		MatteTrackInternal::readIndexEntry(data.data(), recordIndex, offset, size);
		if (size != 0 && (offset < indexBytes || offset > data.size() || size > data.size() - offset))
		{
			outError= "Matte track data file " + binPath.string() + " places record " + std::to_string(recordIndex)
					  + " outside the file";
			return false;
		}
	}

	m_header= std::move(header);
	m_data= std::move(data);
	m_bIsOpen= true;
	return true;
}

void MatteTrackReader::close()
{
	m_header= MatteTrackHeader();
	m_data.clear();
	m_data.shrink_to_fit();
	m_bIsOpen= false;
}

bool MatteTrackReader::readRecord(size_t recordIndex, cv::Mat& outLabels, const MatteLabelValues& labelValues) const
{
	if (!m_bIsOpen || recordIndex >= m_header.recordCount)
		return false;

	uint64_t offset= 0;
	uint32_t size= 0;
	MatteTrackInternal::readIndexEntry(m_data.data(), recordIndex, offset, size);
	if (size == 0)
	{
		outLabels.create(m_header.matteHeight, m_header.matteWidth, CV_8UC1);
		outLabels.setTo(labelValues[(size_t)eMatteLabel::keep]);
		return true;
	}

	if (!MatteTrack::decodeRecord(m_data.data() + offset, size, m_header.matteWidth, m_header.matteHeight, labelValues,
								  outLabels))
	{
		MIKAN_LOG_ERROR("MatteTrackReader::readRecord") << "Record " << (uint64_t)recordIndex << " is malformed";
		return false;
	}

	return true;
}
