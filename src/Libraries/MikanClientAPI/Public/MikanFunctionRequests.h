#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanFunctionTypes.h"
#include "MikanVariantTypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanFunctionRequests.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanFunctionRequests")) InvokeSystemFunctionRequest
	: public MikanRequest
{
public:
	InvokeSystemFunctionRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(InvokeSystemFunctionRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::String functionName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	InvokeSystemFunctionRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanFunctionRequests")) InvokeComponentFunctionRequest
	: public MikanRequest
{
public:
	InvokeComponentFunctionRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(InvokeComponentFunctionRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() int componentId= INVALID_MIKAN_ID;
	FIELD() Serialization::String functionName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	InvokeComponentFunctionRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanFunctionRequests"),
						Serialization::ResponseType("FunctionDescriptorResponse")) GetFunctionListRequest
	: public MikanRequest
{
public:
	GetFunctionListRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(GetFunctionListRequest)}

	FIELD() Serialization::String systemFilter;
	FIELD() Serialization::String componentFilter;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetFunctionListRequest_GENERATED
#endif
};

// Function Response Types
// ------

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanFunctionRequests")) FunctionDescriptorResponse
	: public MikanResponse
{
	FunctionDescriptorResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(FunctionDescriptorResponse)}

	FIELD() Serialization::List<MikanFunctionDescriptor> descriptor_list;

#ifdef MIKANAPI_REFLECTION_ENABLED
	FunctionDescriptorResponse_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanFunctionRequests_GENERATED
#endif
