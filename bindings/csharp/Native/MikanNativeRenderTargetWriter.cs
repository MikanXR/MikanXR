using System;

namespace MikanXR
{
	// The shared texture writer inside MikanClientCore.dll, reached through the native
	// transport's context. The client hands it its graphics device and its API texture
	// pointers, and the native library owns the Spout senders.
	public class MikanNativeRenderTargetWriter : IMikanRenderTargetWriter, IMikanGraphicsDeviceInterfaces
	{
		private readonly MikanNativeTransport _transport;

		public MikanNativeRenderTargetWriter(MikanNativeTransport transport)
		{
			_transport= transport;
		}

		private IntPtr Context => _transport.Context;

		public MikanAPIResult SetGraphicsDeviceInterface(MikanClientGraphicsApi api, IntPtr graphicsDeviceInterface)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_SetGraphicsDeviceInterface(
				Context, api, graphicsDeviceInterface);
		}

		public MikanAPIResult GetGraphicsDeviceInterface(MikanClientGraphicsApi api, out IntPtr outGraphicsDeviceInterface)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_GetGraphicsDeviceInterface(
				Context, api, out outGraphicsDeviceInterface);
		}

		public MikanAPIResult SetGraphicsCommandQueueInterface(MikanClientGraphicsApi api, IntPtr graphicsCommandQueueInterface)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_SetGraphicsCommandQueueInterface(
				Context, api, graphicsCommandQueueInterface);
		}

		public MikanAPIResult GetGraphicsCommandQueueInterface(MikanClientGraphicsApi api, out IntPtr outGraphicsCommandQueueInterface)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_GetGraphicsCommandQueueInterface(
				Context, api, out outGraphicsCommandQueueInterface);
		}

		public IntPtr GetCameraPackDepthTextureResourcePtr(int cameraId)
		{
			return MikanCoreNative.Mikan_GetCameraPackDepthTextureResourcePtr(Context, cameraId);
		}

		public MikanAPIResult AllocateRenderTargetTextures(int cameraId, ref MikanRenderTargetDescriptor descriptor)
		{
			MikanRenderTargetDescriptor_Native desiredDescriptor_Native=
				new MikanRenderTargetDescriptor_Native() {
					color_buffer_type= descriptor.color_buffer_type,
					depth_buffer_type= descriptor.depth_buffer_type,
					shadow_buffer_type= descriptor.shadow_buffer_type,
					width= descriptor.width,
					height= descriptor.height,
					aux_width= descriptor.aux_width,
					aux_height= descriptor.aux_height,
					graphicsAPI= descriptor.graphicsAPI
				};

			MikanAPIResult result=
				(MikanAPIResult)MikanCoreNative.Mikan_AllocateCameraRenderTargetTextures(
					Context, cameraId, ref desiredDescriptor_Native);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			// The actual descriptor can differ from the desired one with the writer's capabilities,
			// and the actual one is what the editor has to be told
			result= (MikanAPIResult)MikanCoreNative.Mikan_GetCameraRenderTargetDescriptor(
				Context, cameraId, out MikanRenderTargetDescriptor_Native actualDescriptor_Native);
			if (result != MikanAPIResult.Success)
			{
				return result;
			}

			descriptor= new MikanRenderTargetDescriptor()
			{
				color_buffer_type= actualDescriptor_Native.color_buffer_type,
				depth_buffer_type= actualDescriptor_Native.depth_buffer_type,
				shadow_buffer_type= actualDescriptor_Native.shadow_buffer_type,
				width= actualDescriptor_Native.width,
				height= actualDescriptor_Native.height,
				aux_width= actualDescriptor_Native.aux_width,
				aux_height= actualDescriptor_Native.aux_height,
				graphicsAPI= actualDescriptor_Native.graphicsAPI
			};

			return MikanAPIResult.Success;
		}

		public MikanAPIResult FreeRenderTargetTextures(int cameraId)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_FreeCameraRenderTargetTextures(Context, cameraId);
		}

		public MikanAPIResult WriteColorRenderTargetTexture(int cameraId, IntPtr apiTexturePtr)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_WriteCameraColorRenderTargetTexture(
				Context, cameraId, apiTexturePtr);
		}

		public MikanAPIResult WriteDepthRenderTargetTexture(int cameraId, IntPtr apiTexturePtr, float zNear, float zFar)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_WriteCameraDepthRenderTargetTexture(
				Context, cameraId, apiTexturePtr, zNear, zFar);
		}

		public MikanAPIResult WriteShadowRenderTargetTexture(int cameraId, IntPtr apiTexturePtr)
		{
			return (MikanAPIResult)MikanCoreNative.Mikan_WriteCameraShadowRenderTargetTexture(
				Context, cameraId, apiTexturePtr);
		}
	}
}
