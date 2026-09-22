// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class AllocateCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;
		public MikanRenderTargetDescriptor descriptor;

		public AllocateCameraRenderTargetTextures()
		{
			requestTypeName = "AllocateCameraRenderTargetTextures";
		}
	};

	public class FreeCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;

		public FreeCameraRenderTargetTextures()
		{
			requestTypeName = "FreeCameraRenderTargetTextures";
		}
	};

	public class GetCameraProperties : MikanRequest
	{
		public int camera_id;

		public GetCameraProperties()
		{
			requestTypeName = "GetCameraProperties";
		}
	};

	public class MikanCameraPropertiesResponse : MikanResponse
	{
		public int camera_id;
		public bool compositor_running;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;

		public MikanCameraPropertiesResponse()
		{
			responseTypeName = "MikanCameraPropertiesResponse";
		}
	};

	public class PublishCameraRenderTargetTextures : MikanRequest
	{
		public int camera_id;
		public long frame_index;

		public PublishCameraRenderTargetTextures()
		{
			requestTypeName = "PublishCameraRenderTargetTextures";
		}
	};

	public class WriteCameraColorRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_color_texture_ptr;

		public WriteCameraColorRenderTargetTexture()
		{
			requestTypeName = "WriteCameraColorRenderTargetTexture";
		}
	};

	public class WriteCameraDepthRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_depth_texture_ptr;
		public float z_near;
		public float z_far;

		public WriteCameraDepthRenderTargetTexture()
		{
			requestTypeName = "WriteCameraDepthRenderTargetTexture";
		}
	};

	public class WriteCameraShadowRenderTargetTexture : MikanRequest
	{
		public int camera_id;
		public IntPtr api_shadow_texture_ptr;

		public WriteCameraShadowRenderTargetTexture()
		{
			requestTypeName = "WriteCameraShadowRenderTargetTexture";
		}
	};

}
