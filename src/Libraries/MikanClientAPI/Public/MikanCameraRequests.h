#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanCameraTypes.h"
#include "MikanMathTypes.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCameraRequests.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) AllocateCameraRenderTargetTextures
	: public MikanRequest
{
public:
	AllocateCameraRenderTargetTextures(){MIKAN_REQUEST_TYPE_INFO_INIT(AllocateCameraRenderTargetTextures)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	FIELD() MikanRenderTargetDescriptor descriptor;

#ifdef MIKANAPI_REFLECTION_ENABLED
	AllocateCameraRenderTargetTextures_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) WriteCameraColorRenderTargetTexture
	: public MikanRequest
{
public:
	WriteCameraColorRenderTargetTexture(){MIKAN_REQUEST_TYPE_INFO_INIT(WriteCameraColorRenderTargetTexture)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	FIELD() void* api_color_texture_ptr= nullptr;

#ifdef MIKANAPI_REFLECTION_ENABLED
	WriteCameraColorRenderTargetTexture_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) WriteCameraDepthRenderTargetTexture
	: public MikanRequest
{
public:
	WriteCameraDepthRenderTargetTexture(){MIKAN_REQUEST_TYPE_INFO_INIT(WriteCameraDepthRenderTargetTexture)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	FIELD() void* api_depth_texture_ptr= nullptr;

	// Need to know the min and max z values for R32 -> RGBA depth packing
	// Since the camera intrinsics can in theory vary per frame
	FIELD() float z_near= 0.f;

	FIELD() float z_far= 0.f;

#ifdef MIKANAPI_REFLECTION_ENABLED
	WriteCameraDepthRenderTargetTexture_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) WriteCameraShadowRenderTargetTexture
	: public MikanRequest
{
public:
	WriteCameraShadowRenderTargetTexture(){MIKAN_REQUEST_TYPE_INFO_INIT(WriteCameraShadowRenderTargetTexture)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	FIELD() void* api_shadow_texture_ptr= nullptr;

#ifdef MIKANAPI_REFLECTION_ENABLED
	WriteCameraShadowRenderTargetTexture_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) PublishCameraRenderTargetTextures
	: public MikanRequest
{
public:
	PublishCameraRenderTargetTextures(){MIKAN_REQUEST_TYPE_INFO_INIT(PublishCameraRenderTargetTextures)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	FIELD() int64_t frame_index= 0;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PublishCameraRenderTargetTextures_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) FreeCameraRenderTargetTextures
	: public MikanRequest
{
public:
	FreeCameraRenderTargetTextures(){MIKAN_REQUEST_TYPE_INFO_INIT(FreeCameraRenderTargetTextures)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	FreeCameraRenderTargetTextures_GENERATED
#endif
};

/// Pulls the camera state a client renders from. A client that starts, or binds a
/// camera, after the last MikanCameraNewPropertiesEvent went out asks for it here
/// rather than waiting for the state to change.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) GetCameraProperties : public MikanRequest
{
public:
	GetCameraProperties(){MIKAN_REQUEST_TYPE_INFO_INIT(GetCameraProperties)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetCameraProperties_GENERATED
#endif
};

/// The same fields as MikanCameraNewPropertiesEvent, plus whether a compositor is
/// running for the camera and so consuming what a client publishes for it.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCameraRequests")) MikanCameraPropertiesResponse
	: public MikanResponse
{
	MikanCameraPropertiesResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(MikanCameraPropertiesResponse)}

	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;
	FIELD() bool compositor_running= false;
	FIELD() MikanVector3f camera_forward;
	FIELD() MikanVector3f camera_up;
	FIELD() MikanVector3f camera_position;
	FIELD() MikanVector2i pixel_size;
	FIELD() MikanVector2i aux_pixel_size;
	FIELD() MikanVector2d focal_length;
	FIELD() MikanVector2d principal_point;
	FIELD() MikanVector2d z_bounds;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCameraPropertiesResponse_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCameraRequests_GENERATED
#endif