#pragma once

#include "MikanCoreExport.h"
#include "MikanCoreTypes.h"

typedef void* MikanContext;

/** \brief Initializes the MikanXR Client API.
 This function must be called before calling any other client functions.
 Calling this function after the api is already initialized will return MikanCoreResult_Success.
 */
MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_Initialize(const char* client_name, MikanLogLevel min_log_level, MikanLogCallback log_callback,
				 MikanContext* outContext);

MIKAN_CORE_CAPI(const char*)
Mikan_GetClientName(MikanContext context);

MIKAN_CORE_CAPI(bool)
Mikan_GetIsInitialized(MikanContext context);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_SetGraphicsDeviceInterface(MikanContext context, MikanClientGraphicsApi api, void* graphicsDeviceInterface);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_GetGraphicsDeviceInterface(MikanContext context, MikanClientGraphicsApi api, void** outGraphicsDeviceInterface);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_SetGraphicsCommandQueueInterface(MikanContext context, MikanClientGraphicsApi api,
									   void* graphicsCommandQueueInterface);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_GetGraphicsCommandQueueInterface(MikanContext context, MikanClientGraphicsApi api,
									   void** outGraphicsCommandQueueInterface);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_GetCameraRenderTargetDescriptor(MikanContext context, MikanCameraID camera_id,
									  MikanRenderTargetDescriptor* out_descriptor);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_AllocateCameraRenderTargetTextures(MikanContext context, MikanCameraID camera_id,
										 const MikanRenderTargetDescriptor* descriptor);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_FreeCameraRenderTargetTextures(MikanContext context, MikanCameraID camera_id);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_WriteCameraColorRenderTargetTexture(MikanContext context, MikanCameraID camera_id, void* color_texture);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_WriteCameraDepthRenderTargetTexture(MikanContext context, MikanCameraID camera_id, void* depth_texture,
										  float z_near, float z_far);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_WriteCameraShadowRenderTargetTexture(MikanContext context, MikanCameraID camera_id, void* shadow_texture);

MIKAN_CORE_CAPI(void*)
Mikan_GetCameraPackDepthTextureResourcePtr(MikanContext context, MikanCameraID camera_id);

/** \brief Initializes a connection to MikanXR.
 Starts connection process to MikanXR at the given address and port.
	.
 \param host The address that MikanXR is running at, usually MIKANXR_DEFAULT_ADDRESS
 \param port The port that MikanXR is running at, usually MIKANXR_DEFAULT_PORT
 */
MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_Connect(MikanContext context, const char* host, const char* port);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_Disconnect(MikanContext context, uint16_t code, const char* reason);

/** \brief Get the client connection status
	\return true if the client is connected to MikanXR Client API
 */
MIKAN_CORE_CAPI(bool)
Mikan_GetIsConnected(MikanContext context);

// Copies the buffer into the provided buffer and returns the number of bytes written
// If no buffer is provided, the function will return the size of the buffer needed to store the event
MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_FetchNextEvent(MikanContext context, size_t utf8_buffer_size, char* out_utf8_buffer,
					 size_t* out_utf8_bytes_written);

// Sends a MikanRequest as a UTF8 encoded JSON string
MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_SendRequestJSON(MikanContext context, const char* utf8_request_json);

MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_SetTextResponseCallback(MikanContext context, MikanTextResponseCallback callback, void* callback_userdata);

/** \brief Cleans up the MikanXR Client API
 Free the resources allocated by the MikanXR Client API.
 Calling this function again after the api already cleaned up will return MikanCoreResult_Uninitialized.
 */
MIKAN_CORE_CAPI(MikanCoreResult)
Mikan_Shutdown(MikanContext context);