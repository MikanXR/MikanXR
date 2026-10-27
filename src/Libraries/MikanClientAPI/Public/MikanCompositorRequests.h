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
/// compositor_id -1 means every compositor allowed to publish output, which is the
/// scene's display compositor. The feed is sent beside the compositor's own
/// rtp_output_host at the compositor's size and bitrate.
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
