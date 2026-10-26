// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class DisposeClientRequest : MikanRequest
	{
		public string clientId;

		public DisposeClientRequest()
		{
			requestTypeName = "DisposeClientRequest";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("clientId", MikanJson.WriteString(this.clientId));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.clientId = MikanJson.ReadString(MikanJson.Field(json, "clientId"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "clientId": value = this.clientId; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "clientId": return MikanJson.TryAssign(value, ref this.clientId);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class InitClientRequest : MikanRequest
	{
		public MikanClientInfo clientInfo;

		public InitClientRequest()
		{
			requestTypeName = "InitClientRequest";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("clientInfo", MikanJson.WriteObject(this.clientInfo));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.clientInfo = MikanJson.ReadObject(MikanJson.Field(json, "clientInfo"), this.clientInfo ?? new MikanClientInfo());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "clientInfo": value = this.clientInfo; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "clientInfo": return MikanJson.TryAssign(value, ref this.clientInfo);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
