#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanLightTypes.h"
#include "SerializableList.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanLightRequests.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanLightRequests")) SetLightDMXDataSubcription
	: public MikanRequest
{
	SetLightDMXDataSubcription(){MIKAN_REQUEST_TYPE_INFO_INIT(SetLightDMXDataSubcription)}

	FIELD() Serialization::List<MikanLightID> light_ids;

	FIELD() bool subscribe= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetLightDMXDataSubcription_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanLightRequests"),
						Serialization::ResponseType("MikanDMXDataResponse")) GetDMXData : public MikanRequest
{
	GetDMXData(){MIKAN_REQUEST_TYPE_INFO_INIT(GetDMXData)}

	FIELD() Serialization::List<int> dmx_universe_ids;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetDMXData_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanLightRequests")) MikanDMXDataResponse : public MikanResponse
{
	MikanDMXDataResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanDMXDataResponse)}

	FIELD() MikanDMXData dmx_data;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanDMXDataResponse_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanLightRequests_GENERATED
#endif
