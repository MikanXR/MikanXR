#include "DepthTrackReader.h"
#include "Logger.h"

#include <cstdint>
#include <vector>

bool DepthTrackReader::open(const std::filesystem::path& archivePath, const CaptureArchiveMember& depthMember,
							const CaptureArchiveMember& confidenceMember, int width, int height, double depthUnitMm,
							size_t recordCount)
{
	close();

	if (width <= 0 || height <= 0)
	{
		MIKAN_LOG_ERROR("DepthTrackReader::open") << "Invalid depth record size " << width << "x" << height;
		return false;
	}

	const uint64_t pixelCount= (uint64_t)width * (uint64_t)height;
	const uint64_t expectedDepthBytes= (uint64_t)recordCount * pixelCount * 2;
	const uint64_t expectedConfidenceBytes= (uint64_t)recordCount * pixelCount;

	if (depthMember.size != expectedDepthBytes)
	{
		MIKAN_LOG_ERROR("DepthTrackReader::open")
			<< archivePath << " member " << depthMember.name << " is " << depthMember.size << " bytes, expected "
			<< expectedDepthBytes << " for " << (uint64_t)recordCount << " records of " << width << "x" << height;
		return false;
	}

	if (confidenceMember.size != expectedConfidenceBytes)
	{
		MIKAN_LOG_ERROR("DepthTrackReader::open")
			<< archivePath << " member " << confidenceMember.name << " is " << confidenceMember.size
			<< " bytes, expected " << expectedConfidenceBytes << " for " << (uint64_t)recordCount << " records of "
			<< width << "x" << height;
		return false;
	}

	m_file.open(archivePath, std::ios::in | std::ios::binary);
	if (!m_file.is_open())
	{
		MIKAN_LOG_ERROR("DepthTrackReader::open") << "Failed to open " << archivePath;
		close();
		return false;
	}

	m_depthOffset= depthMember.dataOffset;
	m_confidenceOffset= confidenceMember.dataOffset;
	m_width= width;
	m_height= height;
	m_depthUnitMm= depthUnitMm;
	m_recordCount= recordCount;
	m_bIsOpen= true;
	return true;
}

void DepthTrackReader::close()
{
	if (m_file.is_open())
		m_file.close();
	m_file.clear();

	m_depthOffset= 0;
	m_confidenceOffset= 0;
	m_width= 0;
	m_height= 0;
	m_depthUnitMm= 1.0;
	m_recordCount= 0;
	m_bIsOpen= false;
}

bool DepthTrackReader::readRecord(size_t recordIndex, cv::Mat& outDepthMeters, cv::Mat& outConfidence)
{
	if (!m_bIsOpen || recordIndex >= m_recordCount)
		return false;

	const size_t pixelCount= (size_t)m_width * (size_t)m_height;

	std::vector<uint8_t> depthBytes(pixelCount * 2);
	m_file.clear();
	m_file.seekg((std::streamoff)(m_depthOffset + (uint64_t)recordIndex * pixelCount * 2), std::ios::beg);
	m_file.read((char*)depthBytes.data(), (std::streamsize)depthBytes.size());
	if (!m_file || m_file.gcount() != (std::streamsize)depthBytes.size())
	{
		MIKAN_LOG_ERROR("DepthTrackReader::readRecord") << "Failed to read depth record " << (uint64_t)recordIndex;
		return false;
	}

	std::vector<uint8_t> confidenceBytes(pixelCount);
	m_file.clear();
	m_file.seekg((std::streamoff)(m_confidenceOffset + (uint64_t)recordIndex * pixelCount), std::ios::beg);
	m_file.read((char*)confidenceBytes.data(), (std::streamsize)confidenceBytes.size());
	if (!m_file || m_file.gcount() != (std::streamsize)confidenceBytes.size())
	{
		MIKAN_LOG_ERROR("DepthTrackReader::readRecord") << "Failed to read confidence record " << (uint64_t)recordIndex;
		return false;
	}

	const double metersPerUnit= m_depthUnitMm / 1000.0;
	outDepthMeters.create(m_height, m_width, CV_32FC1);
	outConfidence.create(m_height, m_width, CV_8UC1);
	for (int y= 0; y < m_height; ++y)
	{
		float* depthRow= outDepthMeters.ptr<float>(y);
		uint8_t* confidenceRow= outConfidence.ptr<uint8_t>(y);
		const size_t rowStart= (size_t)y * (size_t)m_width;
		for (int x= 0; x < m_width; ++x)
		{
			const size_t pixel= rowStart + (size_t)x;
			// Little-endian uint16 depth units
			const uint16_t value= (uint16_t)(depthBytes[pixel * 2] | (depthBytes[pixel * 2 + 1] << 8));
			depthRow[x]= (float)((double)value * metersPerUnit);
			confidenceRow[x]= confidenceBytes[pixel];
		}
	}

	return true;
}

void DepthTrackReader::confidenceToWeight(const cv::Mat& confidence, cv::Mat& outWeight)
{
	confidence.convertTo(outWeight, CV_32F, 0.5);
}
