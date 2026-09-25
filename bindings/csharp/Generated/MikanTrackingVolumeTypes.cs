// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanTrackingRuntime
	{
		INVALID= -1,
		SteamVR= 0,
	};

	public enum MikanTrackingSpace
	{
		INVALID= -1,
		StageSpace= 0,
		VRSpace= 1,
	};

	public enum MikanTrackingVolumeType
	{
		INVALID= -1,
		marker= 0,
		vr= 1,
	};

	public class MikanMarkerTrackingVolumeComponentValues : MikanTrackingVolumeComponentValues
	{
	};

	public class MikanTrackingVolumeComponentValues : MikanComponentValues
	{
		[MikanFieldOrder(0)] public int origin_marker_id;
	};

	public class MikanVRTrackingVolumeComponentValues : MikanTrackingVolumeComponentValues
	{
		[MikanFieldOrder(0)] public MikanTrackingRuntime tracking_runtime;
		[MikanFieldOrder(1)] public int charuco_mount_id;
		[MikanFieldOrder(2)] public MikanVector3f charuco_mount_offset_mm;
		[MikanFieldOrder(3)] public int utility_marker_id;
		[MikanFieldOrder(4)] public List<int> tracking_mount_ids;
		[MikanFieldOrder(5)] public MikanMatrix4f vr_space_to_stage_space;
		[MikanFieldOrder(6)] public MikanTrackingSpace display_tracking_space;
	};

}
