#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanClientTypes.h"
#include "MikanResponseFuture.h"

#include <memory>

using IMikanAPIPtr= std::shared_ptr<class IMikanAPI>;
using IMikanAPIWeakPtr= std::weak_ptr<class IMikanAPI>;

// -- Mikan API -----
// Strings cross this interface as const char* only, never std::string: a client built against
// another CRT (a debug build over the release SDK) lays std::string out differently.
class MIKAN_API IMikanAPI
{
public:
	IMikanAPI()= default;
	virtual ~IMikanAPI()= default;

	// API Lifecycle
	static IMikanAPIPtr createMikanAPI();
	virtual MikanAPIResult init(const char* client_name, MikanLogLevel min_log_level, MikanLogCallback log_callback)= 0;
	virtual bool getIsInitialized()= 0;
	virtual MikanAPIResult shutdown()= 0;

	// Client Info
	virtual int getClientAPIVersion() const= 0;
	// Owned by the API, valid until shutdown.
	virtual const char* getClientName() const= 0;
	virtual MikanClientInfo allocateClientInfo() const= 0;
	virtual MikanAPIResult setGraphicsDeviceInterface(MikanClientGraphicsApi api, void* graphicsDeviceInterface)= 0;
	virtual MikanAPIResult getGraphicsDeviceInterface(MikanClientGraphicsApi api, void** outGraphicsDeviceInterface)= 0;
	virtual MikanAPIResult setGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															void* graphicsCommandQueueInterface)= 0;
	virtual MikanAPIResult getGraphicsCommandQueueInterface(MikanClientGraphicsApi api,
															void** outGraphicsCommandQueueInterface)= 0;
	virtual MikanAPIResult getCameraPackDepthTextureResourcePtr(MikanCameraID cameraId, void** outResourcePtr)= 0;

	// Connection Management
	virtual MikanAPIResult connect()= 0;
	// A null host or port is the default.
	virtual MikanAPIResult connect(const char* host, const char* port)= 0;
	virtual bool getIsConnected()= 0;
	virtual MikanAPIResult disconnect()= 0;
	virtual MikanAPIResult disconnect(uint16_t code, const char* reason)= 0;
	// "http://<host>:<port>" of the editor's HTTP server on the host this client connected to, which a
	// component's geometry_url is joined to. Empty until the connected event has arrived. Owned by the
	// API, valid until the next connect() or fetchNextEvent() call.
	virtual const char* getHttpBaseUrl() const= 0;

	// Messaging
	virtual MikanResponseFuture sendRequest(MikanRequest& request)= 0;
	virtual MikanAPIResult cancelRequest(const MikanRequestID& requestId)= 0;
	virtual MikanAPIResult fetchNextEvent(MikanEventPtr& out_event)= 0;
};
