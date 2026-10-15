#include "CaptureUstarHeader.h"

#include <cstdio>
#include <cstring>

namespace
{
// Field offsets and sizes from the ustar header layout
constexpr size_t k_nameOffset= 0;
constexpr size_t k_modeOffset= 100;
constexpr size_t k_uidOffset= 108;
constexpr size_t k_gidOffset= 116;
constexpr size_t k_sizeOffset= 124;
constexpr size_t k_mtimeOffset= 136;
constexpr size_t k_checksumOffset= 148;
constexpr size_t k_typeFlagOffset= 156;
constexpr size_t k_linkNameOffset= 157;
constexpr size_t k_magicOffset= 257;
constexpr size_t k_versionOffset= 263;
constexpr size_t k_unameOffset= 265;
constexpr size_t k_gnameOffset= 297;
constexpr size_t k_devMajorOffset= 329;
constexpr size_t k_devMinorOffset= 337;
constexpr size_t k_prefixOffset= 345;

constexpr size_t k_sizeFieldSize= 12;
constexpr size_t k_mtimeFieldSize= 12;
constexpr size_t k_checksumFieldSize= 8;
constexpr size_t k_linkNameFieldSize= 100;
constexpr size_t k_prefixFieldSize= 155;

constexpr char k_regularFileTypeFlag= '0';
constexpr char k_legacyRegularFileTypeFlag= '\0';
// "ustar" followed by a NUL, then version "00": the POSIX form, as opposed to
// the GNU "ustar  " form
const char k_posixMagic[6]= {'u', 's', 't', 'a', 'r', '\0'};
const char k_posixVersion[2]= {'0', '0'};

// Writes value as zero-padded octal into fieldSize - 1 digits followed by a NUL
void writeOctal(char* field, size_t fieldSize, uint64_t value)
{
	for (size_t index= fieldSize - 1; index-- > 0;)
	{
		field[index]= (char)('0' + (value & 7));
		value>>= 3;
	}
	field[fieldSize - 1]= '\0';
}

// Reads an octal field terminated by NUL or space, leading spaces allowed.
// Rejects the base-256 encoding (high bit set) and any other character.
bool readOctal(const char* field, size_t fieldSize, uint64_t& outValue)
{
	if (((unsigned char)field[0] & 0x80) != 0)
		return false;

	size_t index= 0;
	while (index < fieldSize && field[index] == ' ')
		++index;

	uint64_t value= 0;
	bool bAnyDigit= false;
	for (; index < fieldSize; ++index)
	{
		const char c= field[index];
		if (c == '\0' || c == ' ')
			break;
		if (c < '0' || c > '7')
			return false;
		value= (value << 3) | (uint64_t)(c - '0');
		bAnyDigit= true;
	}

	if (!bAnyDigit)
		return false;

	outValue= value;
	return true;
}

// The unsigned sum of every header byte with the checksum field read as spaces
uint32_t computeChecksum(const char* block)
{
	uint32_t sum= 0;
	for (size_t index= 0; index < CaptureUstarHeader::k_blockSize; ++index)
	{
		if (index >= k_checksumOffset && index < k_checksumOffset + k_checksumFieldSize)
			sum+= (uint32_t)' ';
		else
			sum+= (uint32_t)(unsigned char)block[index];
	}
	return sum;
}

bool fieldIsEmpty(const char* field, size_t fieldSize)
{
	for (size_t index= 0; index < fieldSize; ++index)
	{
		if (field[index] != '\0')
			return false;
	}
	return true;
}
} // namespace

void CaptureUstarHeader::write(char* block, const std::string& name, uint64_t size, int64_t modificationTimeSeconds)
{
	std::memset(block, 0, k_blockSize);

	std::memcpy(block + k_nameOffset, name.data(), name.size());
	writeOctal(block + k_modeOffset, 8, 0644);
	writeOctal(block + k_uidOffset, 8, 0);
	writeOctal(block + k_gidOffset, 8, 0);
	writeOctal(block + k_sizeOffset, k_sizeFieldSize, size);
	writeOctal(block + k_mtimeOffset, k_mtimeFieldSize,
			   modificationTimeSeconds > 0 ? (uint64_t)modificationTimeSeconds : 0);
	block[k_typeFlagOffset]= k_regularFileTypeFlag;
	std::memcpy(block + k_magicOffset, k_posixMagic, sizeof(k_posixMagic));
	std::memcpy(block + k_versionOffset, k_posixVersion, sizeof(k_posixVersion));
	writeOctal(block + k_devMajorOffset, 8, 0);
	writeOctal(block + k_devMinorOffset, 8, 0);

	// Six octal digits, a NUL, then a space, which is the layout every tar
	// implementation writes
	const uint32_t checksum= computeChecksum(block);
	char checksumField[k_checksumFieldSize];
	std::snprintf(checksumField, sizeof(checksumField), "%06o", checksum);
	checksumField[6]= '\0';
	checksumField[7]= ' ';
	std::memcpy(block + k_checksumOffset, checksumField, k_checksumFieldSize);
}

bool CaptureUstarHeader::parse(const char* block, std::string& outName, uint64_t& outSize, std::string& outError)
{
	if (std::memcmp(block + k_magicOffset, k_posixMagic, sizeof(k_posixMagic)) != 0
		|| std::memcmp(block + k_versionOffset, k_posixVersion, sizeof(k_posixVersion)) != 0)
	{
		outError= "Header is not a POSIX ustar header";
		return false;
	}

	uint64_t storedChecksum= 0;
	if (!readOctal(block + k_checksumOffset, k_checksumFieldSize, storedChecksum)
		|| storedChecksum != computeChecksum(block))
	{
		outError= "Header checksum does not match";
		return false;
	}

	const char typeFlag= block[k_typeFlagOffset];
	if (typeFlag != k_regularFileTypeFlag && typeFlag != k_legacyRegularFileTypeFlag)
	{
		outError= std::string("Header type '") + typeFlag + "' is not a regular file";
		return false;
	}

	if (!fieldIsEmpty(block + k_prefixOffset, k_prefixFieldSize))
	{
		outError= "Header carries a path prefix";
		return false;
	}

	if (!fieldIsEmpty(block + k_linkNameOffset, k_linkNameFieldSize))
	{
		outError= "Header carries a link name";
		return false;
	}

	if (!readOctal(block + k_sizeOffset, k_sizeFieldSize, outSize))
	{
		outError= "Header size is not an octal number";
		return false;
	}

	const char* nameField= block + k_nameOffset;
	size_t nameLength= 0;
	while (nameLength < k_nameFieldSize && nameField[nameLength] != '\0')
		++nameLength;
	outName.assign(nameField, nameLength);

	return true;
}

bool CaptureUstarHeader::isZeroBlock(const char* block) { return fieldIsEmpty(block, k_blockSize); }
