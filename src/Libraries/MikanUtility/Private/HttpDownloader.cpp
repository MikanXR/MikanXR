// -- includes -----
#include "HttpDownloader.h"

#include <fstream>
#include <vector>

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
// winhttp.h has to follow windows.h
#include <winhttp.h>
#endif

namespace
{
constexpr int k_connectTimeoutMs= 15000;
constexpr int k_sendTimeoutMs= 15000;
// A large asset arrives over many reads; this is the gap allowed between two
// of them, not a budget for the whole transfer.
constexpr int k_receiveTimeoutMs= 30000;

#if defined WIN32 || defined _WIN32 || defined WINCE
struct UrlParts
{
	std::wstring host;
	std::wstring path;
	bool bValid= false;
};

// URLs are ASCII by construction (anything else is percent-encoded), so a
// widening copy is the whole conversion.
std::wstring widenAscii(const std::string& text) { return std::wstring(text.begin(), text.end()); }

UrlParts parseHttpsUrl(const std::string& url, std::string& outError)
{
	UrlParts parts;

	const std::string httpsPrefix= "https://";
	if (url.compare(0, httpsPrefix.size(), httpsPrefix) != 0)
	{
		outError= "only https URLs are supported: " + url;
		return parts;
	}

	const std::string remainder= url.substr(httpsPrefix.size());
	const size_t slashPos= remainder.find('/');
	const std::string host= (slashPos != std::string::npos) ? remainder.substr(0, slashPos) : remainder;
	const std::string path= (slashPos != std::string::npos) ? remainder.substr(slashPos) : "/";
	if (host.empty())
	{
		outError= "no host in URL: " + url;
		return parts;
	}

	parts.host= widenAscii(host);
	parts.path= widenAscii(path);
	parts.bValid= true;

	return parts;
}

/// Closes a WinHTTP handle when it goes out of scope, so the several early
/// exits below cannot leak one.
class ScopedInternetHandle
{
public:
	explicit ScopedInternetHandle(HINTERNET handle= nullptr)
		: m_handle(handle)
	{
	}
	~ScopedInternetHandle()
	{
		if (m_handle != nullptr)
			WinHttpCloseHandle(m_handle);
	}
	ScopedInternetHandle(const ScopedInternetHandle&)= delete;
	ScopedInternetHandle& operator=(const ScopedInternetHandle&)= delete;

	HINTERNET get() const { return m_handle; }
	explicit operator bool() const { return m_handle != nullptr; }

private:
	HINTERNET m_handle= nullptr;
};

/// One GET attempt, handing each received block to the sink. The sink returns
/// false to abandon the transfer, which is how both cancellation and a write
/// failure stop it.
using BlockSink= std::function<bool(const char* data, size_t length)>;

HttpDownloader::Result performGet(const std::string& url, const BlockSink& sink,
								  const HttpDownloader::ProgressCallback& progressCallback,
								  const std::atomic<bool>* cancelFlag)
{
	HttpDownloader::Result result;

	const auto bIsCancelled= [cancelFlag]() { return cancelFlag != nullptr && cancelFlag->load(); };

	std::string parseError;
	const UrlParts parts= parseHttpsUrl(url, parseError);
	if (!parts.bValid)
	{
		result.errorMessage= parseError;
		return result;
	}

	ScopedInternetHandle session(WinHttpOpen(L"MikanXR/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
											 WINHTTP_NO_PROXY_BYPASS, 0));
	if (!session)
	{
		result.errorMessage= "WinHttpOpen failed (" + std::to_string(GetLastError()) + ")";
		return result;
	}
	WinHttpSetTimeouts(session.get(), k_connectTimeoutMs, k_connectTimeoutMs, k_sendTimeoutMs, k_receiveTimeoutMs);

	ScopedInternetHandle connection(WinHttpConnect(session.get(), parts.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
	if (!connection)
	{
		result.errorMessage= "WinHttpConnect failed (" + std::to_string(GetLastError()) + ")";
		return result;
	}

	ScopedInternetHandle request(WinHttpOpenRequest(connection.get(), L"GET", parts.path.c_str(), nullptr,
													WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
													WINHTTP_FLAG_SECURE));
	if (!request)
	{
		result.errorMessage= "WinHttpOpenRequest failed (" + std::to_string(GetLastError()) + ")";
		return result;
	}

	if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0)
		|| !WinHttpReceiveResponse(request.get(), nullptr))
	{
		result.errorMessage= "request failed (" + std::to_string(GetLastError()) + ")";
		return result;
	}

	DWORD statusCode= 0;
	DWORD statusCodeSize= sizeof(statusCode);
	WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
						WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
	result.httpStatus= (int)statusCode;
	if (statusCode != 200)
	{
		result.errorMessage= "HTTP " + std::to_string(statusCode);
		return result;
	}

	// Absent on a chunked response, which leaves the progress callback with a
	// total of 0 and the caller showing an indeterminate transfer.
	uint64_t totalBytes= 0;
	{
		DWORD contentLength= 0;
		DWORD contentLengthSize= sizeof(contentLength);
		if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
								WINHTTP_HEADER_NAME_BY_INDEX, &contentLength, &contentLengthSize,
								WINHTTP_NO_HEADER_INDEX))
		{
			totalBytes= contentLength;
		}
	}

	std::vector<char> buffer;
	for (;;)
	{
		if (bIsCancelled())
		{
			result.bCancelled= true;
			return result;
		}

		DWORD bytesAvailable= 0;
		if (!WinHttpQueryDataAvailable(request.get(), &bytesAvailable))
		{
			result.errorMessage= "read stalled (" + std::to_string(GetLastError()) + ")";
			return result;
		}
		if (bytesAvailable == 0)
			break;

		if (buffer.size() < bytesAvailable)
			buffer.resize(bytesAvailable);

		DWORD bytesRead= 0;
		if (!WinHttpReadData(request.get(), buffer.data(), bytesAvailable, &bytesRead))
		{
			result.errorMessage= "read failed (" + std::to_string(GetLastError()) + ")";
			return result;
		}
		if (bytesRead == 0)
			break;

		if (!sink(buffer.data(), (size_t)bytesRead))
		{
			result.errorMessage= "could not write the received data";
			return result;
		}

		result.receivedBytes+= bytesRead;

		if (progressCallback && !progressCallback(result.receivedBytes, totalBytes))
		{
			result.bCancelled= true;
			return result;
		}
	}

	// A transfer that ends early looks like success to the read loop, so the
	// promised length is the only thing that catches a truncated body.
	if (totalBytes != 0 && result.receivedBytes != totalBytes)
	{
		result.errorMessage=
			"received " + std::to_string(result.receivedBytes) + " of " + std::to_string(totalBytes) + " bytes";
		return result;
	}

	result.bSuccess= true;
	return result;
}
#endif // WIN32
} // namespace

namespace HttpDownloader
{
#if defined WIN32 || defined _WIN32 || defined WINCE
Result downloadToFile(const std::string& url, const std::filesystem::path& destPath,
					  const ProgressCallback& progressCallback, const std::atomic<bool>* cancelFlag, int retryCount)
{
	Result result;

	std::error_code ec;
	std::filesystem::create_directories(destPath.parent_path(), ec);

	// The partial file carries a different name until the transfer completes,
	// so an interrupted download is never mistaken for an installed model.
	std::filesystem::path partialPath= destPath;
	partialPath+= ".part";

	for (int attempt= 0; attempt <= retryCount; ++attempt)
	{
		std::ofstream out(partialPath, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			result.errorMessage= "could not open " + partialPath.string() + " for writing";
			return result;
		}

		const auto sink= [&out](const char* data, size_t length) -> bool
		{
			out.write(data, (std::streamsize)length);
			return (bool)out;
		};

		result= performGet(url, sink, progressCallback, cancelFlag);
		out.close();

		if (result.bSuccess)
		{
			std::filesystem::remove(destPath, ec);
			std::filesystem::rename(partialPath, destPath, ec);
			if (ec)
			{
				result.bSuccess= false;
				result.errorMessage= "could not move " + partialPath.string() + " into place: " + ec.message();
			}
			return result;
		}

		std::filesystem::remove(partialPath, ec);

		// A cancel is a decision, not a failure to retry past.
		if (result.bCancelled)
			return result;
	}

	return result;
}

Result downloadToString(const std::string& url, std::string& outBody, const std::atomic<bool>* cancelFlag,
						int retryCount)
{
	Result result;

	for (int attempt= 0; attempt <= retryCount; ++attempt)
	{
		outBody.clear();

		const auto sink= [&outBody](const char* data, size_t length) -> bool
		{
			outBody.append(data, length);
			return true;
		};

		result= performGet(url, sink, {}, cancelFlag);
		if (result.bSuccess || result.bCancelled)
			return result;
	}

	return result;
}

uint64_t getFreeDiskSpace(const std::filesystem::path& directory)
{
	// The target directory usually does not exist yet, so ask about the
	// nearest ancestor that does.
	std::filesystem::path probe= directory;
	std::error_code ec;
	while (!probe.empty() && !std::filesystem::exists(probe, ec))
	{
		const std::filesystem::path parent= probe.parent_path();
		if (parent == probe)
			break;
		probe= parent;
	}

	ULARGE_INTEGER freeBytesAvailable= {};
	if (!GetDiskFreeSpaceExW(probe.wstring().c_str(), &freeBytesAvailable, nullptr, nullptr))
		return 0;

	return (uint64_t)freeBytesAvailable.QuadPart;
}
#else
Result downloadToFile(const std::string& url, const std::filesystem::path& destPath,
					  const ProgressCallback& progressCallback, const std::atomic<bool>* cancelFlag, int retryCount)
{
	(void)url;
	(void)destPath;
	(void)progressCallback;
	(void)cancelFlag;
	(void)retryCount;

	Result result;
	result.errorMessage= "HTTP downloads are only implemented on Windows";
	return result;
}

Result downloadToString(const std::string& url, std::string& outBody, const std::atomic<bool>* cancelFlag,
						int retryCount)
{
	(void)url;
	(void)cancelFlag;
	(void)retryCount;
	outBody.clear();

	Result result;
	result.errorMessage= "HTTP downloads are only implemented on Windows";
	return result;
}

uint64_t getFreeDiskSpace(const std::filesystem::path& directory)
{
	std::error_code ec;
	const auto info= std::filesystem::space(directory, ec);
	return ec ? 0 : (uint64_t)info.available;
}
#endif // WIN32
}; // namespace HttpDownloader
