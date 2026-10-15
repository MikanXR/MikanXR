#pragma once

#include "HttpInterprocessMessageServer.h"
#include "IServerRequestHandler.h"

#include "ARKitMarkerAutoAligner.h"

#include <functional>
#include <map>
#include <memory>

// What a port resolves to for the alignment route
enum class eARKitAlignmentQueryResult : int
{
	found,
	// No ARKit source uses the port
	unknownPort,
	// The editor did not answer in time
	unavailable,
};

// Answers the phone's stream prompt over the HTTP message server, the first GET
// route the editor serves:
//
//   GET /arkit/alignment?port=<base_port>
//
// replies {"marker_visible": bool, "aligned": bool} for the ARKitVideoSourceComponent
// streaming to that base port, or 404 when none does. aligned is true while the
// source holds a pose offset, which it drops itself when the phone restarts its
// ARKit session. A poll also starts, or keeps alive, an ARKitMarkerAutoAligner on
// an unaligned source, so the prompt clears once the marker has been in view for
// a few seconds with nobody at the editor.
//
// The route runs on the connection thread and hops to the main thread once to
// read the source. The reply is built by buildResponse so the tests can drive the
// route through a server with a stub query.
class ARKitAlignmentRequestHandler : public IServerRequestHandler
{
public:
	ARKitAlignmentRequestHandler(class MikanServer* owner)
		: IServerRequestHandler(owner)
	{
	}
	virtual ~ARKitAlignmentRequestHandler();

	virtual bool startup(class MainWindow* mainWindow) override;
	virtual void shutdown() override;
	// Ticks the aligners, on the main thread
	void update();

	static const char* k_routePath;

	using AlignmentQuery=
		std::function<eARKitAlignmentQueryResult(int basePort, bool& outMarkerVisible, bool& outAligned)>;
	// The reply for one request: 405 for a verb other than GET, 400 for a
	// missing or malformed port, 404 for an unknown port, 504 when the editor
	// did not answer, otherwise the JSON status
	static HttpRouteResponse buildResponse(const HttpRouteRequest& request, const AlignmentQuery& query);
	// Registers the route on a server with the given query
	static bool registerRoute(HttpInterprocessMessageServer* server, const AlignmentQuery& query);

private:
	eARKitAlignmentQueryResult queryOnMainThread(int basePort, bool& outMarkerVisible, bool& outAligned);
	// Runs on the main thread: finds the source, records the poll, and reads its status
	eARKitAlignmentQueryResult queryNow(int basePort, bool& outMarkerVisible, bool& outAligned);
	static double nowSeconds();

	std::map<MikanVideoSourceID, std::unique_ptr<ARKitMarkerAutoAligner>> m_aligners;
};
