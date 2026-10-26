// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
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

	internal static class MikanCameraFrameSyncModeJson
	{
		public static JToken ToJson(MikanCameraFrameSyncMode value)
		{
			switch (value)
			{
				case MikanCameraFrameSyncMode.Auto: return MikanJson.WriteString("Auto");
				case MikanCameraFrameSyncMode.VideoFrame: return MikanJson.WriteString("VideoFrame");
				case MikanCameraFrameSyncMode.FreeRunning: return MikanJson.WriteString("FreeRunning");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanCameraFrameSyncMode has no wire name for " + (long)value);
		}

		public static MikanCameraFrameSyncMode FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanCameraFrameSyncMode)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Auto": return MikanCameraFrameSyncMode.Auto;
				case "VideoFrame": return MikanCameraFrameSyncMode.VideoFrame;
				case "FreeRunning": return MikanCameraFrameSyncMode.FreeRunning;
			}
			throw new Exception("MikanCameraFrameSyncMode has no value named " + name);
		}
	};

	public enum MikanClientMaxBufferDimension
	{
		Dim1024= 0,
		Dim2048= 1,
		Dim4096= 2,
		Dim8192= 3,
	};

	internal static class MikanClientMaxBufferDimensionJson
	{
		public static JToken ToJson(MikanClientMaxBufferDimension value)
		{
			switch (value)
			{
				case MikanClientMaxBufferDimension.Dim1024: return MikanJson.WriteString("Dim1024");
				case MikanClientMaxBufferDimension.Dim2048: return MikanJson.WriteString("Dim2048");
				case MikanClientMaxBufferDimension.Dim4096: return MikanJson.WriteString("Dim4096");
				case MikanClientMaxBufferDimension.Dim8192: return MikanJson.WriteString("Dim8192");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanClientMaxBufferDimension has no wire name for " + (long)value);
		}

		public static MikanClientMaxBufferDimension FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanClientMaxBufferDimension)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "Dim1024": return MikanClientMaxBufferDimension.Dim1024;
				case "Dim2048": return MikanClientMaxBufferDimension.Dim2048;
				case "Dim4096": return MikanClientMaxBufferDimension.Dim4096;
				case "Dim8192": return MikanClientMaxBufferDimension.Dim8192;
			}
			throw new Exception("MikanClientMaxBufferDimension has no value named " + name);
		}
	};

	public class MikanCameraComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "CameraComponent";
		public const string k_ownerSystemName = "CameraObjectSystem";

		public int stage_id;
		public int tracking_mount_id;
		public int video_source_id;
		public int light_environment_id;
		public int tracking_frame_delay;
		public MikanCameraFrameSyncMode frame_sync_mode;
		public bool pose_driven_per_frame;
		public float client_color_render_scale;
		public float client_aux_render_scale;
		public MikanClientMaxBufferDimension client_max_buffer_dimension;
		public MikanQuatd aperture_orientation_offset;
		public MikanVector3d aperture_position_offset;
		public bool has_valid_aperture_offset;

		public override string RuntimeClassName => "MikanCameraComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("stage_id", MikanJson.WriteInt(this.stage_id));
			json.Add("tracking_mount_id", MikanJson.WriteInt(this.tracking_mount_id));
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("light_environment_id", MikanJson.WriteInt(this.light_environment_id));
			json.Add("tracking_frame_delay", MikanJson.WriteInt(this.tracking_frame_delay));
			json.Add("frame_sync_mode", MikanCameraFrameSyncModeJson.ToJson(this.frame_sync_mode));
			json.Add("pose_driven_per_frame", MikanJson.WriteBool(this.pose_driven_per_frame));
			json.Add("client_color_render_scale", MikanJson.WriteFloat(this.client_color_render_scale));
			json.Add("client_aux_render_scale", MikanJson.WriteFloat(this.client_aux_render_scale));
			json.Add("client_max_buffer_dimension", MikanClientMaxBufferDimensionJson.ToJson(this.client_max_buffer_dimension));
			json.Add("aperture_orientation_offset", MikanJson.WriteObject(this.aperture_orientation_offset));
			json.Add("aperture_position_offset", MikanJson.WriteObject(this.aperture_position_offset));
			json.Add("has_valid_aperture_offset", MikanJson.WriteBool(this.has_valid_aperture_offset));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.stage_id = MikanJson.ReadInt(MikanJson.Field(json, "stage_id"));
			this.tracking_mount_id = MikanJson.ReadInt(MikanJson.Field(json, "tracking_mount_id"));
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.light_environment_id = MikanJson.ReadInt(MikanJson.Field(json, "light_environment_id"));
			this.tracking_frame_delay = MikanJson.ReadInt(MikanJson.Field(json, "tracking_frame_delay"));
			this.frame_sync_mode = MikanCameraFrameSyncModeJson.FromJson(MikanJson.Field(json, "frame_sync_mode"));
			this.pose_driven_per_frame = MikanJson.ReadBool(MikanJson.Field(json, "pose_driven_per_frame"));
			this.client_color_render_scale = MikanJson.ReadFloat(MikanJson.Field(json, "client_color_render_scale"));
			this.client_aux_render_scale = MikanJson.ReadFloat(MikanJson.Field(json, "client_aux_render_scale"));
			this.client_max_buffer_dimension = MikanClientMaxBufferDimensionJson.FromJson(MikanJson.Field(json, "client_max_buffer_dimension"));
			this.aperture_orientation_offset = MikanJson.ReadObject(MikanJson.Field(json, "aperture_orientation_offset"), this.aperture_orientation_offset ?? new MikanQuatd());
			this.aperture_position_offset = MikanJson.ReadObject(MikanJson.Field(json, "aperture_position_offset"), this.aperture_position_offset ?? new MikanVector3d());
			this.has_valid_aperture_offset = MikanJson.ReadBool(MikanJson.Field(json, "has_valid_aperture_offset"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "stage_id": value = this.stage_id; return true;
				case "tracking_mount_id": value = this.tracking_mount_id; return true;
				case "video_source_id": value = this.video_source_id; return true;
				case "light_environment_id": value = this.light_environment_id; return true;
				case "tracking_frame_delay": value = this.tracking_frame_delay; return true;
				case "frame_sync_mode": value = this.frame_sync_mode; return true;
				case "pose_driven_per_frame": value = this.pose_driven_per_frame; return true;
				case "client_color_render_scale": value = this.client_color_render_scale; return true;
				case "client_aux_render_scale": value = this.client_aux_render_scale; return true;
				case "client_max_buffer_dimension": value = this.client_max_buffer_dimension; return true;
				case "aperture_orientation_offset": value = this.aperture_orientation_offset; return true;
				case "aperture_position_offset": value = this.aperture_position_offset; return true;
				case "has_valid_aperture_offset": value = this.has_valid_aperture_offset; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "stage_id": return MikanJson.TryAssign(value, ref this.stage_id);
				case "tracking_mount_id": return MikanJson.TryAssign(value, ref this.tracking_mount_id);
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "light_environment_id": return MikanJson.TryAssign(value, ref this.light_environment_id);
				case "tracking_frame_delay": return MikanJson.TryAssign(value, ref this.tracking_frame_delay);
				case "frame_sync_mode": return MikanJson.TryAssign(value, ref this.frame_sync_mode);
				case "pose_driven_per_frame": return MikanJson.TryAssign(value, ref this.pose_driven_per_frame);
				case "client_color_render_scale": return MikanJson.TryAssign(value, ref this.client_color_render_scale);
				case "client_aux_render_scale": return MikanJson.TryAssign(value, ref this.client_aux_render_scale);
				case "client_max_buffer_dimension": return MikanJson.TryAssign(value, ref this.client_max_buffer_dimension);
				case "aperture_orientation_offset": return MikanJson.TryAssign(value, ref this.aperture_orientation_offset);
				case "aperture_position_offset": return MikanJson.TryAssign(value, ref this.aperture_position_offset);
				case "has_valid_aperture_offset": return MikanJson.TryAssign(value, ref this.has_valid_aperture_offset);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
