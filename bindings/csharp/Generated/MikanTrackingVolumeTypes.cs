// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanTrackingRuntime
	{
		INVALID= -1,
		SteamVR= 0,
	};

	internal static class MikanTrackingRuntimeJson
	{
		public static JToken ToJson(MikanTrackingRuntime value)
		{
			switch (value)
			{
				case MikanTrackingRuntime.INVALID: return MikanJson.WriteString("INVALID");
				case MikanTrackingRuntime.SteamVR: return MikanJson.WriteString("SteamVR");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanTrackingRuntime has no wire name for " + (long)value);
		}

		public static MikanTrackingRuntime FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanTrackingRuntime)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanTrackingRuntime.INVALID;
				case "SteamVR": return MikanTrackingRuntime.SteamVR;
			}
			throw new Exception("MikanTrackingRuntime has no value named " + name);
		}
	};

	public enum MikanTrackingSpace
	{
		INVALID= -1,
		StageSpace= 0,
		VRSpace= 1,
	};

	internal static class MikanTrackingSpaceJson
	{
		public static JToken ToJson(MikanTrackingSpace value)
		{
			switch (value)
			{
				case MikanTrackingSpace.INVALID: return MikanJson.WriteString("INVALID");
				case MikanTrackingSpace.StageSpace: return MikanJson.WriteString("StageSpace");
				case MikanTrackingSpace.VRSpace: return MikanJson.WriteString("VRSpace");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanTrackingSpace has no wire name for " + (long)value);
		}

		public static MikanTrackingSpace FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanTrackingSpace)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanTrackingSpace.INVALID;
				case "StageSpace": return MikanTrackingSpace.StageSpace;
				case "VRSpace": return MikanTrackingSpace.VRSpace;
			}
			throw new Exception("MikanTrackingSpace has no value named " + name);
		}
	};

	public enum MikanTrackingVolumeType
	{
		INVALID= -1,
		marker= 0,
		vr= 1,
	};

	internal static class MikanTrackingVolumeTypeJson
	{
		public static JToken ToJson(MikanTrackingVolumeType value)
		{
			switch (value)
			{
				case MikanTrackingVolumeType.INVALID: return MikanJson.WriteString("INVALID");
				case MikanTrackingVolumeType.marker: return MikanJson.WriteString("marker");
				case MikanTrackingVolumeType.vr: return MikanJson.WriteString("vr");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanTrackingVolumeType has no wire name for " + (long)value);
		}

		public static MikanTrackingVolumeType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanTrackingVolumeType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanTrackingVolumeType.INVALID;
				case "marker": return MikanTrackingVolumeType.marker;
				case "vr": return MikanTrackingVolumeType.vr;
			}
			throw new Exception("MikanTrackingVolumeType has no value named " + name);
		}
	};

	public class MikanMarkerTrackingVolumeComponentValues : MikanTrackingVolumeComponentValues
	{
		public const string k_componentClassName = "MarkerTrackingVolumeComponent";
		public const string k_ownerSystemName = "MarkerTrackingVolumeSystem";


		public override string RuntimeClassName => "MikanMarkerTrackingVolumeComponentValues";
	};

	public class MikanTrackingVolumeComponentValues : MikanComponentValues
	{
		public int origin_marker_id;

		public override string RuntimeClassName => "MikanTrackingVolumeComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("origin_marker_id", MikanJson.WriteInt(this.origin_marker_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.origin_marker_id = MikanJson.ReadInt(MikanJson.Field(json, "origin_marker_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "origin_marker_id": value = this.origin_marker_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "origin_marker_id": return MikanJson.TryAssign(value, ref this.origin_marker_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVRTrackingVolumeComponentValues : MikanTrackingVolumeComponentValues
	{
		public const string k_componentClassName = "VRTrackingVolumeComponent";
		public const string k_ownerSystemName = "VRTrackingVolumeSystem";

		public MikanTrackingRuntime tracking_runtime;
		public int charuco_mount_id;
		public MikanVector3f charuco_mount_offset_mm;
		public int utility_marker_id;
		public List<int> tracking_mount_ids;
		public MikanMatrix4f vr_space_to_stage_space;
		public MikanTrackingSpace display_tracking_space;

		public override string RuntimeClassName => "MikanVRTrackingVolumeComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("tracking_runtime", MikanTrackingRuntimeJson.ToJson(this.tracking_runtime));
			json.Add("charuco_mount_id", MikanJson.WriteInt(this.charuco_mount_id));
			json.Add("charuco_mount_offset_mm", MikanJson.WriteObject(this.charuco_mount_offset_mm));
			json.Add("utility_marker_id", MikanJson.WriteInt(this.utility_marker_id));
			json.Add("tracking_mount_ids", MikanJson.WriteList(this.tracking_mount_ids, e0 => MikanJson.WriteInt(e0)));
			json.Add("vr_space_to_stage_space", MikanJson.WriteObject(this.vr_space_to_stage_space));
			json.Add("display_tracking_space", MikanTrackingSpaceJson.ToJson(this.display_tracking_space));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.tracking_runtime = MikanTrackingRuntimeJson.FromJson(MikanJson.Field(json, "tracking_runtime"));
			this.charuco_mount_id = MikanJson.ReadInt(MikanJson.Field(json, "charuco_mount_id"));
			this.charuco_mount_offset_mm = MikanJson.ReadObject(MikanJson.Field(json, "charuco_mount_offset_mm"), this.charuco_mount_offset_mm ?? new MikanVector3f());
			this.utility_marker_id = MikanJson.ReadInt(MikanJson.Field(json, "utility_marker_id"));
			this.tracking_mount_ids = MikanJson.ReadList(MikanJson.Field(json, "tracking_mount_ids"), this.tracking_mount_ids, e0 => MikanJson.ReadInt(e0));
			this.vr_space_to_stage_space = MikanJson.ReadObject(MikanJson.Field(json, "vr_space_to_stage_space"), this.vr_space_to_stage_space ?? new MikanMatrix4f());
			this.display_tracking_space = MikanTrackingSpaceJson.FromJson(MikanJson.Field(json, "display_tracking_space"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "tracking_runtime": value = this.tracking_runtime; return true;
				case "charuco_mount_id": value = this.charuco_mount_id; return true;
				case "charuco_mount_offset_mm": value = this.charuco_mount_offset_mm; return true;
				case "utility_marker_id": value = this.utility_marker_id; return true;
				case "tracking_mount_ids": value = this.tracking_mount_ids; return true;
				case "vr_space_to_stage_space": value = this.vr_space_to_stage_space; return true;
				case "display_tracking_space": value = this.display_tracking_space; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "tracking_runtime": return MikanJson.TryAssign(value, ref this.tracking_runtime);
				case "charuco_mount_id": return MikanJson.TryAssign(value, ref this.charuco_mount_id);
				case "charuco_mount_offset_mm": return MikanJson.TryAssign(value, ref this.charuco_mount_offset_mm);
				case "utility_marker_id": return MikanJson.TryAssign(value, ref this.utility_marker_id);
				case "tracking_mount_ids": return MikanJson.TryAssign(value, ref this.tracking_mount_ids);
				case "vr_space_to_stage_space": return MikanJson.TryAssign(value, ref this.vr_space_to_stage_space);
				case "display_tracking_space": return MikanJson.TryAssign(value, ref this.display_tracking_space);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
