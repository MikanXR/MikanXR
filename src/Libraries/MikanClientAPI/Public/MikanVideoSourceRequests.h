#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanVideoSourceTypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanVideoSourceRequests.rfkh.h"
#endif

// Video Source Request Types
// ------

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest"),
						Serialization::ResponseType("MikanVideoSourceModeResponse")) GetVideoSourceMode
	: public MikanRequest
{
public:
	GetVideoSourceMode(){MIKAN_REQUEST_TYPE_INFO_INIT(GetVideoSourceMode)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetVideoSourceMode_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest"),
						Serialization::ResponseType("MikanVideoSourceIntrinsicsResponse")) GetVideoSourceIntrinsics
	: public MikanRequest
{
public:
	GetVideoSourceIntrinsics(){MIKAN_REQUEST_TYPE_INFO_INIT(GetVideoSourceIntrinsics)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetVideoSourceIntrinsics_GENERATED
#endif
};

/// Asks for a video source's stage alignment state
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest"),
						Serialization::ResponseType("MikanVideoSourceAlignmentResponse")) GetVideoSourceAlignment
	: public MikanRequest
{
public:
	GetVideoSourceAlignment(){MIKAN_REQUEST_TYPE_INFO_INIT(GetVideoSourceAlignment)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetVideoSourceAlignment_GENERATED
#endif
};

/// A client that receives a source's stream asks the editor to watch its frames
/// for the origin marker while the client is connected, so an unaligned stream is
/// aligned even when nothing else in the editor consumes the source. The watch
/// ends with the connection or with watch= false.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) SetVideoSourceAlignmentWatch
	: public MikanRequest
{
public:
	SetVideoSourceAlignmentWatch(){MIKAN_REQUEST_TYPE_INFO_INIT(SetVideoSourceAlignmentWatch)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;
	FIELD() bool watch= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetVideoSourceAlignmentWatch_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) SetUSBVideoSourceDevice
	: public MikanRequest
{
public:
	SetUSBVideoSourceDevice(){MIKAN_REQUEST_TYPE_INFO_INIT(SetUSBVideoSourceDevice)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

	FIELD() Serialization::String device_path;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetUSBVideoSourceDevice_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) SetUSBVideoSourceResolution
	: public MikanRequest
{
public:
	SetUSBVideoSourceResolution(){MIKAN_REQUEST_TYPE_INFO_INIT(SetUSBVideoSourceResolution)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

	FIELD() Serialization::String resolution;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetUSBVideoSourceResolution_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) SetUSBVideoSourceFrameRate
	: public MikanRequest
{
public:
	SetUSBVideoSourceFrameRate(){MIKAN_REQUEST_TYPE_INFO_INIT(SetUSBVideoSourceFrameRate)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

	FIELD() Serialization::String frame_rate;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetUSBVideoSourceFrameRate_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) SetUSBVideoSourceFormat
	: public MikanRequest
{
public:
	SetUSBVideoSourceFormat(){MIKAN_REQUEST_TYPE_INFO_INIT(SetUSBVideoSourceFormat)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;

	FIELD() Serialization::String format;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetUSBVideoSourceFormat_GENERATED
#endif
};

// Video Source Response Types
// ------

/// Static properties about a video source
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) MikanVideoSourceModeResponse
	: public MikanResponse
{
	MikanVideoSourceModeResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanVideoSourceModeResponse)}

	FIELD() MikanVideoSourceType video_source_type= MikanVideoSourceType::MONO;
	FIELD() Serialization::String video_source_api;
	FIELD() Serialization::String device_path;
	FIELD() Serialization::String video_mode_name;
	FIELD() int32_t resolution_x= 0;
	FIELD() int32_t resolution_y= 0;
	FIELD() float frame_rate= 0.0f;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVideoSourceModeResponse_GENERATED
#endif
};

/// Whether a video source's poses are anchored to the stage.
/// is_aligned: the source holds a stage alignment. A source whose poses are stage
/// space by construction is always aligned.
/// marker_visible: the editor's marker watch saw the origin marker on its last detection.
/// is_watching: the editor is watching the source for the origin marker, so an
/// unaligned source that is not watched is waiting on the editor rather than on
/// the camera being pointed at the marker.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) MikanVideoSourceAlignmentResponse
	: public MikanResponse
{
	MikanVideoSourceAlignmentResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanVideoSourceAlignmentResponse)}

	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;
	FIELD() bool is_aligned= false;
	FIELD() bool marker_visible= false;
	FIELD() bool is_watching= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVideoSourceAlignmentResponse_GENERATED
#endif
};

/// Bundle containing all intrinsic video source properties
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVideoSourceRequest")) MikanVideoSourceIntrinsicsResponse
	: public MikanResponse
{
	MikanVideoSourceIntrinsicsResponse()
		: intrinsics(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanVideoSourceIntrinsicsResponse)}

		FIELD() MikanVideoSourceIntrinsics intrinsics;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVideoSourceIntrinsicsResponse_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanVideoSourceRequests_GENERATED
#endif