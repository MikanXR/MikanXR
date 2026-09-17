// -- includes -----
#include "HashUtils.h"

#include <algorithm>
#include <fstream>
#include <vector>

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
// bcrypt.h has to follow windows.h
#include <bcrypt.h>
#endif

namespace
{
// Big enough that a multi-gigabyte file is not read in tiny pieces, small
// enough to stay off the stack and out of the way.
constexpr size_t k_hashReadChunkBytes= 1024 * 1024;

std::string toLowerHex(const unsigned char* bytes, size_t byteCount)
{
	static const char* k_hexDigits= "0123456789abcdef";

	std::string hex;
	hex.reserve(byteCount * 2);
	for (size_t i= 0; i < byteCount; ++i)
	{
		hex.push_back(k_hexDigits[bytes[i] >> 4]);
		hex.push_back(k_hexDigits[bytes[i] & 0x0F]);
	}

	return hex;
}
} // namespace

namespace HashUtils
{
#if defined WIN32 || defined _WIN32 || defined WINCE
std::string sha256File(const std::filesystem::path& filePath, const std::atomic<bool>* cancelFlag)
{
	std::ifstream file(filePath, std::ios::binary);
	if (!file)
		return std::string();

	BCRYPT_ALG_HANDLE algorithm= nullptr;
	if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
		return std::string();

	std::string result;
	DWORD objectSize= 0;
	DWORD digestSize= 0;
	DWORD bytesWritten= 0;
	if (BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objectSize, sizeof(objectSize),
										 &bytesWritten, 0))
		&& BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, (PUCHAR)&digestSize, sizeof(digestSize),
											&bytesWritten, 0)))
	{
		std::vector<unsigned char> hashObject(objectSize);
		BCRYPT_HASH_HANDLE hash= nullptr;
		if (BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0)))
		{
			std::vector<char> buffer(k_hashReadChunkBytes);
			bool bFailed= false;
			bool bCancelled= false;

			while (file)
			{
				if (cancelFlag != nullptr && cancelFlag->load())
				{
					bCancelled= true;
					break;
				}

				file.read(buffer.data(), (std::streamsize)buffer.size());
				const std::streamsize readCount= file.gcount();
				if (readCount <= 0)
					break;

				if (!BCRYPT_SUCCESS(BCryptHashData(hash, (PUCHAR)buffer.data(), (ULONG)readCount, 0)))
				{
					bFailed= true;
					break;
				}
			}

			if (!bFailed && !bCancelled)
			{
				std::vector<unsigned char> digest(digestSize);
				if (BCRYPT_SUCCESS(BCryptFinishHash(hash, digest.data(), digestSize, 0)))
					result= toLowerHex(digest.data(), digest.size());
			}

			BCryptDestroyHash(hash);
		}
	}

	BCryptCloseAlgorithmProvider(algorithm, 0);

	return result;
}
#else
std::string sha256File(const std::filesystem::path& filePath, const std::atomic<bool>* cancelFlag)
{
	(void)filePath;
	(void)cancelFlag;

	return std::string();
}
#endif // WIN32

bool verifyFileSha256(const std::filesystem::path& filePath, const std::string& expectedSha256,
					  const std::atomic<bool>* cancelFlag)
{
	// A source that publishes no hash still has its size checked by the caller;
	// treat the absent expectation as nothing to compare rather than a failure.
	if (expectedSha256.empty())
		return true;

	const std::string actual= sha256File(filePath, cancelFlag);
	if (actual.empty())
		return false;

	std::string expected= expectedSha256;
	std::transform(expected.begin(), expected.end(), expected.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });

	return actual == expected;
}
}; // namespace HashUtils
