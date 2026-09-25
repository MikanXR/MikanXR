#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCameraEvents.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraEvents")) MikanCameraNewFrameEvent : public MikanEvent
{
	MikanCameraNewFrameEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanCameraNewFrameEvent)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;
	FIELD() MikanVector3f camera_forward;
	FIELD() MikanVector3f camera_up;
	FIELD() MikanVector3f camera_position;
	/// Size of the color buffer to render, and the resolution focal_length and
	/// principal_point are expressed in
	FIELD() MikanVector2i pixel_size;
	/// Size of the depth and shadow buffers to render. Same projection as the color
	/// buffer, just a different target size. Zero means render them at pixel_size.
	FIELD() MikanVector2i aux_pixel_size;
	FIELD() MikanVector2d focal_length;
	FIELD() MikanVector2d principal_point;
	FIELD() MikanVector2d z_bounds;
	FIELD() int64_t frame= 0;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCameraNewFrameEvent_GENERATED
#endif
};

/// The camera state a client renders from, without a frame to render it for.
/// Published for a free-running camera whenever these values change, in place
/// of a MikanCameraNewFrameEvent per video frame.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraEvents")) MikanCameraNewPropertiesEvent
	: public MikanEvent
{
	MikanCameraNewPropertiesEvent(){MIKAN_EVENT_TYPE_INFO_INIT(MikanCameraNewPropertiesEvent)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;
	FIELD() MikanVector3f camera_forward;
	FIELD() MikanVector3f camera_up;
	FIELD() MikanVector3f camera_position;
	/// Size of the color buffer to render, and the resolution focal_length and
	/// principal_point are expressed in
	FIELD() MikanVector2i pixel_size;
	/// Size of the depth and shadow buffers to render. Same projection as the color
	/// buffer, just a different target size. Zero means render them at pixel_size.
	FIELD() MikanVector2i aux_pixel_size;
	FIELD() MikanVector2d focal_length;
	FIELD() MikanVector2d principal_point;
	FIELD() MikanVector2d z_bounds;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCameraNewPropertiesEvent_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCameraEvents_GENERATED
#endif