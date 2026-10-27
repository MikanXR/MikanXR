#pragma once

#include "MikanCoreTypes.h"
#include "MikanAPITypes.h"
#include "MikanTypeFwd.h"
#include "MikanResponseFuture.h"
#include "JsonSerializer.h"
#include "ReflectionHandles.h"

#include <future>
#include <mutex>
#include <string>

struct MikanRequest;
typedef void* MikanContext;

class MikanRequestManager
{
public:
	MikanRequestManager()= default;

	MikanAPIResult init(MikanContext context);
	MikanContext getContext() const { return m_context; }

	MikanResponseFuture sendRequest(MikanRequest& request);
	// expectedResponseType is the struct a result is read into, the plain MikanResponse when null
	MikanResponseFuture addResponseHandler(MikanRequestID requestId, MikanAPIResult result,
										   Serialization::StructTypeHandle expectedResponseType= nullptr);
	MikanAPIResult cancelRequest(MikanRequestID requestId);

protected:
	static void textResponseHandlerStatic(MikanRequestID requestId, const char* utf8ResponseString, void* userdata);
	void textResponseHander(MikanRequestID requestId, const char* utf8ResponseString);
	MikanResponsePtr parseResponseString(MikanRequestID requestId, const char* utf8ResponseString,
										 Serialization::StructTypeHandle expectedResponseType);

private:
	struct PendingRequest
	{
		MikanRequestID id;
		MikanResponsePromise promise;
		// A result carries no type name, so the request records what answers it
		Serialization::StructTypeHandle expectedResponseType= nullptr;
	};
	using PendingRequestPtr= std::shared_ptr<PendingRequest>;
	void insertPendingRequest(MikanRequestManager::PendingRequestPtr pendingRequest);
	PendingRequestPtr removePendingRequest(MikanRequestID requestId);

	MikanContext m_context= nullptr;
	std::map<MikanRequestID, PendingRequestPtr> m_pendingRequests;
	std::mutex m_pending_request_map_mutex;
	MikanRequestID m_nextRequestID= 0;
};
