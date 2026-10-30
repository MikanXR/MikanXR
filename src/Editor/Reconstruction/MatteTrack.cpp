#include "MatteTrack.h"
#include "Logger.h"

#include <configuru.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <limits>
#include <system_error>
#include <utility>

namespace MatteTrackInternal
{
// Bytes written per call while pre-sizing the bin file
const size_t k_fillChunkBytes= 1 << 20;

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

size_t MatteTrack::getRowBytes(int matteWidth) { return ((size_t)matteWidth + 3) / 4; }

size_t MatteTrack::getRecordBytes(int matteWidth, int matteHeight)
{
	return getRowBytes(matteWidth) * (size_t)matteHeight;
}

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

void MatteTrack::packRecord(const cv::Mat& labels, std::vector<uint8_t>& outPacked)
{
	outPacked.clear();
	if (labels.empty() || labels.type() != CV_8UC1)
		return;

	const int width= labels.cols;
	const int height= labels.rows;
	const size_t rowBytes= getRowBytes(width);
	outPacked.assign(rowBytes * (size_t)height, 0);

	const uint8_t keep= (uint8_t)eMatteLabel::keep;
	const uint8_t added= (uint8_t)eMatteLabel::added;
	for (int y= 0; y < height; ++y)
	{
		const uint8_t* labelRow= labels.ptr<uint8_t>(y);
		uint8_t* packedRow= outPacked.data() + (size_t)y * rowBytes;
		for (int x= 0; x < width; ++x)
		{
			const uint8_t label= labelRow[x] > added ? keep : labelRow[x];
			packedRow[x / 4]|= (uint8_t)(label << ((x % 4) * 2));
		}

		// The pixels past the row's end read as keep so the padding is deterministic
		for (int x= width; x < (int)rowBytes * 4; ++x)
			packedRow[x / 4]|= (uint8_t)(keep << ((x % 4) * 2));
	}
}

void MatteTrack::unpackRecord(const uint8_t* packed, int width, int height, cv::Mat& outLabels)
{
	outLabels.create(height, width, CV_8UC1);

	const size_t rowBytes= getRowBytes(width);
	const uint8_t keep= (uint8_t)eMatteLabel::keep;
	for (int y= 0; y < height; ++y)
	{
		const uint8_t* packedRow= packed + (size_t)y * rowBytes;
		uint8_t* labelRow= outLabels.ptr<uint8_t>(y);
		for (int x= 0; x < width; ++x)
		{
			const uint8_t label= (uint8_t)((packedRow[x / 4] >> ((x % 4) * 2)) & 0x3);
			labelRow[x]= label > (uint8_t)eMatteLabel::added ? keep : label;
		}
	}
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
	m_recordBytes= MatteTrack::getRecordBytes(header.matteWidth, header.matteHeight);

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

	// Every record starts as keep, so a record never written reads as keep
	const uint64_t totalBytes= (uint64_t)header.recordCount * (uint64_t)m_recordBytes;
	const std::vector<char> fill((size_t)std::min<uint64_t>(totalBytes, (uint64_t)MatteTrackInternal::k_fillChunkBytes),
								 (char)MatteTrack::k_keepByte);
	for (uint64_t written= 0; written < totalBytes;)
	{
		const uint64_t chunk= std::min<uint64_t>(totalBytes - written, (uint64_t)fill.size());
		m_binFile.write(fill.data(), (std::streamsize)chunk);
		written+= chunk;
		if (!m_binFile)
		{
			MIKAN_LOG_ERROR("MatteTrackWriter::open") << "Could not size " << m_binTempPath;
			removeTempFiles();
			return false;
		}
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

	MatteTrack::packRecord(labels, m_packed);
	if (m_packed.size() != m_recordBytes)
		return false;

	m_binFile.clear();
	m_binFile.seekp((std::streamoff)((uint64_t)recordIndex * (uint64_t)m_recordBytes), std::ios::beg);
	m_binFile.write((const char*)m_packed.data(), (std::streamsize)m_packed.size());
	if (!m_binFile)
	{
		MIKAN_LOG_ERROR("MatteTrackWriter::writeRecord") << "Failed to write record " << (uint64_t)recordIndex;
		return false;
	}

	return true;
}

bool MatteTrackWriter::finish()
{
	if (!m_bIsOpen)
		return false;

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
	const uint64_t recordBytes= (uint64_t)MatteTrack::getRecordBytes(header.matteWidth, header.matteHeight);
	const uint64_t expectedBinBytes= (uint64_t)header.recordCount * recordBytes;
	const uint64_t binBytes= (uint64_t)std::filesystem::file_size(binPath, ec);
	if (ec)
	{
		outError= "Matte track data file is missing: " + binPath.string();
		return false;
	}

	if (binBytes != expectedBinBytes)
	{
		outError= "Matte track data file " + binPath.string() + " is " + std::to_string(binBytes) + " bytes, expected "
				  + std::to_string(expectedBinBytes);
		return false;
	}

	m_binFile.open(binPath, std::ios::in | std::ios::binary);
	if (!m_binFile.is_open())
	{
		outError= "Failed to open " + binPath.string();
		close();
		return false;
	}

	m_header= std::move(header);
	m_recordBytes= (size_t)recordBytes;
	m_bIsOpen= true;
	return true;
}

void MatteTrackReader::close()
{
	if (m_binFile.is_open())
		m_binFile.close();
	m_binFile.clear();

	m_header= MatteTrackHeader();
	m_packed.clear();
	m_recordBytes= 0;
	m_bIsOpen= false;
}

bool MatteTrackReader::readRecord(size_t recordIndex, cv::Mat& outLabels) const
{
	if (!m_bIsOpen || recordIndex >= m_header.recordCount)
		return false;

	m_packed.resize(m_recordBytes);
	m_binFile.clear();
	m_binFile.seekg((std::streamoff)((uint64_t)recordIndex * (uint64_t)m_recordBytes), std::ios::beg);
	m_binFile.read((char*)m_packed.data(), (std::streamsize)m_packed.size());
	if (!m_binFile || m_binFile.gcount() != (std::streamsize)m_packed.size())
	{
		MIKAN_LOG_ERROR("MatteTrackReader::readRecord") << "Failed to read record " << (uint64_t)recordIndex;
		return false;
	}

	MatteTrack::unpackRecord(m_packed.data(), m_header.matteWidth, m_header.matteHeight, outLabels);
	return true;
}
