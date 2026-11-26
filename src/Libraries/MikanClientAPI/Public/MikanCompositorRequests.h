#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCompositorRequests.rfkh.h"
#endif

// Compositor Request Types
// ------

/// A client that receives the compositor's RTP output asks for it here. The destination
/// lives on the client's connection: it goes with the connection or with enable false.
/// compositor_id -1 means whichever compositor publishes output, which is the
/// current scene's. A feed that names a compositor flows only while that one
/// publishes. The feeds are a stream of their own at 720 lines and 4000 kbps,
/// whatever RTP outputs the project has.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCompositorRequest")) SetCompositorReturnFeed
	: public MikanRequest
{
public:
	SetCompositorReturnFeed(){MIKAN_REQUEST_TYPE_INFO_INIT(SetCompositorReturnFeed)}

	FIELD() MikanCompositorID compositor_id= INVALID_MIKAN_ID;
	FIELD() Serialization::String host;
	FIELD() int port= 0;
	FIELD() bool enable= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetCompositorReturnFeed_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCompositorRequests_GENERATED
#endif
