// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanCameraFrameSyncMode
	{
		Auto= 0,
		VideoFrame= 1,
		FreeRunning= 2,
	};

	public class MikanCameraComponentValues : MikanTransformComponentValues
	{
		public int stage_id;
		public int tracking_mount_id;
		public int video_source_id;
		public int light_environment_id;
		public int tracking_frame_delay;
		public MikanCameraFrameSyncMode frame_sync_mode;
		public bool pose_driven_per_frame;
		public float client_color_render_scale;
		public float client_aux_render_scale;
		public MikanQuatd aperture_orientation_offset;
		public MikanVector3d aperture_position_offset;
		public bool has_valid_aperture_offset;
	};

}
