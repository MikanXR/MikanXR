#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCompositorEvents.rfkh.h"
#endif

/// A compositor began running for its camera. A client that renders on its own
/// clock treats this as the signal to start publishing frames for the camera.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCompositorEvents")) MikanCompositorStartedEvent
	: public MikanEvent
{
	MikanCompositorStartedEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanCompositorStartedEvent)}

	FIELD() MikanCompositorID compositor_id= INVALID_MIKAN_ID;
	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCompositorStartedEvent_GENERATED
#endif
};

/// A compositor stopped running for its camera. Nothing consumes frames
/// published for the camera until a compositor starts again.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCompositorEvents")) MikanCompositorStoppedEvent
	: public MikanEvent
{
	MikanCompositorStoppedEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanCompositorStoppedEvent)}

	FIELD() MikanCompositorID compositor_id= INVALID_MIKAN_ID;
	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCompositorStoppedEvent_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCompositorEvents_GENERATED
#endif
