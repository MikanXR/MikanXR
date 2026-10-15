#pragma once

#include "HttpInterprocessMessageServer.h"
#include "IServerRequestHandler.h"

#include <filesystem>
#include <string>

// Receives files POSTed into a project asset folder over the HTTP message server, which is
// how the MikanARStreamer app delivers a capture:
//
//   POST /assets/upload?folder=<folderId>&name=<fileName>   body: the raw file bytes
//
// A streaming route: the body never sits in memory. Once the headers are in, the route
// hops to the main thread through HttpInterprocessMessageServer::runOnMainThread to
// validate the folder and file type and learn the folder directory, and names a hidden
// temporary file there for the server to stream the body into, so the final move into
// place stays a rename on one volume. A request the first hop refuses has its body read
// and dropped. Once the body has landed, a second hop moves it into place and rescans the
// catalog. A file of the same name is replaced, since the same name means the same take
// sent again.
//
// Replies are JSON: {"storedPath": "...", "replaced": bool} on 200, {"error": "..."}
// otherwise, with 400 for a bad request or a refused folder or type, 405 for a verb other
// than POST, 409 when the destination is held open by a reader, 503 when no project is
// loaded, 504 when the main thread did not answer in time.
class AssetUploadRequestHandler : public IServerRequestHandler
{
public:
	AssetUploadRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override;

	// A bare file name: no path separators or drive letters, no parent references,
	// not hidden, no control characters, at most 255 characters
	static bool isSafeUploadFileName(const std::string& fileName);

	static const char* k_uploadRoutePath;

protected:
	// Before the body: validates the request and names the temporary file it streams to
	bool resolveUploadDestination(const HttpRouteRequest& request, std::filesystem::path& outBodyFilePath,
								  HttpRouteResponse& outRefusal);
	// After the body: moves the temporary file into the folder
	HttpRouteResponse handleUploadRequest(const HttpRouteRequest& request);
};
