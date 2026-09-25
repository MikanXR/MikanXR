using System;
using System.Threading.Tasks;

namespace MikanXR
{
	public class MikanRenderTargetAPI
	{
		private IntPtr _mikanContext;
		private MikanRequestManager _requestManager;

		public MikanRenderTargetAPI(MikanRequestManager requestManager)
		{
			_requestManager = requestManager;
		}

		public void Initialize(IntPtr mikanContext)
		{
			_mikanContext= mikanContext;
		}

		public MikanAPIResult SetGraphicsDeviceInterface(
			MikanClientGraphicsApi api,
			IntPtr graphicsDeviceInterface)
		{
			int result = 
				MikanCoreNative.Mikan_SetGraphicsDeviceInterface(
					_mikanContext, api, graphicsDeviceInterface);
			return (MikanAPIResult)result;
		}

		public MikanAPIResult GetGraphicsDeviceInterface(
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsDeviceInterface)
		{
			int result =
				MikanCoreNative.Mikan_GetGraphicsDeviceInterface(
					_mikanContext, api, out outGraphicsDeviceInterface);
			return (MikanAPIResult)result;
		}

		public MikanAPIResult SetGraphicsCommandQueueInterface(
			MikanClientGraphicsApi api,
			IntPtr graphicsCommandQueueInterface)
		{
			int result =
				MikanCoreNative.Mikan_SetGraphicsCommandQueueInterface(
					_mikanContext, api, graphicsCommandQueueInterface);
			return (MikanAPIResult)result;
		}

		public MikanAPIResult GetGraphicsCommandQueueInterface(
			MikanClientGraphicsApi api,
			out IntPtr outGraphicsCommandQueueInterface)
		{
			int result =
				MikanCoreNative.Mikan_GetGraphicsCommandQueueInterface(
					_mikanContext, api, out outGraphicsCommandQueueInterface);
			return (MikanAPIResult)result;
		}

		public IntPtr GetCameraPackDepthTextureResourcePtr(int cameraId)
		{
			return MikanCoreNative.Mikan_GetCameraPackDepthTextureResourcePtr(_mikanContext, cameraId);
		}

		public MikanResponseFuture TryProcessRequest(MikanRequest request)
		{
			if (request is AllocateCameraRenderTargetTextures)
			{
				return RequestAllocateRenderTargetTextures(request);
			}
			else if (request is WriteCameraColorRenderTargetTexture)
			{
				return RequestWriteColorRenderTargetTexture(request);
			}
			else if (request is WriteCameraDepthRenderTargetTexture)
			{
				return RequestWriteDepthRenderTargetTexture(request);
			}
			else if (request is PublishCameraRenderTargetTextures) 
			{
				return RequestPublishRenderTargetTextures(request);
			}
			else if (request is FreeCameraRenderTargetTextures)
			{
				return RequestFreeRenderTargetTextures(request);
			}

			return new MikanResponseFuture();
		}

		private MikanResponseFuture RequestAllocateRenderTargetTextures(MikanRequest request)
		{
			var allocateRequest = request as AllocateCameraRenderTargetTextures;
			MikanRenderTargetDescriptor desiredDescriptor= allocateRequest.descriptor;
			MikanRenderTargetDescriptor_Native desiredDescriptor_Native=
				new MikanRenderTargetDescriptor_Native() {
					color_buffer_type= desiredDescriptor.color_buffer_type,
					depth_buffer_type= desiredDescriptor.depth_buffer_type,
					shadow_buffer_type= desiredDescriptor.shadow_buffer_type,
					width= desiredDescriptor.width,
					height= desiredDescriptor.height,
					aux_width= desiredDescriptor.aux_width,
					aux_height= desiredDescriptor.aux_height,
					graphicsAPI= desiredDescriptor.graphicsAPI
				};

			MikanAPIResult result =
				(MikanAPIResult)MikanCoreNative.Mikan_AllocateCameraRenderTargetTextures(
					_mikanContext, allocateRequest.camera_id, ref desiredDescriptor_Native);
			if (result == MikanAPIResult.Success)
			{
				// Actual descriptor might differ from desired descriptor based on render target writer's capabilities
				MikanRenderTargetDescriptor_Native actualDescriptor_Native;
				result= (MikanAPIResult)MikanCoreNative.Mikan_GetCameraRenderTargetDescriptor(
					_mikanContext, allocateRequest.camera_id, out actualDescriptor_Native);
				if (result == MikanAPIResult.Success)
				{
					// Replace the descriptor in the request with the actual descriptor params 
					// that were used to allocate the render target textures
					allocateRequest.descriptor =
						new MikanRenderTargetDescriptor()
						{
							color_buffer_type = actualDescriptor_Native.color_buffer_type,
							depth_buffer_type = actualDescriptor_Native.depth_buffer_type,
							shadow_buffer_type = actualDescriptor_Native.shadow_buffer_type,
							width = actualDescriptor_Native.width,
							height = actualDescriptor_Native.height,
							aux_width = actualDescriptor_Native.aux_width,
							aux_height = actualDescriptor_Native.aux_height,
							graphicsAPI = actualDescriptor_Native.graphicsAPI
						};

					return _requestManager.SendRequest(allocateRequest);
				}
			}

			return _requestManager.AddResponseHandler(-1, MikanAPIResult.RequestFailed);
		}

		private MikanResponseFuture RequestWriteColorRenderTargetTexture(MikanRequest request)
		{
			var writeRequest = request as WriteCameraColorRenderTargetTexture;
			IntPtr apiColorTexturePtr= writeRequest.api_color_texture_ptr;

			MikanAPIResult result =
				(MikanAPIResult)MikanCoreNative.Mikan_WriteCameraColorRenderTargetTexture(
					_mikanContext, writeRequest.camera_id, apiColorTexturePtr);

			return new MikanResponseFuture(result);
		}

		private MikanResponseFuture RequestWriteDepthRenderTargetTexture(MikanRequest request)
		{
			var writeRequest = request as WriteCameraDepthRenderTargetTexture;
			IntPtr apiDepthTexturePtr= writeRequest.api_depth_texture_ptr;
			float zNear= writeRequest.z_near;
			float zFar= writeRequest.z_far;

			MikanAPIResult result =
				(MikanAPIResult)MikanCoreNative.Mikan_WriteCameraDepthRenderTargetTexture(
					_mikanContext, writeRequest.camera_id, apiDepthTexturePtr, zNear, zFar);

			return new MikanResponseFuture(result);
		}

		private MikanResponseFuture RequestPublishRenderTargetTextures(MikanRequest request)
		{
			return _requestManager.SendRequest(request);
		}

		private MikanResponseFuture RequestFreeRenderTargetTextures(MikanRequest request)
		{
			// Free any locally allocated resources
			var freeRequest = request as FreeCameraRenderTargetTextures;
			MikanCoreNative.Mikan_FreeCameraRenderTargetTextures(_mikanContext, freeRequest.camera_id);

			// Tell the server to free the render target resources too
			return _requestManager.SendRequest(request);
		}
	}
}