// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCameraNewFrameEvent : MikanEvent
	{
		public int camera_id;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2i aux_pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;
		public long frame;

		public MikanCameraNewFrameEvent()
		{
			eventTypeName = "MikanCameraNewFrameEvent";
		}
	};

	public class MikanCameraNewPropertiesEvent : MikanEvent
	{
		public int camera_id;
		public MikanVector3f camera_forward;
		public MikanVector3f camera_up;
		public MikanVector3f camera_position;
		public MikanVector2i pixel_size;
		public MikanVector2i aux_pixel_size;
		public MikanVector2d focal_length;
		public MikanVector2d principal_point;
		public MikanVector2d z_bounds;

		public MikanCameraNewPropertiesEvent()
		{
			eventTypeName = "MikanCameraNewPropertiesEvent";
		}
	};

}
