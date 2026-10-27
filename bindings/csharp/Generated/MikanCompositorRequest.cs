// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class SetCompositorReturnFeed : MikanRequest
	{
		public int compositor_id;
		public string host;
		public int port;
		public bool enable;

		public SetCompositorReturnFeed()
		{
			requestTypeName = "SetCompositorReturnFeed";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("compositor_id", MikanJson.WriteInt(this.compositor_id));
			json.Add("host", MikanJson.WriteString(this.host));
			json.Add("port", MikanJson.WriteInt(this.port));
			json.Add("enable", MikanJson.WriteBool(this.enable));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.compositor_id = MikanJson.ReadInt(MikanJson.Field(json, "compositor_id"));
			this.host = MikanJson.ReadString(MikanJson.Field(json, "host"));
			this.port = MikanJson.ReadInt(MikanJson.Field(json, "port"));
			this.enable = MikanJson.ReadBool(MikanJson.Field(json, "enable"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "compositor_id": value = this.compositor_id; return true;
				case "host": value = this.host; return true;
				case "port": value = this.port; return true;
				case "enable": value = this.enable; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "compositor_id": return MikanJson.TryAssign(value, ref this.compositor_id);
				case "host": return MikanJson.TryAssign(value, ref this.host);
				case "port": return MikanJson.TryAssign(value, ref this.port);
				case "enable": return MikanJson.TryAssign(value, ref this.enable);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
