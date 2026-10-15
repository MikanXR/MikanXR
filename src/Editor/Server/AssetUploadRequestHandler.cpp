#include "AssetUploadRequestHandler.h"
#include "JsonUtils.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanServer.h"
#include "ProjectAssetCatalog.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>

const char* AssetUploadRequestHandler::k_uploadRoutePath= "/assets/upload";

namespace
{
// Generous: it only trips when the main thread is wedged, not on a slow frame
constexpr int k_mainThreadHopTimeoutMs= 10000;

HttpRouteResponse makeErrorResponse(int statusCode, const std::string& error)
{
	HttpRouteResponse response;
	response.statusCode= statusCode;
	response.body= json{{"error", error}}.dump();
	return response;
}

// Hidden and extensionless, so a catalog rescan between the two hops never lists it
std::string makeTempFileName()
{
	static std::atomic<unsigned int> s_counter{0};
	const auto ticks= std::chrono::steady_clock::now().time_since_epoch().count();
	return ".mikan_upload_" + std::to_string(ticks) + "_" + std::to_string(s_counter++);
}

struct ResolveResult
{
	bool bProjectLoaded= false;
	bool bResolved= false;
	std::filesystem::path folderDir;
	std::string error;
};

struct ImportResult
{
	bool bProjectLoaded= false;
	bool bImported= false;
	bool bReplaced= false;
	std::string storedPath;
	std::error_code errorCode;
	std::string error;
};

ProjectAssetCatalog* findAssetCatalog(MikanServer* server)
{
	MainWindow* window= server->getOwnerWindow();
	return window != nullptr ? window->getAssetCatalog() : nullptr;
}

bool hasLoadedProject(MikanServer* server)
{
	ProjectManagerPtr projectManager= server->getProjectManager();
	return projectManager && projectManager->hasLoadedProject();
}
} // namespace

bool AssetUploadRequestHandler::startup(MainWindow* mainWindow)
{
	HttpInterprocessMessageServer* httpServer= m_owner->getHttpMessageServer();

	return httpServer->setStreamingRouteHandler(
		k_uploadRoutePath,
		std::bind(&AssetUploadRequestHandler::resolveUploadDestination, this, std::placeholders::_1,
				  std::placeholders::_2, std::placeholders::_3),
		std::bind(&AssetUploadRequestHandler::handleUploadRequest, this, std::placeholders::_1));
}

void AssetUploadRequestHandler::shutdown()
{
	m_owner->getHttpMessageServer()->removeStreamingRouteHandler(k_uploadRoutePath);
}

bool AssetUploadRequestHandler::isSafeUploadFileName(const std::string& fileName)
{
	if (fileName.empty() || fileName.size() > 255)
		return false;
	if (fileName[0] == '.')
		return false;
	if (fileName.find("..") != std::string::npos)
		return false;

	for (const char c : fileName)
	{
		if (c == '/' || c == '\\' || c == ':' || static_cast<unsigned char>(c) < 0x20)
			return false;
	}

	return true;
}

bool AssetUploadRequestHandler::resolveUploadDestination(const HttpRouteRequest& request,
														 std::filesystem::path& outBodyFilePath,
														 HttpRouteResponse& outRefusal)
{
	if (request.method != "POST")
	{
		outRefusal= makeErrorResponse(405, "Upload with POST");
		return false;
	}

	const auto folderIt= request.queryArgs.find("folder");
	if (folderIt == request.queryArgs.end() || folderIt->second.empty())
	{
		outRefusal= makeErrorResponse(400, "Missing folder query argument");
		return false;
	}

	const auto nameIt= request.queryArgs.find("name");
	if (nameIt == request.queryArgs.end() || nameIt->second.empty())
	{
		outRefusal= makeErrorResponse(400, "Missing name query argument");
		return false;
	}

	const std::string folderId= folderIt->second;
	const std::string fileName= nameIt->second;
	if (!isSafeUploadFileName(fileName))
	{
		outRefusal= makeErrorResponse(400, "name must be a bare file name");
		return false;
	}

	if (request.bodyLength == 0)
	{
		outRefusal= makeErrorResponse(400, "Empty body");
		return false;
	}

	HttpInterprocessMessageServer* httpServer= m_owner->getHttpMessageServer();
	MikanServer* server= m_owner;

	// First hop: the catalog and the project directory are main-thread state
	auto resolved= std::make_shared<ResolveResult>();
	const bool bResolveRan= httpServer->runOnMainThread(
		[server, folderId, fileName, resolved]()
		{
			resolved->bProjectLoaded= hasLoadedProject(server);
			ProjectAssetCatalog* catalog= findAssetCatalog(server);
			if (catalog == nullptr)
			{
				resolved->error= "No asset catalog";
				return;
			}

			resolved->bResolved=
				catalog->resolveUploadDestination(folderId, fileName, resolved->folderDir, resolved->error);
		},
		k_mainThreadHopTimeoutMs);
	if (!bResolveRan)
	{
		outRefusal= makeErrorResponse(504, "Timed out waiting for the editor");
		return false;
	}
	if (!resolved->bResolved)
	{
		outRefusal= makeErrorResponse(resolved->bProjectLoaded ? 400 : 503, resolved->error);
		return false;
	}

	// The body lands beside its destination so the move into place stays on one volume
	std::error_code ec;
	std::filesystem::create_directories(resolved->folderDir, ec);
	outBodyFilePath= resolved->folderDir / makeTempFileName();
	return true;
}

HttpRouteResponse AssetUploadRequestHandler::handleUploadRequest(const HttpRouteRequest& request)
{
	const std::string folderId= request.queryArgs.at("folder");
	const std::string fileName= request.queryArgs.at("name");
	const std::filesystem::path tempPath= request.bodyFilePath;

	HttpInterprocessMessageServer* httpServer= m_owner->getHttpMessageServer();
	MikanServer* server= m_owner;

	// Second hop: move into place and rescan. Safe to run late: it only consumes the temp file.
	auto imported= std::make_shared<ImportResult>();
	const bool bImportRan= httpServer->runOnMainThread(
		[server, folderId, fileName, tempPath, imported]()
		{
			imported->bProjectLoaded= hasLoadedProject(server);
			ProjectAssetCatalog* catalog= findAssetCatalog(server);
			if (catalog == nullptr)
			{
				imported->error= "No asset catalog";
				return;
			}

			imported->bImported=
				catalog->importUploadedAsset(folderId, fileName, tempPath, imported->storedPath, imported->bReplaced,
											 imported->errorCode, imported->error);
			if (!imported->bImported)
			{
				std::error_code ec;
				std::filesystem::remove(tempPath, ec);
			}
		},
		k_mainThreadHopTimeoutMs);
	if (!bImportRan)
	{
		// The temp file stays for the late job to consume
		return makeErrorResponse(504, "Timed out waiting for the editor");
	}
	if (!imported->bImported)
	{
		int statusCode= 400;
		if (!imported->bProjectLoaded)
		{
			statusCode= 503;
		}
		else if (imported->errorCode)
		{
			// MSVC maps a sharing violation, the destination held open by a reader such
			// as a file video source playing the take, to permission_denied
			if (imported->errorCode == std::errc::permission_denied)
			{
				return makeErrorResponse(409, "Destination is in use, stop any source playing it: " + imported->error);
			}
			statusCode= 500;
		}
		return makeErrorResponse(statusCode, imported->error);
	}

	MIKAN_MT_LOG_INFO("AssetUploadRequestHandler::handleUploadRequest")
		<< "Received " << request.bodyLength << " bytes into " << folderId << " as " << imported->storedPath
		<< (imported->bReplaced ? " (replaced)" : "");

	HttpRouteResponse response;
	response.body= json{{"storedPath", imported->storedPath}, {"replaced", imported->bReplaced}}.dump();
	return response;
}
