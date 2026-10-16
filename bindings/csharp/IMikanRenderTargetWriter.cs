using System;

namespace MikanXR
{
	// The client side of the shared texture seam: whatever owns the Spout senders the editor
	// reads a camera's color, depth and shadow buffers from. MikanNativeRenderTargetWriter hands
	// the work to MikanClientCore.dll. A managed client supplies its own, writing senders named
	// "<clientId>_camera<N>_color", "_depth" and "_shadow" through whatever Spout library its
	// engine already carries.
	//
	// MikanRenderTargetAPI routes the render target requests here before anything reaches the
	// wire, so a client calls MikanAPI.SendRequest with the generated request types either way.
	public interface IMikanRenderTargetWriter
	{
		// May adjust the descriptor to what was actually allocated, which is what the editor is
		// then told.
		MikanAPIResult AllocateRenderTargetTextures(int cameraId, ref MikanRenderTargetDescriptor descriptor);
		MikanAPIResult FreeRenderTargetTextures(int cameraId);

		MikanAPIResult WriteColorRenderTargetTexture(int cameraId, IntPtr apiTexturePtr);
		MikanAPIResult WriteDepthRenderTargetTexture(int cameraId, IntPtr apiTexturePtr, float zNear, float zFar);
		MikanAPIResult WriteShadowRenderTargetTexture(int cameraId, IntPtr apiTexturePtr);
	}

	// Implemented by a writer whose library wants the client's graphics device handed to it, as
	// the native shared texture library does for Direct3D and Vulkan. A managed writer that
	// borrows its engine's device has no use for these and does not implement it.
	public interface IMikanGraphicsDeviceInterfaces
	{
		MikanAPIResult SetGraphicsDeviceInterface(MikanClientGraphicsApi api, IntPtr graphicsDeviceInterface);
		MikanAPIResult GetGraphicsDeviceInterface(MikanClientGraphicsApi api, out IntPtr outGraphicsDeviceInterface);
		MikanAPIResult SetGraphicsCommandQueueInterface(MikanClientGraphicsApi api, IntPtr graphicsCommandQueueInterface);
		MikanAPIResult GetGraphicsCommandQueueInterface(MikanClientGraphicsApi api, out IntPtr outGraphicsCommandQueueInterface);
		IntPtr GetCameraPackDepthTextureResourcePtr(int cameraId);
	}
}
