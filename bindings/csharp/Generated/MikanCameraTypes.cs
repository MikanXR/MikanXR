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

	public enum MikanClientMaxBufferDimension
	{
		Dim1024= 0,
		Dim2048= 1,
		Dim4096= 2,
		Dim8192= 3,
	};

	public class MikanCameraComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public int stage_id;
		[MikanFieldOrder(1)] public int tracking_mount_id;
		[MikanFieldOrder(2)] public int video_source_id;
		[MikanFieldOrder(3)] public int light_environment_id;
		[MikanFieldOrder(4)] public int tracking_frame_delay;
		[MikanFieldOrder(5)] public MikanCameraFrameSyncMode frame_sync_mode;
		[MikanFieldOrder(6)] public bool pose_driven_per_frame;
		[MikanFieldOrder(7)] public float client_color_render_scale;
		[MikanFieldOrder(8)] public float client_aux_render_scale;
		[MikanFieldOrder(9)] public MikanClientMaxBufferDimension client_max_buffer_dimension;
		[MikanFieldOrder(10)] public MikanQuatd aperture_orientation_offset;
		[MikanFieldOrder(11)] public MikanVector3d aperture_position_offset;
		[MikanFieldOrder(12)] public bool has_valid_aperture_offset;
	};

}
