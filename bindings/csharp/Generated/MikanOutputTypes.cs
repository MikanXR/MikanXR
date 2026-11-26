// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanOutputComponentValues : MikanComponentValues
	{
		public bool enabled;

		public override string RuntimeClassName => "MikanOutputComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("enabled", MikanJson.WriteBool(this.enabled));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.enabled = MikanJson.ReadBool(MikanJson.Field(json, "enabled"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "enabled": value = this.enabled; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "enabled": return MikanJson.TryAssign(value, ref this.enabled);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRTPOutputComponentValues : MikanOutputComponentValues
	{
		public const string k_componentClassName = "RTPOutputComponent";
		public const string k_ownerSystemName = "RTPOutputSystem";

		public string rtp_output_host;
		public int rtp_output_port;
		public int rtp_output_height;
		public int rtp_output_bitrate_kbps;

		public override string RuntimeClassName => "MikanRTPOutputComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("rtp_output_host", MikanJson.WriteString(this.rtp_output_host));
			json.Add("rtp_output_port", MikanJson.WriteInt(this.rtp_output_port));
			json.Add("rtp_output_height", MikanJson.WriteInt(this.rtp_output_height));
			json.Add("rtp_output_bitrate_kbps", MikanJson.WriteInt(this.rtp_output_bitrate_kbps));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.rtp_output_host = MikanJson.ReadString(MikanJson.Field(json, "rtp_output_host"));
			this.rtp_output_port = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_port"));
			this.rtp_output_height = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_height"));
			this.rtp_output_bitrate_kbps = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_bitrate_kbps"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "rtp_output_host": value = this.rtp_output_host; return true;
				case "rtp_output_port": value = this.rtp_output_port; return true;
				case "rtp_output_height": value = this.rtp_output_height; return true;
				case "rtp_output_bitrate_kbps": value = this.rtp_output_bitrate_kbps; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "rtp_output_host": return MikanJson.TryAssign(value, ref this.rtp_output_host);
				case "rtp_output_port": return MikanJson.TryAssign(value, ref this.rtp_output_port);
				case "rtp_output_height": return MikanJson.TryAssign(value, ref this.rtp_output_height);
				case "rtp_output_bitrate_kbps": return MikanJson.TryAssign(value, ref this.rtp_output_bitrate_kbps);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanSpoutOutputComponentValues : MikanOutputComponentValues
	{
		public const string k_componentClassName = "SpoutOutputComponent";
		public const string k_ownerSystemName = "SpoutOutputSystem";

		public string spout_output_name;

		public override string RuntimeClassName => "MikanSpoutOutputComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("spout_output_name", MikanJson.WriteString(this.spout_output_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.spout_output_name = MikanJson.ReadString(MikanJson.Field(json, "spout_output_name"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "spout_output_name": value = this.spout_output_name; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "spout_output_name": return MikanJson.TryAssign(value, ref this.spout_output_name);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
