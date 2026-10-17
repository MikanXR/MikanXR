#pragma once

#include "MikanCoreTypes.h"
#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanClientEvents.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanClientEvents")) MikanConnectedEvent : public MikanEvent
{
	MikanConnectedEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanConnectedEvent)}

	FIELD() MikanClientAPIVersion serverVersion;

	FIELD() MikanClientAPIVersion minClientVersion;

	FIELD() bool isClientCompatible= false;

	// The port of the editor's HTTP server, which serves model geometry on the host the client connected to
	FIELD() int httpPort= 0;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanConnectedEvent_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanClientEvents")) MikanDisconnectedEvent : public MikanEvent
{
	MikanDisconnectedEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanDisconnectedEvent)}

	FIELD() MikanDisconnectCode code= MikanDisconnectCode::MikanDisconnectCode_Normal;

	FIELD() Serialization::String reason;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanDisconnectedEvent_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanClientEvents_GENERATED
#endif