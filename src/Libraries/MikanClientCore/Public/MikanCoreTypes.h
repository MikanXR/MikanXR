#pragma once

#include "MikanCoreExport.h"
#include "SerializationProperty.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef MIKANCORE_REFLECTION_ENABLED
#include "MikanCoreTypes.rfkh.h"
#endif // MIKANAPI_REFLECTION_ENABLED

#define INVALID_MIKAN_ID -1

/// Result enum in response to a client API request
enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanConstants
{
	MikanConstants_InvalidMikanID ENUMVALUE_STRING("InvalidMikanID")= -1,
	MikanConstants_ClientAPIVersion ENUMVALUE_STRING("ClientAPIVersion")= 0,
};

/// Result enum for Client Core API
/// These values must remain stable across all versions of the API
enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanCoreResult
{
	MikanCoreResult_Success ENUMVALUE_STRING("Success")= 0,           ///< General Success Result
	MikanCoreResult_GeneralError ENUMVALUE_STRING("GeneralError")= 1, ///< General Error Result
	MikanCoreResult_Uninitialized ENUMVALUE_STRING("Uninitialized")= 2,
	MikanCoreResult_NullParam ENUMVALUE_STRING("NullParam")= 3,
	MikanCoreResult_InvalidParam ENUMVALUE_STRING("InvalidParam")= 4,
	MikanCoreResult_RequestFailed ENUMVALUE_STRING("RequestFailed")= 5,
	MikanCoreResult_NotConnected ENUMVALUE_STRING("NotConnected")= 6,
	MikanCoreResult_AlreadyConnected ENUMVALUE_STRING("AlreadyConnected")= 7,
	MikanCoreResult_SocketError ENUMVALUE_STRING("SocketError")= 8,
	MikanCoreResult_Timeout ENUMVALUE_STRING("Timeout")= 9,
	MikanCoreResult_Canceled ENUMVALUE_STRING("Canceled")= 10,
	MikanCoreResult_NoData ENUMVALUE_STRING("NoData")= 11,
	MikanCoreResult_BufferTooSmall ENUMVALUE_STRING("BufferTooSmall")= 12,
	MikanCoreResult_UnknownClient ENUMVALUE_STRING("UnknownClient")= 13,
	MikanCoreResult_UnknownFunction ENUMVALUE_STRING("UnknownFunction")= 14,
	MikanCoreResult_MalformedParameters ENUMVALUE_STRING("MalformedParameters")= 15,
	MikanCoreResult_MalformedResponse ENUMVALUE_STRING("MalformedResponse")= 16,
};

/// Disconnection Result Codes
/// These values must remain stable across all versions of the API
enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanDisconnectCode
{
	MikanDisconnectCode_Normal ENUMVALUE_STRING("Normal")= 1000,
	MikanDisconnectCode_ProtocolError ENUMVALUE_STRING("ProtocolError")= 1002,
	MikanDisconnectCode_NoStatus ENUMVALUE_STRING("NoStatus")= 1005,
	MikanDisconnectCode_AbnormalClose ENUMVALUE_STRING("AbnormalClose")= 1006,
	MikanDisconnectCode_InvalidFramePayload ENUMVALUE_STRING("InvalidFramePayload")= 1007,
	MikanDisconnectCode_InternalError ENUMVALUE_STRING("InternalError")= 1011,
	MikanDisconnectCode_IncompatibleVersion ENUMVALUE_STRING("IncompatibleVersion")= 2000,
};

enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanLogLevel
{
	MikanLogLevel_Trace ENUMVALUE_STRING("Trace"),
	MikanLogLevel_Debug ENUMVALUE_STRING("Debug"),
	MikanLogLevel_Info ENUMVALUE_STRING("Info"),
	MikanLogLevel_Warning ENUMVALUE_STRING("Warning"),
	MikanLogLevel_Error ENUMVALUE_STRING("Error"),
	MikanLogLevel_Fatal ENUMVALUE_STRING("Fatal")
};

enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanClientGraphicsApi
{
	MikanClientGraphicsApi_UNKNOWN ENUMVALUE_STRING("UNKNOWN")= -1,

	MikanClientGraphicsApi_Direct3D9 ENUMVALUE_STRING("Direct3D9"),
	MikanClientGraphicsApi_Direct3D11 ENUMVALUE_STRING("Direct3D11"),
	MikanClientGraphicsApi_Direct3D12 ENUMVALUE_STRING("Direct3D12"),
	MikanClientGraphicsApi_OpenGL ENUMVALUE_STRING("OpenGL"),
	MikanClientGraphicsApi_Metal ENUMVALUE_STRING("Metal"),
	MikanClientGraphicsApi_Vulkan ENUMVALUE_STRING("Vulkan"),

	MikanClientGraphicsApi_COUNT,
};

enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanColorBufferType
{
	MikanColorBuffer_NOCOLOR ENUMVALUE_STRING("NOCOLOR"),
	MikanColorBuffer_RGB24 ENUMVALUE_STRING("RGB24"),
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	MikanColorBuffer_RGBA32 ENUMVALUE_STRING("RGBA32"),
	// DXGI_FORMAT_B8G8R8A8_UNORM / DXGI_FORMAT_B8G8R8A8_TYPELESS
	MikanColorBuffer_BGRA32 ENUMVALUE_STRING("BGRA32"),
	// 16-bit-per-channel half-float (64bpp), RGBA order / DXGI_FORMAT_R16G16B16A16_FLOAT
	MikanColorBuffer_RGBA16F ENUMVALUE_STRING("RGBA16F"),
};

enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanDepthBufferType
{
	MikanDepthBuffer_NODEPTH ENUMVALUE_STRING("NOCOLOR"),
	// Raw float non-linear depth values from the z-buffer (in source world units)
	MikanDepthBuffer_FLOAT_DEVICE_DEPTH ENUMVALUE_STRING("FLOAT_DEVICE_DEPTH"),
	// Linearized float distance-from-camera values (in source world units)
	MikanDepthBuffer_FLOAT_SCENE_DEPTH ENUMVALUE_STRING("FLOAT_SCENE_DEPTH"),
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	MikanDepthBuffer_PACK_DEPTH_RGBA ENUMVALUE_STRING("PACK_DEPTH_RGBA"),
};

// Optional second color buffer carrying a per-channel shadow (light survival) factor,
// composited multiplicatively against the scene (identity / no-shadow is white).
enum ENUM(Serialization::CodeGenModule("MikanCoreConstants")) MikanShadowBufferType
{
	MikanShadowBuffer_NOSHADOW ENUMVALUE_STRING("NOSHADOW"),
	MikanShadowBuffer_RGB24 ENUMVALUE_STRING("RGB24"),
	// DXGI_FORMAT_R8G8B8A8_UNORM / DXGI_FORMAT_R8G8B8A8_TYPELESS
	MikanShadowBuffer_RGBA32 ENUMVALUE_STRING("RGBA32"),
	// DXGI_FORMAT_B8G8R8A8_UNORM / DXGI_FORMAT_B8G8R8A8_TYPELESS
	MikanShadowBuffer_BGRA32 ENUMVALUE_STRING("BGRA32"),
	// 16-bit-per-channel half-float (64bpp), RGBA order / DXGI_FORMAT_R16G16B16A16_FLOAT
	MikanShadowBuffer_RGBA16F ENUMVALUE_STRING("RGBA16F"),
};

struct MIKAN_CORE_API STRUCT(Serialization::CodeGenModule("MikanCoreTypes")) MikanClientAPIVersion
{
	FIELD() int version= 0;

#ifdef MIKANCORE_REFLECTION_ENABLED
	MikanClientAPIVersion_GENERATED
#endif // MIKANCORE_REFLECTION_ENABLED
};

struct MIKAN_CORE_API STRUCT(Serialization::CodeGenModule("MikanCoreTypes")) MikanRenderTargetDescriptor
{
	FIELD() MikanColorBufferType color_buffer_type= MikanColorBufferType::MikanColorBuffer_NOCOLOR;
	FIELD() MikanDepthBufferType depth_buffer_type= MikanDepthBufferType::MikanDepthBuffer_NODEPTH;
	FIELD() MikanShadowBufferType shadow_buffer_type= MikanShadowBufferType::MikanShadowBuffer_NOSHADOW;
	/// Size of the color buffer
	FIELD() uint32_t width= 0;
	FIELD() uint32_t height= 0;
	/// Size of the depth and shadow buffers. 0 means same as width/height.
	FIELD() uint32_t aux_width= 0;
	FIELD() uint32_t aux_height= 0;
	FIELD() MikanClientGraphicsApi graphicsAPI= MikanClientGraphicsApi_UNKNOWN;

#ifdef MIKANCORE_REFLECTION_ENABLED
	MikanRenderTargetDescriptor_GENERATED
#endif // MIKANCORE_REFLECTION_ENABLED
};

// Wrapper Types
//--------------
/// The ID of a pending request send to Mikan
typedef int MikanRequestID;

/// The ID of a camera
typedef int MikanCameraID;

/// Registered text response callback function for a Mikan request
typedef void(MIKAN_CALLBACK* MikanTextResponseCallback)(MikanRequestID request_id, const char* utf8_response_string,
														void* userdata);

/// Registered binary response callback function for a Mikan request
typedef void(MIKAN_CALLBACK* MikanBinaryResponseCallback)(const uint8_t* buffer, size_t buffer_size, void* userdata);

typedef void(MIKAN_CALLBACK* MikanLogCallback)(int /*log_level*/, const char* /*log_message*/);

#ifdef MIKANCORE_REFLECTION_ENABLED
File_MikanCoreTypes_GENERATED
#endif // MIKANCORE_REFLECTION_ENABLED