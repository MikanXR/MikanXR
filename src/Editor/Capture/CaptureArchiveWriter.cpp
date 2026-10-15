#include "CaptureArchiveWriter.h"
#include "CaptureArchiveReader.h"
#include "CaptureUstarHeader.h"
#include "Logger.h"

#include <algorithm>
#include <ctime>
#include <system_error>
#include <vector>

namespace
{
constexpr size_t k_copyChunkSize= 1 << 20;
}

CaptureArchiveWriter::~CaptureArchiveWriter()
{
	if (m_bIsOpen)
		abort();
}

bool CaptureArchiveWriter::open(const std::filesystem::path& path, const CaptureManifest& manifest,
								std::string& outError)
{
	if (m_bIsOpen)
		abort();

	if (!manifest.validate(outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::open") << path.string() << ": " << outError;
		return false;
	}

	if (CaptureKindUtils::kindFromExtension(path) != manifest.kind)
	{
		outError= "The extension of " + path.filename().string() + " does not name a "
				  + CaptureKindUtils::kindToString(manifest.kind) + " capture";
		MIKAN_LOG_ERROR("CaptureArchiveWriter::open") << outError;
		return false;
	}

	for (const std::string& name : manifest.getMemberNames())
	{
		if (!CaptureArchiveReader::isValidMemberName(name))
		{
			outError= "Member name is not accepted: " + name;
			MIKAN_LOG_ERROR("CaptureArchiveWriter::open") << path.string() << ": " << outError;
			return false;
		}
	}

	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);

	m_finalPath= path;
	m_tempPath= path.string() + ".tmp";
	m_file.open(m_tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!m_file.is_open())
	{
		outError= "Cannot create " + m_tempPath.string();
		MIKAN_LOG_ERROR("CaptureArchiveWriter::open") << outError;
		m_finalPath.clear();
		m_tempPath.clear();
		return false;
	}

	m_manifest= manifest;
	m_writtenNames.clear();
	m_bIsOpen= true;

	const std::string manifestJson= manifest.toJsonString();
	if (!beginMember(CaptureManifest::k_manifestMemberName, manifestJson.size(), outError)
		|| !writeBytes(manifestJson.data(), manifestJson.size(), outError) || !endMember(manifestJson.size(), outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::open") << path.string() << ": " << outError;
		abort();
		return false;
	}

	return true;
}

bool CaptureArchiveWriter::addMemberFromMemory(const std::string& name, const void* data, size_t size,
											   std::string& outError)
{
	if (!beginMember(name, size, outError) || !writeBytes(data, size, outError) || !endMember(size, outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromMemory") << m_finalPath.string() << ": " << outError;
		return false;
	}

	return true;
}

bool CaptureArchiveWriter::addMemberFromFile(const std::string& name, const std::filesystem::path& sourcePath,
											 std::string& outError)
{
	std::error_code error;
	const uint64_t size= (uint64_t)std::filesystem::file_size(sourcePath, error);
	if (error)
	{
		outError= "Cannot read the size of " + sourcePath.string() + ": " + error.message();
		MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << outError;
		return false;
	}

	std::ifstream source(sourcePath, std::ios::in | std::ios::binary);
	if (!source.is_open())
	{
		outError= "Cannot open " + sourcePath.string();
		MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << outError;
		return false;
	}

	if (!beginMember(name, size, outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << m_finalPath.string() << ": " << outError;
		return false;
	}

	// The header already carries the size, so the copy stops at exactly that
	// many bytes and a source that shrank underneath us is an error
	std::vector<char> chunk(k_copyChunkSize);
	uint64_t remaining= size;
	while (remaining > 0)
	{
		const size_t toRead= (size_t)std::min<uint64_t>(remaining, chunk.size());
		source.read(chunk.data(), (std::streamsize)toRead);
		if (!source || source.gcount() != (std::streamsize)toRead)
		{
			outError= "Read of " + sourcePath.string() + " ended early";
			MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << outError;
			return false;
		}

		if (!writeBytes(chunk.data(), toRead, outError))
		{
			MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << m_finalPath.string() << ": " << outError;
			return false;
		}

		remaining-= toRead;
	}

	if (!endMember(size, outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::addMemberFromFile") << m_finalPath.string() << ": " << outError;
		return false;
	}

	return true;
}

bool CaptureArchiveWriter::finish(std::string& outError)
{
	if (!m_bIsOpen)
	{
		outError= "Archive is not open";
		return false;
	}

	for (const std::string& name : m_manifest.getMemberNames())
	{
		if (m_writtenNames.find(name) == m_writtenNames.end())
		{
			outError= "Manifest member " + name + " was never added";
			MIKAN_LOG_ERROR("CaptureArchiveWriter::finish") << m_finalPath.string() << ": " << outError;
			abort();
			return false;
		}
	}

	// Two zero blocks mark the end of the archive
	char zeroBlocks[2 * CaptureUstarHeader::k_blockSize]= {};
	if (!writeBytes(zeroBlocks, sizeof(zeroBlocks), outError))
	{
		MIKAN_LOG_ERROR("CaptureArchiveWriter::finish") << m_finalPath.string() << ": " << outError;
		abort();
		return false;
	}

	m_file.close();
	if (!m_file)
	{
		outError= "Flush of " + m_tempPath.string() + " failed";
		MIKAN_LOG_ERROR("CaptureArchiveWriter::finish") << outError;
		abort();
		return false;
	}

	std::error_code error;
	std::filesystem::rename(m_tempPath, m_finalPath, error);
	if (error)
	{
		outError= "Cannot move " + m_tempPath.string() + " into place: " + error.message();
		MIKAN_LOG_ERROR("CaptureArchiveWriter::finish") << outError;
		abort();
		return false;
	}

	m_tempPath.clear();
	m_writtenNames.clear();
	m_bIsOpen= false;
	return true;
}

void CaptureArchiveWriter::abort()
{
	if (m_file.is_open())
		m_file.close();
	m_file.clear();

	if (!m_tempPath.empty())
	{
		std::error_code ignored;
		std::filesystem::remove(m_tempPath, ignored);
	}

	m_tempPath.clear();
	m_writtenNames.clear();
	m_bIsOpen= false;
}

bool CaptureArchiveWriter::beginMember(const std::string& name, uint64_t size, std::string& outError)
{
	if (!m_bIsOpen)
	{
		outError= "Archive is not open";
		return false;
	}

	if (name != CaptureManifest::k_manifestMemberName)
	{
		bool bDeclared= false;
		for (const std::string& declared : m_manifest.getMemberNames())
		{
			if (declared == name)
			{
				bDeclared= true;
				break;
			}
		}
		if (!bDeclared)
		{
			outError= "Member " + name + " is not declared by the manifest";
			return false;
		}
	}

	if (m_writtenNames.find(name) != m_writtenNames.end())
	{
		outError= "Member " + name + " was already added";
		return false;
	}

	if (size > CaptureArchiveReader::k_maxMemberSize)
	{
		outError= "Member " + name + " exceeds the ustar size limit";
		return false;
	}

	char header[CaptureUstarHeader::k_blockSize];
	CaptureUstarHeader::write(header, name, size, (int64_t)std::time(nullptr));
	if (!writeBytes(header, sizeof(header), outError))
		return false;

	m_writtenNames.insert(name);
	return true;
}

bool CaptureArchiveWriter::endMember(uint64_t size, std::string& outError)
{
	const uint64_t padding=
		(CaptureUstarHeader::k_blockSize - size % CaptureUstarHeader::k_blockSize) % CaptureUstarHeader::k_blockSize;
	if (padding == 0)
		return true;

	char zeros[CaptureUstarHeader::k_blockSize]= {};
	return writeBytes(zeros, (size_t)padding, outError);
}

bool CaptureArchiveWriter::writeBytes(const void* data, size_t size, std::string& outError)
{
	if (size == 0)
		return true;

	m_file.write((const char*)data, (std::streamsize)size);
	if (!m_file)
	{
		outError= "Write to " + m_tempPath.string() + " failed";
		return false;
	}

	return true;
}
