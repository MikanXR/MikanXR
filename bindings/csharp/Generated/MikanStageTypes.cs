// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanStageTrackingVolume
	{
		StaticMarker= 0,
		SteamVR= 1,
	};

	public class MikanStageComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public int tracking_volume_id;
		[MikanFieldOrder(1)] public MikanVector3f stage_bounds_min;
		[MikanFieldOrder(2)] public MikanVector3f stage_bounds_max;
	};

}
