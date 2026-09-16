#pragma once

#include "HttpInterprocessMessageServer.h"
#include "IServerRequestHandler.h"

#include <string>

// Receives files POSTed into a project asset folder over the HTTP message server, which is
// how the MikanARStreamer app delivers a recorded take and its pose track sidecar:
//
//   POST /assets/upload?folder=<folderId>&name=<fileName>   body: the raw file bytes
//
// The route runs on the connection thread, so the body never crosses the main thread. It
// hops to the main thread twice through HttpInterprocessMessageServer::runOnMainThread:
// once to validate the folder and file type and learn the folder directory, then, after
// writing the body to a temporary file beside its destination, once more to move it into
// place and rescan the catalog. A file of the same name is replaced, since the same name
// means the same take sent again.
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
	HttpRouteResponse handleUploadRequest(const HttpRouteRequest& request);
};
