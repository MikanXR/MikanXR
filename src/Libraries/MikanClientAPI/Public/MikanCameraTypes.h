#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanTransformTypes.h"
#include "SerializableList.h"
#include "SerializableObjectPtr.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCameraTypes.rfkh.h"
#endif

#include <assert.h>

// Enums
/// How a client paces its renders for a camera. Auto resolves at runtime from
/// whether the camera's pose is driven per frame (pose_driven_per_frame).
enum ENUM(Serialization::CodeGenModule("MikanCameraTypes")) MikanCameraFrameSyncMode
{
	MikanCameraFrameSyncMode_Auto ENUMVALUE_STRING("Auto")= 0,
	MikanCameraFrameSyncMode_VideoFrame ENUMVALUE_STRING("VideoFrame"),
	MikanCameraFrameSyncMode_FreeRunning ENUMVALUE_STRING("FreeRunning"),
};

/// Ceiling on the longer edge of every buffer a client renders for a camera, applied after
/// the render scales and preserving aspect ratio. Caps what a scale can demand of a client's
/// texture allocator, which the scales alone cannot do since they multiply a video
/// resolution the camera does not choose.
enum ENUM(Serialization::CodeGenModule("MikanCameraTypes")) MikanClientMaxBufferDimension
{
	// The string is the member name the generated bindings carry, so it has to be an
	// identifier rather than the bare pixel count
	MikanClientMaxBufferDimension_1024 ENUMVALUE_STRING("Dim1024")= 0,
	MikanClientMaxBufferDimension_2048 ENUMVALUE_STRING("Dim2048"),
	MikanClientMaxBufferDimension_4096 ENUMVALUE_STRING("Dim4096"),
	MikanClientMaxBufferDimension_8192 ENUMVALUE_STRING("Dim8192"),
};

// Structures
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraTypes")) MikanCameraComponentValues
	: public MikanTransformComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanStageID stage_id= INVALID_MIKAN_ID;
	FIELD() MikanTrackingMountID tracking_mount_id= INVALID_MIKAN_ID;
	FIELD() MikanVideoSourceID video_source_id= INVALID_MIKAN_ID;
	FIELD() MikanLightID light_environment_id= INVALID_MIKAN_ID;
	FIELD() int tracking_frame_delay= 0;
	FIELD() MikanCameraFrameSyncMode frame_sync_mode= MikanCameraFrameSyncMode_Auto;
	FIELD() bool pose_driven_per_frame= false; ///< read only: a tracking mount or frame-coupled source drives the pose
	/// Multiplier on the video source resolution for the color buffer a client renders.
	/// Above 1 the client supersamples: the composite stays at video resolution and
	/// downsamples what arrives, which is what smooths a rendered silhouette.
	FIELD() float client_color_render_scale= 1.f;
	/// The same multiplier for the depth and shadow buffers, which rarely need the
	/// color buffer's resolution and cost a full render pass each.
	FIELD() float client_aux_render_scale= 1.f;
	FIELD() MikanClientMaxBufferDimension client_max_buffer_dimension= MikanClientMaxBufferDimension_4096;
	FIELD() MikanQuatd aperture_orientation_offset;
	FIELD() MikanVector3d aperture_position_offset;
	FIELD() bool has_valid_aperture_offset= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCameraComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCameraTypes_GENERATED
#endif