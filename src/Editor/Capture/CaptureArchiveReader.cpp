#include "CaptureArchiveReader.h"
#include "CaptureManifest.h"
#include "CaptureUstarHeader.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <system_error>

CaptureArchiveReader::~CaptureArchiveReader() { close(); }

bool CaptureArchiveReader::open(const std::filesystem::path& path, std::string& outError)
{
	close();

	std::error_code error;
	const uint64_t fileSize= (uint64_t)std::filesystem::file_size(path, error);
	if (error)
	{
		outError= "Cannot read the size of " + path.string() + ": " + error.message();
		MIKAN_LOG_ERROR("CaptureArchiveReader::open") << outError;
		return false;
	}

	m_file.open(path, std::ios::in | std::ios::binary);
	if (!m_file.is_open())
	{
		outError= "Cannot open " + path.string();
		MIKAN_LOG_ERROR("CaptureArchiveReader::open") << outError;
		return false;
	}

	m_path= path;
	if (!parseHeaders(fileSize, outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveReader::open") << path.string() << ": " << outError;
		close();
		return false;
	}

	m_bIsOpen= true;
	return true;
}

bool CaptureArchiveReader::parseHeaders(uint64_t fileSize, std::string& outError)
{
	uint64_t offset= 0;
	char block[k_blockSize];

	while (true)
	{
		if (offset + k_blockSize > fileSize)
		{
			outError= "Archive ends without an end-of-archive block";
			return false;
		}

		m_file.clear();
		m_file.seekg((std::streamoff)offset, std::ios::beg);
		m_file.read(block, (std::streamsize)k_blockSize);
		if (!m_file || m_file.gcount() != (std::streamsize)k_blockSize)
		{
			outError= "Read failed at header offset " + std::to_string(offset);
			return false;
		}

		if (CaptureUstarHeader::isZeroBlock(block))
		{
			// The end-of-archive marker. The second zero block and any trailing
			// padding are not inspected.
			break;
		}

		CaptureArchiveMember member;
		if (!CaptureUstarHeader::parse(block, member.name, member.size, outError))
		{
			outError+= " (header at offset " + std::to_string(offset) + ")";
			return false;
		}

		if (!isValidMemberName(member.name))
		{
			outError= "Member name is not accepted: " + member.name;
			return false;
		}

		if (member.size > k_maxMemberSize)
		{
			outError= "Member " + member.name + " exceeds the ustar size limit";
			return false;
		}

		member.dataOffset= offset + k_blockSize;
		if (member.dataOffset + member.size > fileSize)
		{
			outError= "Member " + member.name + " runs past the end of the archive";
			return false;
		}

		if (m_members.empty() && member.name != CaptureManifest::k_manifestMemberName)
		{
			outError= "The first member is " + member.name + ", not " + CaptureManifest::k_manifestMemberName;
			return false;
		}

		if (findMember(member.name) != nullptr)
		{
			outError= "Member " + member.name + " appears twice";
			return false;
		}

		m_members.push_back(member);

		const uint64_t paddedSize= (member.size + k_blockSize - 1) / k_blockSize * k_blockSize;
		offset= member.dataOffset + paddedSize;
	}

	if (m_members.empty())
	{
		outError= "Archive has no members";
		return false;
	}

	return true;
}

void CaptureArchiveReader::close()
{
	if (m_file.is_open())
		m_file.close();
	m_file.clear();
	m_path.clear();
	m_members.clear();
	m_bIsOpen= false;
}

const CaptureArchiveMember* CaptureArchiveReader::findMember(const std::string& name) const
{
	for (const CaptureArchiveMember& member : m_members)
	{
		if (member.name == name)
			return &member;
	}

	return nullptr;
}

bool CaptureArchiveReader::readMember(const CaptureArchiveMember& member, std::vector<uint8_t>& outBytes,
									  std::string& outError)
{
	if (!m_bIsOpen)
	{
		outError= "Archive is not open";
		return false;
	}

	outBytes.resize((size_t)member.size);
	if (member.size == 0)
		return true;

	m_file.clear();
	m_file.seekg((std::streamoff)member.dataOffset, std::ios::beg);
	m_file.read((char*)outBytes.data(), (std::streamsize)member.size);
	if (!m_file || m_file.gcount() != (std::streamsize)member.size)
	{
		outError= "Read of member " + member.name + " failed";
		MIKAN_LOG_ERROR("CaptureArchiveReader::readMember") << m_path.string() << ": " << outError;
		outBytes.clear();
		return false;
	}

	return true;
}

bool CaptureArchiveReader::readMemberString(const std::string& name, std::string& outText, std::string& outError)
{
	const CaptureArchiveMember* member= findMember(name);
	if (member == nullptr)
	{
		outError= "Archive has no member " + name;
		return false;
	}

	std::vector<uint8_t> bytes;
	if (!readMember(*member, bytes, outError))
		return false;

	outText.assign((const char*)bytes.data(), bytes.size());
	return true;
}

bool CaptureArchiveReader::isValidMemberName(const std::string& name)
{
	if (name.empty() || name.size() > CaptureUstarHeader::k_nameFieldSize)
		return false;

	if (name == "." || name == "..")
		return false;

	for (char c : name)
	{
		const unsigned char uc= (unsigned char)c;
		if (uc < 0x20 || uc > 0x7E || c == '/' || c == '\\')
			return false;
	}

	return true;
}
