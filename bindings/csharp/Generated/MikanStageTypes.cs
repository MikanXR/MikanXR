// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanStageTrackingVolume
	{
		StaticMarker= 0,
		SteamVR= 1,
	};

	internal static class MikanStageTrackingVolumeJson
	{
		public static JToken ToJson(MikanStageTrackingVolume value)
		{
			switch (value)
			{
				case MikanStageTrackingVolume.StaticMarker: return MikanJson.WriteString("StaticMarker");
				case MikanStageTrackingVolume.SteamVR: return MikanJson.WriteString("SteamVR");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanStageTrackingVolume has no wire name for " + (long)value);
		}

		public static MikanStageTrackingVolume FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanStageTrackingVolume)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "StaticMarker": return MikanStageTrackingVolume.StaticMarker;
				case "SteamVR": return MikanStageTrackingVolume.SteamVR;
			}
			throw new Exception("MikanStageTrackingVolume has no value named " + name);
		}
	};

	public class MikanStageComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "StageComponent";
		public const string k_ownerSystemName = "StageObjectSystem";

		public int tracking_volume_id;
		public MikanVector3f stage_bounds_min;
		public MikanVector3f stage_bounds_max;

		public override string RuntimeClassName => "MikanStageComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("tracking_volume_id", MikanJson.WriteInt(this.tracking_volume_id));
			json.Add("stage_bounds_min", MikanJson.WriteObject(this.stage_bounds_min));
			json.Add("stage_bounds_max", MikanJson.WriteObject(this.stage_bounds_max));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.tracking_volume_id = MikanJson.ReadInt(MikanJson.Field(json, "tracking_volume_id"));
			this.stage_bounds_min = MikanJson.ReadObject(MikanJson.Field(json, "stage_bounds_min"), this.stage_bounds_min ?? new MikanVector3f());
			this.stage_bounds_max = MikanJson.ReadObject(MikanJson.Field(json, "stage_bounds_max"), this.stage_bounds_max ?? new MikanVector3f());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "tracking_volume_id": value = this.tracking_volume_id; return true;
				case "stage_bounds_min": value = this.stage_bounds_min; return true;
				case "stage_bounds_max": value = this.stage_bounds_max; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "tracking_volume_id": return MikanJson.TryAssign(value, ref this.tracking_volume_id);
				case "stage_bounds_min": return MikanJson.TryAssign(value, ref this.stage_bounds_min);
				case "stage_bounds_max": return MikanJson.TryAssign(value, ref this.stage_bounds_max);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
