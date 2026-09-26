// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetVideoSourceIntrinsics : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;

		public GetVideoSourceIntrinsics()
		{
			requestTypeName = "GetVideoSourceIntrinsics";
		}
	};

	public class GetVideoSourceMode : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;

		public GetVideoSourceMode()
		{
			requestTypeName = "GetVideoSourceMode";
		}
	};

	public class MikanVideoSourceIntrinsicsResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanVideoSourceIntrinsics intrinsics;

		public MikanVideoSourceIntrinsicsResponse()
		{
			responseTypeName = "MikanVideoSourceIntrinsicsResponse";
		}
	};

	public class MikanVideoSourceModeResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanVideoSourceType video_source_type;
		[MikanFieldOrder(1)] public string video_source_api;
		[MikanFieldOrder(2)] public string device_path;
		[MikanFieldOrder(3)] public string video_mode_name;
		[MikanFieldOrder(4)] public int resolution_x;
		[MikanFieldOrder(5)] public int resolution_y;
		[MikanFieldOrder(6)] public float frame_rate;

		public MikanVideoSourceModeResponse()
		{
			responseTypeName = "MikanVideoSourceModeResponse";
		}
	};

	public class SetUSBVideoSourceDevice : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;
		[MikanFieldOrder(1)] public string device_path;

		public SetUSBVideoSourceDevice()
		{
			requestTypeName = "SetUSBVideoSourceDevice";
		}
	};

	public class SetUSBVideoSourceFormat : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;
		[MikanFieldOrder(1)] public string format;

		public SetUSBVideoSourceFormat()
		{
			requestTypeName = "SetUSBVideoSourceFormat";
		}
	};

	public class SetUSBVideoSourceFrameRate : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;
		[MikanFieldOrder(1)] public string frame_rate;

		public SetUSBVideoSourceFrameRate()
		{
			requestTypeName = "SetUSBVideoSourceFrameRate";
		}
	};

	public class SetUSBVideoSourceResolution : MikanRequest
	{
		[MikanFieldOrder(0)] public int video_source_id;
		[MikanFieldOrder(1)] public string resolution;

		public SetUSBVideoSourceResolution()
		{
			requestTypeName = "SetUSBVideoSourceResolution";
		}
	};

}
