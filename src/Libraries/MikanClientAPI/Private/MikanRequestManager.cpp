#include "MikanRequestManager.h"
#include "MikanAPITypes.h"
#include "MikanCoreCAPI.h"
#include "MikanCoreTypes.h"
#include "Logger.h"
#include "JsonDeserializer.h"
#include "SerializableObjectPtr.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include "assert.h"

using json= nlohmann::json;

MikanAPIResult MikanRequestManager::init(MikanContext context)
{
	m_context= context;

	return (MikanAPIResult)Mikan_SetTextResponseCallback(context, textResponseHandlerStatic, this);
}

MikanResponseFuture MikanRequestManager::sendRequest(MikanRequest& inRequest)
{
	Serialization::StructTypeHandle requestStruct=
		Serialization::TypeRegistry::getStructByName(inRequest.requestTypeName.getUtf8Value());
	assert(requestStruct != nullptr);

	// Stamp the request with the next available request ID
	inRequest.requestId= m_nextRequestID;
	m_nextRequestID++;

	std::string jsonString;
	std::string serializeError;
	if (Serialization::serializeToJsonString(&inRequest, requestStruct, jsonString, serializeError))
	{
		MikanAPIResult result= (MikanAPIResult)Mikan_SendRequestJSON(m_context, jsonString.c_str());

		return addResponseHandler(inRequest.requestId, result);
	}
	else
	{
		MIKAN_MT_LOG_ERROR("MikanRequestManager::sendRequest()")
			<< "Failed to serialize request to JSON string: " << serializeError;
		return addResponseHandler(inRequest.requestId, MikanAPIResult::MalformedParameters);
	}
}

MikanResponseFuture MikanRequestManager::addResponseHandler(MikanRequestID requestId, MikanAPIResult result)
{
	MikanResponsePromise promise;
	MikanResponseFuture future(this, requestId, promise);

	if (result == MikanAPIResult::Success)
	{
		auto pendingRequest= std::make_shared<PendingRequest>();
		pendingRequest->id= requestId;
		pendingRequest->promise= std::move(promise);

		// Insert into the pending request map
		insertPendingRequest(pendingRequest);
	}
	else
	{
		auto errorResponse= std::make_shared<MikanResponse>();
		errorResponse->responseTypeName= MikanResponse::staticGetArchetype().getName();
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
		MikanResponsePtr response= parseResponseString(utf8ResponseString);

		if (!response)
		{
			response= std::make_shared<MikanResponse>();
			response->responseTypeName= MikanResponse::staticGetArchetype().getName();
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

MikanResponsePtr MikanRequestManager::parseResponseString(const char* utf8ResponseString)
{
	MikanResponsePtr responsePtr;

	try
	{
		json jsonResponse= json::parse(utf8ResponseString);

		MikanResponse responseHeader= {};
		std::string parseHeaderError;
		if (Serialization::deserializeFromJson(jsonResponse, &responseHeader, &MikanResponse::staticGetArchetype(),
											   parseHeaderError))
		{
			Serialization::StructTypeHandle responseStruct=
				Serialization::TypeRegistry::getStructByName(responseHeader.responseTypeName.getUtf8Value());
			if (responseStruct != nullptr)
			{
				responsePtr= responseStruct->makeSharedInstance<MikanResponse>();

				std::string parseResponseError;
				if (!Serialization::deserializeFromJson(jsonResponse, responsePtr.get(), responseStruct,
														parseResponseError))
				{
					MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()")
						<< "Failed to parse struct of type " << responseHeader.responseTypeName.getUtf8Value() << ": "
						<< parseResponseError;
					responsePtr= nullptr;
				}
			}
			else
			{
				MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()")
					<< "Failed to find struct of type " << responseHeader.responseTypeName.getUtf8Value();
			}
		}
		else
		{
			MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()")
				<< "Failed to parse response header: " << parseHeaderError;
		}
	}
	catch (json::parse_error& e)
	{
		MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()") << "Failed to parse response: " << e.what();
	}
	catch (json::exception& e)
	{
		MIKAN_MT_LOG_ERROR("MikanClient::parseResponseString()") << "Failed to parse response: " << e.what();
	}

	return responsePtr;
}
