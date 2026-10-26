// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetVideoSourceIntrinsics : MikanRequest
	{
		public int video_source_id;

		public GetVideoSourceIntrinsics()
		{
			requestTypeName = "GetVideoSourceIntrinsics";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetVideoSourceMode : MikanRequest
	{
		public int video_source_id;

		public GetVideoSourceMode()
		{
			requestTypeName = "GetVideoSourceMode";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVideoSourceIntrinsicsResponse : MikanResponse
	{
		public MikanVideoSourceIntrinsics intrinsics;

		public MikanVideoSourceIntrinsicsResponse()
		{
			responseTypeName = "MikanVideoSourceIntrinsicsResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("intrinsics", MikanJson.WriteObject(this.intrinsics));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.intrinsics = MikanJson.ReadObject(MikanJson.Field(json, "intrinsics"), this.intrinsics ?? new MikanVideoSourceIntrinsics());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "intrinsics": value = this.intrinsics; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "intrinsics": return MikanJson.TryAssign(value, ref this.intrinsics);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVideoSourceModeResponse : MikanResponse
	{
		public MikanVideoSourceType video_source_type;
		public string video_source_api;
		public string device_path;
		public string video_mode_name;
		public int resolution_x;
		public int resolution_y;
		public float frame_rate;

		public MikanVideoSourceModeResponse()
		{
			responseTypeName = "MikanVideoSourceModeResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_type", MikanVideoSourceTypeJson.ToJson(this.video_source_type));
			json.Add("video_source_api", MikanJson.WriteString(this.video_source_api));
			json.Add("device_path", MikanJson.WriteString(this.device_path));
			json.Add("video_mode_name", MikanJson.WriteString(this.video_mode_name));
			json.Add("resolution_x", MikanJson.WriteInt(this.resolution_x));
			json.Add("resolution_y", MikanJson.WriteInt(this.resolution_y));
			json.Add("frame_rate", MikanJson.WriteFloat(this.frame_rate));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_type = MikanVideoSourceTypeJson.FromJson(MikanJson.Field(json, "video_source_type"));
			this.video_source_api = MikanJson.ReadString(MikanJson.Field(json, "video_source_api"));
			this.device_path = MikanJson.ReadString(MikanJson.Field(json, "device_path"));
			this.video_mode_name = MikanJson.ReadString(MikanJson.Field(json, "video_mode_name"));
			this.resolution_x = MikanJson.ReadInt(MikanJson.Field(json, "resolution_x"));
			this.resolution_y = MikanJson.ReadInt(MikanJson.Field(json, "resolution_y"));
			this.frame_rate = MikanJson.ReadFloat(MikanJson.Field(json, "frame_rate"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_type": value = this.video_source_type; return true;
				case "video_source_api": value = this.video_source_api; return true;
				case "device_path": value = this.device_path; return true;
				case "video_mode_name": value = this.video_mode_name; return true;
				case "resolution_x": value = this.resolution_x; return true;
				case "resolution_y": value = this.resolution_y; return true;
				case "frame_rate": value = this.frame_rate; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_type": return MikanJson.TryAssign(value, ref this.video_source_type);
				case "video_source_api": return MikanJson.TryAssign(value, ref this.video_source_api);
				case "device_path": return MikanJson.TryAssign(value, ref this.device_path);
				case "video_mode_name": return MikanJson.TryAssign(value, ref this.video_mode_name);
				case "resolution_x": return MikanJson.TryAssign(value, ref this.resolution_x);
				case "resolution_y": return MikanJson.TryAssign(value, ref this.resolution_y);
				case "frame_rate": return MikanJson.TryAssign(value, ref this.frame_rate);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SetUSBVideoSourceDevice : MikanRequest
	{
		public int video_source_id;
		public string device_path;

		public SetUSBVideoSourceDevice()
		{
			requestTypeName = "SetUSBVideoSourceDevice";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("device_path", MikanJson.WriteString(this.device_path));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.device_path = MikanJson.ReadString(MikanJson.Field(json, "device_path"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
				case "device_path": value = this.device_path; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "device_path": return MikanJson.TryAssign(value, ref this.device_path);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SetUSBVideoSourceFormat : MikanRequest
	{
		public int video_source_id;
		public string format;

		public SetUSBVideoSourceFormat()
		{
			requestTypeName = "SetUSBVideoSourceFormat";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("format", MikanJson.WriteString(this.format));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.format = MikanJson.ReadString(MikanJson.Field(json, "format"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
				case "format": value = this.format; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "format": return MikanJson.TryAssign(value, ref this.format);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SetUSBVideoSourceFrameRate : MikanRequest
	{
		public int video_source_id;
		public string frame_rate;

		public SetUSBVideoSourceFrameRate()
		{
			requestTypeName = "SetUSBVideoSourceFrameRate";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("frame_rate", MikanJson.WriteString(this.frame_rate));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.frame_rate = MikanJson.ReadString(MikanJson.Field(json, "frame_rate"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
				case "frame_rate": value = this.frame_rate; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "frame_rate": return MikanJson.TryAssign(value, ref this.frame_rate);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SetUSBVideoSourceResolution : MikanRequest
	{
		public int video_source_id;
		public string resolution;

		public SetUSBVideoSourceResolution()
		{
			requestTypeName = "SetUSBVideoSourceResolution";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("video_source_id", MikanJson.WriteInt(this.video_source_id));
			json.Add("resolution", MikanJson.WriteString(this.resolution));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.video_source_id = MikanJson.ReadInt(MikanJson.Field(json, "video_source_id"));
			this.resolution = MikanJson.ReadString(MikanJson.Field(json, "resolution"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "video_source_id": value = this.video_source_id; return true;
				case "resolution": value = this.resolution; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "video_source_id": return MikanJson.TryAssign(value, ref this.video_source_id);
				case "resolution": return MikanJson.TryAssign(value, ref this.resolution);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
