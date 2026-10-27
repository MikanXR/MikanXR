#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "SerializableString.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanMarkerRequests.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanMarkerRequests"),
						Serialization::ResponseType("ArucoMarkerImageResponse")) GetArucoMarkerImageRequest
	: public MikanRequest
{
public:
	GetArucoMarkerImageRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(GetArucoMarkerImageRequest)}

	FIELD() int markerId= 0;

	FIELD() int imageSize= 200;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetArucoMarkerImageRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanMarkerRequests")) ArucoMarkerImageResponse
	: public MikanResponse
{
public:
	ArucoMarkerImageResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(ArucoMarkerImageResponse)}

	FIELD() Serialization::String imageData; // base64-encoded PNG

#ifdef MIKANAPI_REFLECTION_ENABLED
	ArucoMarkerImageResponse_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanMarkerRequests_GENERATED
#endif
