using System;
using MikanXR;

namespace Mikan
{
	public abstract class TestCameraRenderTarget
	{
		protected TestGraphicsContext _ownerContext;
        protected int _cameraId;

		protected int _width = 0;
		protected int _height = 0;

		// The size a reallocation last failed on, so it is not attempted again every frame
		private int _failedAllocWidth = 0;
		private int _failedAllocHeight = 0;
		protected float _zNear = 0.1f;
		protected float _zFar = 100.0f;
		protected bool _hasAllocatedRemoteTexture = false;
		protected bool _hasValidProjMatrix = false;
		protected bool _hasValidViewMatrix = false;

		protected long _lastReceivedFrameIndex = 0;

		public int CameraId => _cameraId;
		public int Width => _width;
		public int Height => _height;
		public float ZNear => _zNear;
		public float ZFar => _zFar;

		protected TestCameraRenderTarget(
			TestGraphicsContext ownerContext,
            int cameraId)
		{
			_ownerContext = ownerContext;
            _cameraId = cameraId;
		}

        public bool ProcessCameraNewFrameEvent(
            MikanAPI mikanAPI,
            MikanCameraNewFrameEvent newFrameEvent)
		{
			// Remember the frame index of the last frame we published
			_lastReceivedFrameIndex = newFrameEvent.frame;

			// Update the camera view matrix using the latest camera extrinsics
			UpdateCameraViewMatrix(newFrameEvent);

			// Update the camera perspective matrix using the latest camera intrinsics
			UpdateCameraProjectionMatrix(newFrameEvent);

			// Store z-bounds from the frame event
			_zNear = (float)newFrameEvent.z_bounds.x;
			_zFar = (float)newFrameEvent.z_bounds.y;

			// Recreate the render target if the texture size changed
			int newWidth = newFrameEvent.pixel_size.x;
			int newHeight = newFrameEvent.pixel_size.y;
			if (_width != newWidth || _height != newHeight)
			{
				// A failed reallocation tears the graphics resources back down, which puts
				// _width/_height back to 0 and makes the next frame event look like another size
				// change. Without this the same failing size is retried at frame rate, so give up
				// on a size that already failed and wait for the camera to ask for a different one.
				if (newWidth == _failedAllocWidth && newHeight == _failedAllocHeight)
				{
					return false;
				}

				if (ReallocateRenderTarget(mikanAPI, newWidth, newHeight))
				{
					_failedAllocWidth = 0;
					_failedAllocHeight = 0;

					Console.WriteLine($"INFO: Update frame size (camera_id: {_cameraId}, new size: {newWidth}x{newHeight}, frame: {_lastReceivedFrameIndex}).");
				}
				else
				{
					_failedAllocWidth = newWidth;
					_failedAllocHeight = newHeight;

					Console.WriteLine($"ERROR: Failed to update frame size (camera_id: {_cameraId}, new size: {newWidth}x{newHeight}, frame: {_lastReceivedFrameIndex}). Invalidating render target.");
					return false;
				}
			}

			// Bind the render target for rendering
			BindGraphicsAPIResource();

			// Render the scene from the PoV of this camera
			if (!_ownerContext.RenderToCameraTarget(this))
			{
				Console.WriteLine($"INFO: Failed to render camera frame (camera_id: {_cameraId}, frame: {_lastReceivedFrameIndex}).");
			}

			// Unbind the render target
			UnbindGraphicsAPIResource();

			if (_cameraId != -1)
			{
                // Write the color texture, if any, to the shared texture
                IntPtr colorTexturePtr = GetGraphicsApiColorTexturePtr();
                if (colorTexturePtr != IntPtr.Zero)
                {
                    mikanAPI.SendRequest(new WriteCameraColorRenderTargetTexture()
                    {
                        api_color_texture_ptr = colorTexturePtr,
                        camera_id = _cameraId
                    });
                }

                // Write the depth texture, if any, to the shared texture
                IntPtr depthTexturePtr = GetGraphicsApiDepthTexturePtr();
                if (depthTexturePtr != IntPtr.Zero)
                {
                    mikanAPI.SendRequest(new WriteCameraDepthRenderTargetTexture()
                    {
                        api_depth_texture_ptr = depthTexturePtr,
                        camera_id = _cameraId,
                        z_near = _zNear,
                        z_far = _zFar
                    });
                }

                // Publish the new video frame back to Mikan
                if (colorTexturePtr != IntPtr.Zero || depthTexturePtr != IntPtr.Zero)
                {
                    mikanAPI.SendRequest(new PublishCameraRenderTargetTextures()
                    {
                        camera_id = _cameraId,
                        frame_index = newFrameEvent.frame,
                    });
                }
            }

            return true;
		}

		public virtual void Dispose(MikanAPI mikanAPI)
		{
			// Camera matrices aren't valid until we receive the next frame
			_hasValidProjMatrix = false;
			_hasValidViewMatrix = false;

			// Clean up shared texture first
			FreeSharedTexture(mikanAPI);

			// Clean up locally allocated graphics resources
			FreeGraphicsAPIResources();
		}

		protected bool ReallocateRenderTarget(MikanAPI mikanAPI, int textureWidth, int textureHeight)
		{
			// Clean up anything we had
			Dispose(mikanAPI);

			// Create a new render texture to render frames to
			bool success = false;
			if (CreateGraphicsAPIResources(textureWidth, textureHeight))
			{
				Console.WriteLine($"INFO: Successfully created graphics texture resources (size: {textureWidth}x{textureHeight})");

				if (CreateSharedTexture(mikanAPI, textureWidth, textureHeight))
				{
					Console.WriteLine($"INFO: Successfully created shared texture (camera id: {_cameraId})");
					success = true;
				}
				else
				{
					// Clean back up the graphics resources, since there is no point to making them if we can't allocate the shared texture
					FreeGraphicsAPIResources();
				}
			}

			return success;
		}

		protected bool CreateSharedTexture(MikanAPI mikanAPI, int textureWidth, int textureHeight)
		{
            if (_cameraId == -1)
            {
                Console.WriteLine("Skipping shared texture allocation for offline camera");
                return true;
            }

            if (_hasAllocatedRemoteTexture)
			{
				Console.WriteLine("ERROR: Shared texture already allocated");
				return false;
			}

			// Tell Mikan to create a shared BGRA32 texture for us to render frames to
			var desc = new MikanRenderTargetDescriptor()
			{
				width = (uint)textureWidth,
				height = (uint)textureHeight,
				color_buffer_type = MikanColorBufferType.BGRA32,
				depth_buffer_type = MikanDepthBufferType.FLOAT_DEVICE_DEPTH,
				graphicsAPI = MikanClientGraphicsApi.Direct3D11
			};

			var response = mikanAPI.SendRequest(new AllocateCameraRenderTargetTextures()
			{
				camera_id = _cameraId,
                descriptor = desc
			}).FetchResponse();

			if (response.resultCode == MikanAPIResult.Success)
			{
				Console.WriteLine("INFO: Successfully allocated shared texture");
				_hasAllocatedRemoteTexture = true;
			}
			else
			{
				Console.WriteLine($"WARNING: Failed to allocate remote texture (error_code: {(int)response.resultCode})");
				return false;
			}

			return true;
		}

		protected void FreeSharedTexture(MikanAPI mikanAPI)
		{
			if (_hasAllocatedRemoteTexture && mikanAPI != null)
			{
				// Tell mikan to clean up the shared texture on its side first
				var response = mikanAPI.SendRequest(new FreeCameraRenderTargetTextures() { 
					camera_id = _cameraId
                }).FetchResponse();

				if (response.resultCode != MikanAPIResult.Success)
				{
					Console.WriteLine("WARNING: Failed to free remote texture");
				}

				_hasAllocatedRemoteTexture = false;
			}
		}

		// Abstract methods to be implemented by derived classes
		protected abstract bool CreateGraphicsAPIResources(int textureWidth, int textureHeight);
		protected abstract void BindGraphicsAPIResource();
		protected abstract void UnbindGraphicsAPIResource();
		protected abstract void FreeGraphicsAPIResources();
		protected abstract IntPtr GetGraphicsApiColorTexturePtr();
		protected abstract IntPtr GetGraphicsApiDepthTexturePtr();

		protected abstract void UpdateCameraViewMatrix(MikanCameraNewFrameEvent newFrameEvent);
		protected abstract void UpdateCameraProjectionMatrix(MikanCameraNewFrameEvent newFrameEvent);
	}
}
