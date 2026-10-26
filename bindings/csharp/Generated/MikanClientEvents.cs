// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanConnectedEvent : MikanEvent
	{
		public MikanClientAPIVersion serverVersion;
		public MikanClientAPIVersion minClientVersion;
		public bool isClientCompatible;
		public int httpPort;

		public MikanConnectedEvent()
		{
			eventTypeName = "MikanConnectedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("serverVersion", MikanJson.WriteObject(this.serverVersion));
			json.Add("minClientVersion", MikanJson.WriteObject(this.minClientVersion));
			json.Add("isClientCompatible", MikanJson.WriteBool(this.isClientCompatible));
			json.Add("httpPort", MikanJson.WriteInt(this.httpPort));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.serverVersion = MikanJson.ReadObject(MikanJson.Field(json, "serverVersion"), this.serverVersion ?? new MikanClientAPIVersion());
			this.minClientVersion = MikanJson.ReadObject(MikanJson.Field(json, "minClientVersion"), this.minClientVersion ?? new MikanClientAPIVersion());
			this.isClientCompatible = MikanJson.ReadBool(MikanJson.Field(json, "isClientCompatible"));
			this.httpPort = MikanJson.ReadInt(MikanJson.Field(json, "httpPort"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "serverVersion": value = this.serverVersion; return true;
				case "minClientVersion": value = this.minClientVersion; return true;
				case "isClientCompatible": value = this.isClientCompatible; return true;
				case "httpPort": value = this.httpPort; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "serverVersion": return MikanJson.TryAssign(value, ref this.serverVersion);
				case "minClientVersion": return MikanJson.TryAssign(value, ref this.minClientVersion);
				case "isClientCompatible": return MikanJson.TryAssign(value, ref this.isClientCompatible);
				case "httpPort": return MikanJson.TryAssign(value, ref this.httpPort);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDisconnectedEvent : MikanEvent
	{
		public MikanDisconnectCode code;
		public string reason;

		public MikanDisconnectedEvent()
		{
			eventTypeName = "MikanDisconnectedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("code", MikanDisconnectCodeJson.ToJson(this.code));
			json.Add("reason", MikanJson.WriteString(this.reason));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.code = MikanDisconnectCodeJson.FromJson(MikanJson.Field(json, "code"));
			this.reason = MikanJson.ReadString(MikanJson.Field(json, "reason"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "code": value = this.code; return true;
				case "reason": value = this.reason; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "code": return MikanJson.TryAssign(value, ref this.code);
				case "reason": return MikanJson.TryAssign(value, ref this.reason);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
