// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class InvokeScriptTrigger : MikanRequest
	{
		public string script_name;
		public string trigger_name;
		public Dictionary<string, string> trigger_args;

		public InvokeScriptTrigger()
		{
			requestTypeName = "InvokeScriptTrigger";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("script_name", MikanJson.WriteString(this.script_name));
			json.Add("trigger_name", MikanJson.WriteString(this.trigger_name));
			json.Add("trigger_args", MikanJson.WriteMap(this.trigger_args, k0 => MikanJson.WriteString(k0), v0 => MikanJson.WriteString(v0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.script_name = MikanJson.ReadString(MikanJson.Field(json, "script_name"));
			this.trigger_name = MikanJson.ReadString(MikanJson.Field(json, "trigger_name"));
			this.trigger_args = MikanJson.ReadMap(MikanJson.Field(json, "trigger_args"), this.trigger_args, k0 => MikanJson.ReadString(k0), v0 => MikanJson.ReadString(v0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "script_name": value = this.script_name; return true;
				case "trigger_name": value = this.trigger_name; return true;
				case "trigger_args": value = this.trigger_args; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "script_name": return MikanJson.TryAssign(value, ref this.script_name);
				case "trigger_name": return MikanJson.TryAssign(value, ref this.trigger_name);
				case "trigger_args": return MikanJson.TryAssign(value, ref this.trigger_args);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SendScriptMessage : MikanRequest
	{
		public MikanScriptMessageInfo message;

		public SendScriptMessage()
		{
			requestTypeName = "SendScriptMessage";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("message", MikanJson.WriteObject(this.message));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.message = MikanJson.ReadObject(MikanJson.Field(json, "message"), this.message ?? new MikanScriptMessageInfo());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "message": value = this.message; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "message": return MikanJson.TryAssign(value, ref this.message);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
