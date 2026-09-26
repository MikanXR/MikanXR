// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCameraNewFrameEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public MikanVector3f camera_forward;
		[MikanFieldOrder(2)] public MikanVector3f camera_up;
		[MikanFieldOrder(3)] public MikanVector3f camera_position;
		[MikanFieldOrder(4)] public MikanVector2i pixel_size;
		[MikanFieldOrder(5)] public MikanVector2i aux_pixel_size;
		[MikanFieldOrder(6)] public MikanVector2d focal_length;
		[MikanFieldOrder(7)] public MikanVector2d principal_point;
		[MikanFieldOrder(8)] public MikanVector2d z_bounds;
		[MikanFieldOrder(9)] public long frame;

		public MikanCameraNewFrameEvent()
		{
			eventTypeName = "MikanCameraNewFrameEvent";
		}
	};

	public class MikanCameraNewPropertiesEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public int camera_id;
		[MikanFieldOrder(1)] public MikanVector3f camera_forward;
		[MikanFieldOrder(2)] public MikanVector3f camera_up;
		[MikanFieldOrder(3)] public MikanVector3f camera_position;
		[MikanFieldOrder(4)] public MikanVector2i pixel_size;
		[MikanFieldOrder(5)] public MikanVector2i aux_pixel_size;
		[MikanFieldOrder(6)] public MikanVector2d focal_length;
		[MikanFieldOrder(7)] public MikanVector2d principal_point;
		[MikanFieldOrder(8)] public MikanVector2d z_bounds;

		public MikanCameraNewPropertiesEvent()
		{
			eventTypeName = "MikanCameraNewPropertiesEvent";
		}
	};

}
