#include "ModelDownloadTask.h"
#include "HashUtils.h"
#include "HttpDownloader.h"
#include "Logger.h"
#include "ModelRepository.h"

#include <algorithm>
#include <fstream>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244) // conversion from X to Y, possible loss of data
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace
{
// Headroom over the catalog's stated requirement, so an install does not fill
// the volume to the last byte.
constexpr uint64_t k_diskHeadroomBytes= 512ull * 1024ull * 1024ull;
constexpr size_t k_assembleChunkBytes= 8 * 1024 * 1024;

// Documents that travel with a model download and are written beside it, so
// the license is on disk next to the weights rather than only in a web page.
const char* k_licenseSidecarFiles[]= {"LICENSE-MODEL.txt", "NOTICE.md"};

/// A manifest names its files relative to the release; a direct source gives
/// each file its own absolute URL.
std::string resolveFileUrl(const ModelCatalogEntry& entry, const std::string& fileName)
{
	if (entry.hasManifest())
		return entry.releaseBaseUrl + "/" + fileName;

	for (const ModelDirectFile& directFile : entry.directFiles)
	{
		if (directFile.name == fileName)
			return directFile.url;
	}

	return std::string();
}
} // namespace

ModelDownloadTask::~ModelDownloadTask() { cancelAndJoin(); }

void ModelDownloadTask::start(const std::vector<eModelId>& models)
{
	if (m_thread.joinable())
		return;

	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status= Status();
	}
	m_bCancelRequested= false;
	m_bRunning= true;
	m_thread= std::thread(&ModelDownloadTask::runThread, this, models);
}

void ModelDownloadTask::cancelAndJoin()
{
	m_bCancelRequested= true;
	if (m_thread.joinable())
		m_thread.join();
	m_bRunning= false;
}

ModelDownloadTask::Status ModelDownloadTask::getStatus() const
{
	std::lock_guard<std::mutex> lock(m_statusMutex);

	return m_status;
}

void ModelDownloadTask::setPhase(eModelDownloadPhase phase)
{
	std::lock_guard<std::mutex> lock(m_statusMutex);
	m_status.phase= phase;
}

void ModelDownloadTask::fail(const std::string& detail)
{
	MIKAN_LOG_ERROR("ModelDownloadTask") << detail;

	std::lock_guard<std::mutex> lock(m_statusMutex);
	m_status.phase= eModelDownloadPhase::failed;
	m_status.errorDetail= detail;
}

void ModelDownloadTask::runThread(std::vector<eModelId> models)
{
	bool bSucceeded= true;

	for (const eModelId id : models)
	{
		if (isCancelled())
			break;

		const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
		if (entry == nullptr)
		{
			fail("unknown model id");
			bSucceeded= false;
			break;
		}

		if (!installModel(*entry))
		{
			bSucceeded= false;
			break;
		}
	}

	std::lock_guard<std::mutex> lock(m_statusMutex);
	m_status.bCancelled= isCancelled();
	m_status.bSucceeded= bSucceeded && !m_status.bCancelled;
	m_status.bFinished= true;
	if (m_status.bSucceeded)
		m_status.phase= eModelDownloadPhase::complete;
	else if (m_status.phase != eModelDownloadPhase::failed)
		m_status.phase= eModelDownloadPhase::idle;
	m_bRunning= false;
}

bool ModelDownloadTask::installModel(const ModelCatalogEntry& entry)
{
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.currentModel= entry.id;
	}

	const std::filesystem::path directory= ModelRepository::getDownloadDirectory(entry.id);
	std::error_code ec;
	std::filesystem::create_directories(directory, ec);
	if (ec)
	{
		fail("could not create " + directory.string() + ": " + ec.message());
		return false;
	}

	setPhase(eModelDownloadPhase::checkingDiskSpace);
	const uint64_t freeBytes= HttpDownloader::getFreeDiskSpace(directory);
	if (freeBytes != 0 && freeBytes < entry.requiredDiskBytes + k_diskHeadroomBytes)
	{
		fail("not enough free space on " + directory.root_name().string() + ": " + std::to_string(freeBytes / 1000000)
			 + " MB free, " + std::to_string((entry.requiredDiskBytes + k_diskHeadroomBytes) / 1000000) + " MB needed");
		return false;
	}

	std::vector<PlannedFile> plannedFiles;
	std::vector<std::string> sidecarFiles;
	if (entry.hasManifest())
	{
		setPhase(eModelDownloadPhase::fetchingManifest);
		if (!planFromManifest(entry, plannedFiles, sidecarFiles))
			return false;
	}
	else if (!planFromCatalog(entry, plannedFiles))
	{
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		for (const PlannedFile& file : plannedFiles)
			m_status.overallTotalBytes+= file.sizeBytes;
	}

	for (const PlannedFile& file : plannedFiles)
	{
		if (isCancelled())
			return false;

		const std::filesystem::path destPath= directory / file.name;

		// Already installed and intact: this is what makes a cancelled or
		// failed run resumable rather than a total loss.
		if (std::filesystem::exists(destPath, ec) && std::filesystem::file_size(destPath, ec) == file.sizeBytes)
		{
			setPhase(eModelDownloadPhase::verifying);
			{
				std::lock_guard<std::mutex> lock(m_statusMutex);
				m_status.currentFile= file.name;
			}

			if (HashUtils::verifyFileSha256(destPath, file.sha256, &m_bCancelRequested))
			{
				MIKAN_LOG_INFO("ModelDownloadTask") << "Keeping existing " << file.name;

				std::lock_guard<std::mutex> lock(m_statusMutex);
				m_status.overallReceivedBytes+= file.sizeBytes;
				continue;
			}

			if (isCancelled())
				return false;

			MIKAN_LOG_WARNING("ModelDownloadTask") << file.name << " on disk does not verify, fetching it again";
		}

		setPhase(eModelDownloadPhase::downloading);

		if (file.parts.empty())
		{
			if (!fetchOne(resolveFileUrl(entry, file.name), destPath, file))
				return false;
		}
		else
		{
			for (const PlannedFile& part : file.parts)
			{
				if (isCancelled())
					return false;

				const std::filesystem::path partPath= directory / part.name;
				if (std::filesystem::exists(partPath, ec) && std::filesystem::file_size(partPath, ec) == part.sizeBytes
					&& HashUtils::verifyFileSha256(partPath, part.sha256, &m_bCancelRequested))
				{
					std::lock_guard<std::mutex> lock(m_statusMutex);
					m_status.overallReceivedBytes+= part.sizeBytes;
					continue;
				}

				if (!fetchOne(entry.releaseBaseUrl + "/" + part.name, partPath, part))
					return false;
			}

			if (!assembleParts(file, directory))
				return false;
		}
	}

	// The license and notice ride along so they sit beside the weights rather
	// than only in a web page. Their absence is logged rather than fatal: a
	// model that downloaded fine should not be thrown away over a text file.
	for (const std::string& sidecar : sidecarFiles)
	{
		if (isCancelled())
			return false;

		PlannedFile sidecarFile;
		sidecarFile.name= sidecar;
		fetchOne(resolveFileUrl(entry, sidecar), directory / sidecar, sidecarFile, false);
	}

	const std::vector<std::string> missing= ModelRepository::getMissingFiles(entry.id, directory);
	if (!missing.empty())
	{
		std::string detail= "install finished but these are missing from " + directory.string() + ":";
		for (const std::string& name : missing)
			detail+= " " + name;
		fail(detail);
		return false;
	}

	MIKAN_LOG_INFO("ModelDownloadTask") << "Installed " << entry.name << " into " << directory.string();

	return true;
}

bool ModelDownloadTask::planFromCatalog(const ModelCatalogEntry& entry, std::vector<PlannedFile>& outFiles)
{
	for (const ModelDirectFile& directFile : entry.directFiles)
	{
		PlannedFile file;
		file.name= directFile.name;
		file.sizeBytes= directFile.sizeBytes;
		file.sha256= directFile.sha256;
		outFiles.push_back(file);
	}

	if (outFiles.empty())
	{
		fail("catalog entry " + entry.name + " lists no files to download");
		return false;
	}

	return true;
}

bool ModelDownloadTask::planFromManifest(const ModelCatalogEntry& entry, std::vector<PlannedFile>& outFiles,
										 std::vector<std::string>& outSidecarFiles)
{
	const std::string manifestUrl= entry.releaseBaseUrl + "/manifest.json";

	std::string body;
	const HttpDownloader::Result result= HttpDownloader::downloadToString(manifestUrl, body, &m_bCancelRequested);
	if (!result.bSuccess)
	{
		if (result.bCancelled)
			return false;

		fail("could not fetch " + manifestUrl + ": " + result.errorMessage);
		return false;
	}

	try
	{
		const nlohmann::json manifest= nlohmann::json::parse(body);
		for (const auto& fileJson : manifest.at("files"))
		{
			PlannedFile file;
			file.name= fileJson.at("name").get<std::string>();
			file.sizeBytes= fileJson.at("size").get<uint64_t>();
			file.sha256= fileJson.value("sha256", std::string());

			if (fileJson.contains("parts"))
			{
				for (const auto& partJson : fileJson.at("parts"))
				{
					PlannedFile part;
					part.name= partJson.at("name").get<std::string>();
					part.sizeBytes= partJson.at("size").get<uint64_t>();
					part.sha256= partJson.value("sha256", std::string());
					file.parts.push_back(part);
				}
			}

			outFiles.push_back(file);
		}

		// The well known documents, plus whatever the manifest calls its
		// license, with no name fetched twice.
		for (const char* sidecar : k_licenseSidecarFiles)
			outSidecarFiles.push_back(sidecar);

		if (manifest.contains("license"))
		{
			const auto& licenseJson= manifest.at("license");
			for (const char* field : {"file", "notice"})
			{
				if (!licenseJson.contains(field))
					continue;

				const std::string name= licenseJson.at(field).get<std::string>();
				if (std::find(outSidecarFiles.begin(), outSidecarFiles.end(), name) == outSidecarFiles.end())
					outSidecarFiles.push_back(name);
			}
		}
	}
	catch (const std::exception& e)
	{
		fail(std::string("could not parse ") + manifestUrl + ": " + e.what());
		return false;
	}

	if (outFiles.empty())
	{
		fail(manifestUrl + " lists no files");
		return false;
	}

	return true;
}

bool ModelDownloadTask::fetchOne(const std::string& url, const std::filesystem::path& destPath, const PlannedFile& file,
								 bool bRequired)
{
	// An optional file must not leave the status in a failed phase, so its
	// problems are reported to the log and nowhere else.
	const auto report= [this, bRequired](const std::string& detail)
	{
		if (bRequired)
			fail(detail);
		else
			MIKAN_LOG_WARNING("ModelDownloadTask") << detail;
	};

	if (url.empty())
	{
		report("no download URL for " + file.name);
		return false;
	}

	uint64_t alreadyCounted= 0;
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.currentFile= file.name;
		m_status.currentFileReceivedBytes= 0;
		m_status.currentFileTotalBytes= file.sizeBytes;
	}

	const auto onProgress= [this, &alreadyCounted](uint64_t received, uint64_t total) -> bool
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.currentFileReceivedBytes= received;
		if (total != 0)
			m_status.currentFileTotalBytes= total;
		// The overall counter tracks the delta so a retry does not double count.
		m_status.overallReceivedBytes+= received - alreadyCounted;
		alreadyCounted= received;

		return !m_bCancelRequested.load();
	};

	MIKAN_LOG_INFO("ModelDownloadTask") << "Fetching " << url;

	const HttpDownloader::Result result= HttpDownloader::downloadToFile(url, destPath, onProgress, &m_bCancelRequested);
	if (!result.bSuccess)
	{
		if (!result.bCancelled)
			report("failed to download " + file.name + ": " + result.errorMessage);

		return false;
	}

	if (file.sizeBytes != 0 && result.receivedBytes != file.sizeBytes)
	{
		report(file.name + " is " + std::to_string(result.receivedBytes) + " bytes, expected "
			   + std::to_string(file.sizeBytes));
		return false;
	}

	if (!file.sha256.empty())
	{
		setPhase(eModelDownloadPhase::verifying);
		if (!HashUtils::verifyFileSha256(destPath, file.sha256, &m_bCancelRequested))
		{
			if (isCancelled())
				return false;

			std::error_code ec;
			std::filesystem::remove(destPath, ec);
			report(file.name + " does not match the expected SHA256");
			return false;
		}
		setPhase(eModelDownloadPhase::downloading);
	}

	return true;
}

bool ModelDownloadTask::assembleParts(const PlannedFile& file, const std::filesystem::path& directory)
{
	setPhase(eModelDownloadPhase::assembling);
	{
		std::lock_guard<std::mutex> lock(m_statusMutex);
		m_status.currentFile= file.name;
	}

	// Concatenate under a .part name and rename, the same rule the downloader
	// follows, so an interrupted assembly is never mistaken for the real file.
	std::filesystem::path partialPath= directory / file.name;
	partialPath+= ".part";

	{
		std::ofstream out(partialPath, std::ios::binary | std::ios::trunc);
		if (!out)
		{
			fail("could not open " + partialPath.string() + " for writing");
			return false;
		}

		std::vector<char> buffer(k_assembleChunkBytes);
		for (const PlannedFile& part : file.parts)
		{
			std::ifstream in(directory / part.name, std::ios::binary);
			if (!in)
			{
				fail("missing part " + part.name);
				return false;
			}

			while (in)
			{
				if (isCancelled())
					return false;

				in.read(buffer.data(), (std::streamsize)buffer.size());
				const std::streamsize readCount= in.gcount();
				if (readCount <= 0)
					break;

				out.write(buffer.data(), readCount);
				if (!out)
				{
					fail("write failed while assembling " + file.name);
					return false;
				}
			}
		}
	}

	std::error_code ec;
	const std::filesystem::path destPath= directory / file.name;
	std::filesystem::remove(destPath, ec);
	std::filesystem::rename(partialPath, destPath, ec);
	if (ec)
	{
		fail("could not move the assembled " + file.name + " into place: " + ec.message());
		return false;
	}

	if (!file.sha256.empty())
	{
		setPhase(eModelDownloadPhase::verifying);
		if (!HashUtils::verifyFileSha256(destPath, file.sha256, &m_bCancelRequested))
		{
			if (isCancelled())
				return false;

			std::filesystem::remove(destPath, ec);
			fail("the assembled " + file.name + " does not match the expected SHA256");
			return false;
		}
	}

	// The parts are several gigabytes of duplicate data once the whole file
	// exists, so they go as soon as it verifies.
	for (const PlannedFile& part : file.parts)
		std::filesystem::remove(directory / part.name, ec);

	return true;
}
