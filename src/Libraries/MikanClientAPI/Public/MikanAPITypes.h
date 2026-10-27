#pragma once

#include "MikanAPIExport.h"
#include "MikanTypeFwd.h"
#include "MikanCoreTypes.h"
#include "SerializableString.h"

#ifdef MIKANCORE_REFLECTION_ENABLED
#include "MikanCoreTypes.rfkh.h"
#endif // MIKANCORE_REFLECTION_ENABLED

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanAPITypes.rfkh.h"
#endif // MIKANAPI_REFLECTION_ENABLED

#include <string>

/// Result enum in response to a client API request
enum class ENUM(Serialization::CodeGenModule("MikanAPITypes")) MikanAPIResult
{
	// MikanCoreResult Enum Values
	// KEEP THESE IN SYNC WITH THE ENUM VALUES IN MikanCoreResult.h
	Success ENUMVALUE_STRING("Success")= 0,                          // MikanCoreResult_Success
	GeneralError ENUMVALUE_STRING("GeneralError")= 1,                // MikanCoreResult_GeneralError
	Uninitialized ENUMVALUE_STRING("Uninitialized")= 2,              // MikanCoreResult_Uninitialized
	NullParam ENUMVALUE_STRING("NullParam")= 3,                      // MikanCoreResult_NullParam
	InvalidParam ENUMVALUE_STRING("InvalidParam")= 4,                // MikanCoreResult_InvalidParam
	RequestFailed ENUMVALUE_STRING("RequestFailed")= 5,              // MikanCoreResult_RequestFailed
	NotConnected ENUMVALUE_STRING("NotConnected")= 6,                // MikanCoreResult_NotConnected
	AlreadyConnected ENUMVALUE_STRING("AlreadyConnected")= 7,        // MikanCoreResult_AlreadyConnected
	SocketError ENUMVALUE_STRING("SocketError")= 8,                  // MikanCoreResult_SocketError
	Timeout ENUMVALUE_STRING("Timeout")= 9,                          // MikanCoreResult_Timeout
	Canceled ENUMVALUE_STRING("Canceled")= 10,                       // MikanCoreResult_Canceled
	NoData ENUMVALUE_STRING("NoData")= 11,                           // MikanCoreResult_NoData
	BufferTooSmall ENUMVALUE_STRING("BufferTooSmall")= 12,           // MikanCoreResult_BufferTooSmall
	UnknownClient ENUMVALUE_STRING("UnknownClient")= 13,             // MikanCoreResult_UnknownClient
	UnknownFunction ENUMVALUE_STRING("UnknownFunction")= 14,         // MikanCoreResult_UnknownFunction
	MalformedParameters ENUMVALUE_STRING("MalformedParameters")= 15, // MikanCoreResult_MalformedParameters
	MalformedResponse ENUMVALUE_STRING("MalformedResponse")= 16,     // MikanCoreResult_MalformedResponse

	// MikanAPIResult Enum Values
	// Free to edit these, but keep enum values stable across versions
	NoVideoSource ENUMVALUE_STRING("NoVideoSource")= 100,
	NoVideoSourceAssignedTracker ENUMVALUE_STRING("NoVideoSourceAssignedTracker")= 101,
	InvalidDeviceId ENUMVALUE_STRING("InvalidDeviceId")= 102,
	InvalidStencilID ENUMVALUE_STRING("InvalidStencilID")= 103,
	InvalidAnchorID ENUMVALUE_STRING("InvalidAnchorID")= 104,
	InvalidCameraID ENUMVALUE_STRING("InvalidCameraID")= 105,
	InvalidSceneID ENUMVALUE_STRING("InvalidSceneID")= 106,
	InvalidStageID ENUMVALUE_STRING("InvalidStageID")= 106,
	InvalidShapeID ENUMVALUE_STRING("InvalidShapeID")= 107,
};

#ifdef MIKANAPI_REFLECTION_ENABLED
#define MIKAN_TYPE_INFO_INIT(classPrefix, className) classPrefix##TypeName= className::staticGetArchetype().getName();

#define MIKAN_EVENT_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(event, className)
#define MIKAN_REQUEST_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(request, className)
#define MIKAN_RESPONSE_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(response, className)
#else
// Consumers built without reflection still construct requests through these inline constructors,
// and the server routes on the type name, so stamp the class name directly. It equals the
// Refureku archetype name the reflection build produces.
#define MIKAN_TYPE_INFO_INIT(classPrefix, className) classPrefix##TypeName= #className;

#define MIKAN_EVENT_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(event, className)
#define MIKAN_REQUEST_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(request, className)
#define MIKAN_RESPONSE_TYPE_INFO_INIT(className) MIKAN_TYPE_INFO_INIT(response, className)
#endif // MIKANAPI_REFLECTION_ENABLED

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanAPITypes")) MikanRequest
{
	MikanRequest() { MIKAN_TYPE_INFO_INIT(request, MikanRequest) }
	virtual ~MikanRequest() {} // Virtual destructor for RTTI

	// The envelope carries these, so they are not reflected fields: requestTypeName is the
	// JSON-RPC method and requestId the id
	Serialization::String requestTypeName;
	MikanRequestID requestId= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanRequest_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanAPITypes")) MikanResponse
{
	MikanResponse() { MIKAN_TYPE_INFO_INIT(response, MikanResponse) }
	virtual ~MikanResponse() {} // Virtual destructor for RTTI

	// The envelope carries these, so they are not reflected fields: the id is the answered
	// request's, and resultCode is Success for a result and error.data.resultCode for an error.
	// responseTypeName names the struct the result was read into.
	Serialization::String responseTypeName;
	MikanRequestID requestId= INVALID_MIKAN_ID;
	MikanAPIResult resultCode= MikanAPIResult::Success;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanResponse_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanAPITypes")) MikanEvent
{
	MikanEvent() { MIKAN_TYPE_INFO_INIT(event, MikanEvent) }
	virtual ~MikanEvent() {} // Virtual destructor for RTTI

	// The envelope carries this as the JSON-RPC method, so it is not a reflected field
	Serialization::String eventTypeName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanEvent_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanAPITypes_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED