using System;

namespace MikanXR
{
	// Routes the render target requests between the installed IMikanRenderTargetWriter and the
	// editor. Allocate and free reach both, since the editor opens and closes its readers on
	// them; the writes are local to the writer, and publish is the editor's alone.
	public class MikanRenderTargetAPI
	{
		private readonly MikanRequestManager _requestManager;
		private IMikanRenderTargetWriter _writer;

		public MikanRenderTargetAPI(MikanRequestManager requestManager, IMikanRenderTargetWriter writer)
		{
			_requestManager= requestManager;
			_writer= writer;
		}

		public IMikanRenderTargetWriter Writer
		{
			get { return _writer; }
			set { _writer= value; }
		}

		// Returns an invalid future for a request that is not a render target request, which the
		// caller then sends over the wire as it would any other
		public MikanResponseFuture TryProcessRequest(MikanRequest request)
		{
			if (request is AllocateCameraRenderTargetTextures allocateRequest)
			{
				return RequestAllocateRenderTargetTextures(allocateRequest);
			}
			else if (request is WriteCameraColorRenderTargetTexture writeColorRequest)
			{
				return new MikanResponseFuture(
					_writer != null
					? _writer.WriteColorRenderTargetTexture(writeColorRequest.camera_id, writeColorRequest.api_color_texture_ptr)
					: MikanAPIResult.RequestFailed);
			}
			else if (request is WriteCameraDepthRenderTargetTexture writeDepthRequest)
			{
				return new MikanResponseFuture(
					_writer != null
					? _writer.WriteDepthRenderTargetTexture(
						writeDepthRequest.camera_id, writeDepthRequest.api_depth_texture_ptr,
						writeDepthRequest.z_near, writeDepthRequest.z_far)
					: MikanAPIResult.RequestFailed);
			}
			else if (request is WriteCameraShadowRenderTargetTexture writeShadowRequest)
			{
				return new MikanResponseFuture(
					_writer != null
					? _writer.WriteShadowRenderTargetTexture(writeShadowRequest.camera_id, writeShadowRequest.api_shadow_texture_ptr)
					: MikanAPIResult.RequestFailed);
			}
			else if (request is PublishCameraRenderTargetTextures)
			{
				return _requestManager.SendRequest(request);
			}
			else if (request is FreeCameraRenderTargetTextures freeRequest)
			{
				// Free the local senders first, then tell the editor to drop its readers
				_writer?.FreeRenderTargetTextures(freeRequest.camera_id);

				return _requestManager.SendRequest(request);
			}

			return new MikanResponseFuture();
		}

		private MikanResponseFuture RequestAllocateRenderTargetTextures(AllocateCameraRenderTargetTextures allocateRequest)
		{
			if (_writer == null)
			{
				return _requestManager.AddResponseHandler(-1, MikanAPIResult.RequestFailed);
			}

			MikanRenderTargetDescriptor descriptor= allocateRequest.descriptor;
			MikanAPIResult result= _writer.AllocateRenderTargetTextures(allocateRequest.camera_id, ref descriptor);
			if (result != MikanAPIResult.Success)
			{
				return _requestManager.AddResponseHandler(-1, result);
			}

			// The editor is told what was actually allocated
			allocateRequest.descriptor= descriptor;

			return _requestManager.SendRequest(allocateRequest);
		}
	}
}
