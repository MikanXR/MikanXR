#include "MikanRequestManager.h"
#include "MikanAPITypes.h"
#include "MikanCoreCAPI.h"
#include "MikanCoreTypes.h"
#include "Logger.h"
#include "JsonDeserializer.h"
#include "SerializableObjectPtr.h"
#include "TypeRegistry.h"
#include "JsonRpc.h"
#include "ReflectionHandles.h"

#include <nlohmann/json.hpp>

#include "assert.h"

using json= nlohmann::json;

MikanAPIResult MikanRequestManager::init(MikanContext context)
{
	m_context= context;

	return (MikanAPIResult)Mikan_SetTextResponseCallback(context, textResponseHandlerStatic, this);
}

namespace
{
// The struct a request is answered with: the one its ResponseType property names, or the
// plain MikanResponse when it carries none
Serialization::StructTypeHandle getExpectedResponseType(Serialization::StructTypeHandle requestStruct)
{
	const char* responseTypeName= Serialization::getResponseTypeName(requestStruct);
	if (responseTypeName == nullptr)
	{
		return &MikanResponse::staticGetArchetype();
	}

	Serialization::StructTypeHandle responseStruct= Serialization::TypeRegistry::getStructByName(responseTypeName);
	if (responseStruct == nullptr)
	{
		MIKAN_MT_LOG_ERROR("MikanRequestManager::getExpectedResponseType()")
			<< requestStruct->getName() << " names an unknown response type " << responseTypeName;
		return &MikanResponse::staticGetArchetype();
	}

	return responseStruct;
}

// An error's data carries the MikanAPIResult name, which is what fills in the response
MikanAPIResult getErrorResultCode(const JsonRpcError& error)
{
	if (error.data.is_object())
	{
		auto resultCodeIt= error.data.find("resultCode");
		if (resultCodeIt != error.data.end() && resultCodeIt->is_string())
		{
			const std::string resultCodeName= resultCodeIt->get<std::string>();

			int64_t resultCodeValue= 0;
			if (Serialization::findEnumValueByName(Serialization::TypeRegistry::getEnumByName("MikanAPIResult"),
												   resultCodeName.c_str(), resultCodeValue))
			{
				return (MikanAPIResult)resultCodeValue;
			}
		}
	}

	MIKAN_MT_LOG_WARNING("MikanRequestManager::getErrorResultCode()")
		<< "Error " << error.code << " (" << error.message << ") carries no known resultCode";
	return MikanAPIResult::GeneralError;
}
} // namespace

MikanResponseFuture MikanRequestManager::sendRequest(MikanRequest& inRequest)
{
	Serialization::StructTypeHandle requestStruct=
		Serialization::TypeRegistry::getStructByName(inRequest.requestTypeName.getUtf8Value());
	assert(requestStruct != nullptr);

	// Stamp the request with the next available request ID
	inRequest.requestId= m_nextRequestID;
	m_nextRequestID++;

	Serialization::StructTypeHandle expectedResponseType= getExpectedResponseType(requestStruct);

	// The request struct's own fields are the params, since the envelope fields are not reflected
	json params;
	std::string serializeError;
	if (Serialization::serializeToJson(&inRequest, requestStruct, params, serializeError))
	{
		if (params.is_null())
		{
			params= json::object();
		}

		const std::string requestFrame=
			JsonRpc::formatRequest(inRequest.requestTypeName.getUtf8Value(), params, inRequest.requestId);
		MikanAPIResult result= (MikanAPIResult)Mikan_SendRequestJSON(m_context, requestFrame.c_str());

		return addResponseHandler(inRequest.requestId, result, expectedResponseType);
	}
	else
	{
		MIKAN_MT_LOG_ERROR("MikanRequestManager::sendRequest()")
			<< "Failed to serialize request to JSON string: " << serializeError;
		return addResponseHandler(inRequest.requestId, MikanAPIResult::MalformedParameters);
	}
}

MikanResponseFuture MikanRequestManager::addResponseHandler(MikanRequestID requestId, MikanAPIResult result,
															Serialization::StructTypeHandle expectedResponseType)
{
	MikanResponsePromise promise;
	MikanResponseFuture future(this, requestId, promise);

	if (result == MikanAPIResult::Success)
	{
		auto pendingRequest= std::make_shared<PendingRequest>();
		pendingRequest->id= requestId;
		pendingRequest->promise= std::move(promise);
		pendingRequest->expectedResponseType=
			expectedResponseType != nullptr ? expectedResponseType : &MikanResponse::staticGetArchetype();

		// Insert into the pending request map
		insertPendingRequest(pendingRequest);
	}
	else
	{
		auto errorResponse= std::make_shared<MikanResponse>();
		errorResponse->requestId= requestId;
		errorResponse->resultCode= result;

		promise.set_value(errorResponse);
	}

	return future;
}

void MikanRequestManager::insertPendingRequest(MikanRequestManager::PendingRequestPtr pendingRequest)
{
	std::lock_guard<std::mutex> lock(m_pending_request_map_mutex);

	m_pendingRequests.insert({pendingRequest->id, pendingRequest});
}

MikanRequestManager::PendingRequestPtr MikanRequestManager::removePendingRequest(MikanRequestID requestId)
{
	std::lock_guard<std::mutex> lock(m_pending_request_map_mutex);

	PendingRequestPtr pendingRequest;
	auto it= m_pendingRequests.find(requestId);
	if (it != m_pendingRequests.end())
	{
		pendingRequest= it->second;
		m_pendingRequests.erase(it);
	}

	return pendingRequest;
}

MikanAPIResult MikanRequestManager::cancelRequest(MikanRequestID requestId)
{
	PendingRequestPtr existingRequest= removePendingRequest(requestId);

	return existingRequest ? MikanAPIResult::Success : MikanAPIResult::InvalidParam;
}

void MikanRequestManager::textResponseHander(MikanRequestID requestId, const char* utf8ResponseString)
{
	// Find the pending request and remove it from the pending request map
	PendingRequestPtr pendingRequest= removePendingRequest(requestId);

	// Fulfill the promise with the response
	if (pendingRequest)
	{
		MikanResponsePtr response=
			parseResponseString(requestId, utf8ResponseString, pendingRequest->expectedResponseType);

		if (!response)
		{
			response= std::make_shared<MikanResponse>();
			response->requestId= requestId;
			response->resultCode= MikanAPIResult::MalformedResponse;
		}

		pendingRequest->promise.set_value(response);
	}
	else
	{
		MIKAN_MT_LOG_ERROR("MikanInterface::responseHander") << "Request ID not found: " << requestId;
	}
}

void MikanRequestManager::textResponseHandlerStatic(MikanRequestID requestId, const char* utf8ResponseString,
													void* userdata)
{
	MikanRequestManager* self= reinterpret_cast<MikanRequestManager*>(userdata);

	self->textResponseHander(requestId, utf8ResponseString);
}

MikanResponsePtr MikanRequestManager::parseResponseString(MikanRequestID requestId, const char* utf8ResponseString,
														  Serialization::StructTypeHandle expectedResponseType)
{
	JsonRpcFrame frame;
	if (JsonRpc::parseIncoming(utf8ResponseString, frame) != eJsonRpcFrameKind::response)
	{
		MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()") << "Not a response: " << utf8ResponseString;
		return nullptr;
	}

	const JsonRpcResponse& rpcResponse= frame.response;
	MikanResponsePtr responsePtr;

	// An error answers with the plain response whatever the request expected
	if (rpcResponse.error.has_value())
	{
		responsePtr= std::make_shared<MikanResponse>();
		responsePtr->resultCode= getErrorResultCode(*rpcResponse.error);
	}
	else
	{
		responsePtr= expectedResponseType->makeSharedInstance<MikanResponse>();

		std::string parseResponseError;
		if (!Serialization::deserializeFromJson(*rpcResponse.result, responsePtr.get(), expectedResponseType,
												parseResponseError))
		{
			MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()")
				<< "Failed to parse struct of type " << expectedResponseType->getName() << ": " << parseResponseError;
			return nullptr;
		}

		responsePtr->resultCode= MikanAPIResult::Success;
	}

	// The type name was stamped by the struct's constructor
	responsePtr->requestId= requestId;

	return responsePtr;
}
