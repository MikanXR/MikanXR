#include "NpyWriter.h"

#include "Logger.h"

#include <fstream>
#include <string>

bool NpyWriter::write(const std::filesystem::path& path, const std::vector<uint8_t>& values)
{
	return writeRaw(path, "|u1", values.data(), sizeof(uint8_t), values.size());
}

bool NpyWriter::write(const std::filesystem::path& path, const std::vector<uint16_t>& values)
{
	return writeRaw(path, "<u2", values.data(), sizeof(uint16_t), values.size());
}

bool NpyWriter::write(const std::filesystem::path& path, const std::vector<float>& values)
{
	return writeRaw(path, "<f4", values.data(), sizeof(float), values.size());
}

std::vector<char> NpyWriter::makeHeader(const char* typeDescriptor, size_t count)
{
	std::string dictionary= std::string("{'descr': '") + typeDescriptor + "', 'fortran_order': False, 'shape': ("
							+ std::to_string(count) + ",), }";

	// Magic (6), version (2), and header length (2) come before the
	// dictionary, which ends in a newline
	const size_t preamble= 10;
	const size_t unpadded= preamble + dictionary.size() + 1;
	dictionary.append((64 - unpadded % 64) % 64, ' ');
	dictionary.push_back('\n');

	std::vector<char> header= {'\x93', 'N', 'U', 'M', 'P', 'Y', '\x01', '\x00'};
	const uint16_t dictionaryLength= (uint16_t)dictionary.size();
	header.push_back((char)(dictionaryLength & 0xff));
	header.push_back((char)(dictionaryLength >> 8));
	header.insert(header.end(), dictionary.begin(), dictionary.end());
	return header;
}

bool NpyWriter::writeRaw(const std::filesystem::path& path, const char* typeDescriptor, const void* data,
						 size_t elementSize, size_t count)
{
	// Every platform the editor builds for is little-endian, so the values
	// go out as they sit in memory
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		MIKAN_LOG_ERROR("NpyWriter::write") << "Could not open '" << path.string() << "' for writing";
		return false;
	}

	const std::vector<char> header= makeHeader(typeDescriptor, count);
	file.write(header.data(), (std::streamsize)header.size());
	if (count > 0)
		file.write((const char*)data, (std::streamsize)(elementSize * count));
	if (!file)
	{
		MIKAN_LOG_ERROR("NpyWriter::write") << "Failed writing '" << path.string() << "'";
		return false;
	}
	return true;
}
