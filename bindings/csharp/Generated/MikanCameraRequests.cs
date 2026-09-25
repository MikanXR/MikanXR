// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class AllocateCameraRenderTargetTextures : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public MikanRenderTargetDescriptor descriptor;

		public AllocateCameraRenderTargetTextures()
		{
			requestTypeName = "AllocateCameraRenderTargetTextures";
		}
	};

	public class FreeCameraRenderTargetTextures : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;

		public FreeCameraRenderTargetTextures()
		{
			requestTypeName = "FreeCameraRenderTargetTextures";
		}
	};

	public class GetCameraProperties : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;

		public GetCameraProperties()
		{
			requestTypeName = "GetCameraProperties";
		}
	};

	public class MikanCameraPropertiesResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public bool compositor_running;
		[MikanFieldOrder(2)] public MikanVector3f camera_forward;
		[MikanFieldOrder(3)] public MikanVector3f camera_up;
		[MikanFieldOrder(4)] public MikanVector3f camera_position;
		[MikanFieldOrder(5)] public MikanVector2i pixel_size;
		[MikanFieldOrder(6)] public MikanVector2i aux_pixel_size;
		[MikanFieldOrder(7)] public MikanVector2d focal_length;
		[MikanFieldOrder(8)] public MikanVector2d principal_point;
		[MikanFieldOrder(9)] public MikanVector2d z_bounds;

		public MikanCameraPropertiesResponse()
		{
			responseTypeName = "MikanCameraPropertiesResponse";
		}
	};

	public class PublishCameraRenderTargetTextures : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public long frame_index;

		public PublishCameraRenderTargetTextures()
		{
			requestTypeName = "PublishCameraRenderTargetTextures";
		}
	};

	public class WriteCameraColorRenderTargetTexture : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public IntPtr api_color_texture_ptr;

		public WriteCameraColorRenderTargetTexture()
		{
			requestTypeName = "WriteCameraColorRenderTargetTexture";
		}
	};

	public class WriteCameraDepthRenderTargetTexture : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public IntPtr api_depth_texture_ptr;
		[MikanFieldOrder(2)] public float z_near;
		[MikanFieldOrder(3)] public float z_far;

		public WriteCameraDepthRenderTargetTexture()
		{
			requestTypeName = "WriteCameraDepthRenderTargetTexture";
		}
	};

	public class WriteCameraShadowRenderTargetTexture : MikanRequest
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public IntPtr api_shadow_texture_ptr;

		public WriteCameraShadowRenderTargetTexture()
		{
			requestTypeName = "WriteCameraShadowRenderTargetTexture";
		}
	};

}
