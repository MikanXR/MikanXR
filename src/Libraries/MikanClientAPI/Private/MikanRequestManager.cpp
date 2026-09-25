#include "MikanRequestManager.h"
#include "MikanAPITypes.h"
#include "MikanCoreCAPI.h"
#include "MikanCoreTypes.h"
#include "Logger.h"
#include "BinaryDeserializer.h"
#include "JsonDeserializer.h"
#include "SerializableObjectPtr.h"
#include "TypeRegistry.h"

#include <nlohmann/json.hpp>

#include "assert.h"

using json= nlohmann::json;

MikanAPIResult MikanRequestManager::init(MikanContext context)
{
	m_context= context;

	MikanAPIResult result= (MikanAPIResult)Mikan_SetTextResponseCallback(context, textResponseHandlerStatic, this);
	if (result != MikanAPIResult::Success)
	{
		return result;
	}

	result= (MikanAPIResult)Mikan_SetBinaryResponseCallback(context, binaryResponseHandlerStatic, this);
	if (result != MikanAPIResult::Success)
	{
		return result;
	}

	return MikanAPIResult::Success;
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

void MikanRequestManager::binaryResponseHander(const uint8_t* buffer, size_t bufferSize)
{
	BinaryReader reader(buffer, bufferSize);
	PendingRequestPtr pendingRequest;

	std::string parseError;
	MikanResponse responseHeader= {};
	if (Serialization::deserializeFromBytes(buffer, bufferSize, &responseHeader, &MikanResponse::staticGetArchetype(),
											parseError))
	{
		// Find the pending request and remove it from the pending request map
		pendingRequest= removePendingRequest(responseHeader.requestId);

		// Fulfill the promise with the response
		if (pendingRequest)
		{
			MikanResponsePtr response= parseResponseBinaryReader(responseHeader, buffer, bufferSize);

			if (!response)
			{
				response= std::make_shared<MikanResponse>();
				response->responseTypeName= MikanResponse::staticGetArchetype().getName();
				response->requestId= responseHeader.requestId;
				response->resultCode= MikanAPIResult::MalformedResponse;
			}

			pendingRequest->promise.set_value(response);
		}
		else
		{
			MIKAN_MT_LOG_ERROR("MikanInterface::responseHander")
				<< "Request ID not found: " << responseHeader.requestId;
		}
	}
	else
	{
		MIKAN_MT_LOG_ERROR("MikanInterface::responseHander") << "Failed to parse response: " << parseError;
		return;
	}
}

void MikanRequestManager::binaryResponseHandlerStatic(const uint8_t* buffer, size_t bufferSize, void* userdata)
{
	MikanRequestManager* self= reinterpret_cast<MikanRequestManager*>(userdata);

	self->binaryResponseHander(buffer, bufferSize);
}

MikanResponsePtr MikanRequestManager::parseResponseBinaryReader(const MikanResponse& responseHeader,
																const uint8_t* buffer, size_t bufferSize)
{
	MikanResponsePtr responsePtr;

	Serialization::StructTypeHandle responseStruct=
		Serialization::TypeRegistry::getStructByName(responseHeader.responseTypeName.getUtf8Value());
	if (responseStruct != nullptr)
	{
		std::string parseError;
		responsePtr= responseStruct->makeSharedInstance<MikanResponse>();

		if (!Serialization::deserializeFromBytes(buffer, bufferSize, responsePtr.get(), responseStruct, parseError))
		{
			MIKAN_MT_LOG_WARNING("MikanClient::parseResponseBinaryReader()")
				<< "Failed to parse response: " << parseError;
			responsePtr= nullptr;
		}
	}
	else
	{
		MIKAN_MT_LOG_WARNING("MikanClient::parseResponseBinaryReader()")
			<< "Received response of unknown responseType: " << responseHeader.responseTypeName.getUtf8Value();
	}

	return responsePtr;
}