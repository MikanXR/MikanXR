// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanAppStageChangedEvent : MikanEvent
	{
		public string new_app_state_name;
		public string old_app_state_name;

		public MikanAppStageChangedEvent()
		{
			eventTypeName = "MikanAppStageChangedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("new_app_state_name", MikanJson.WriteString(this.new_app_state_name));
			json.Add("old_app_state_name", MikanJson.WriteString(this.old_app_state_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.new_app_state_name = MikanJson.ReadString(MikanJson.Field(json, "new_app_state_name"));
			this.old_app_state_name = MikanJson.ReadString(MikanJson.Field(json, "old_app_state_name"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "new_app_state_name": value = this.new_app_state_name; return true;
				case "old_app_state_name": value = this.old_app_state_name; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "new_app_state_name": return MikanJson.TryAssign(value, ref this.new_app_state_name);
				case "old_app_state_name": return MikanJson.TryAssign(value, ref this.old_app_state_name);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanRemoteControlEvent : MikanEvent
	{
		public string remoteControlEvent;
		public List<string> parameters;

		public MikanRemoteControlEvent()
		{
			eventTypeName = "MikanRemoteControlEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("remoteControlEvent", MikanJson.WriteString(this.remoteControlEvent));
			json.Add("parameters", MikanJson.WriteList(this.parameters, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.remoteControlEvent = MikanJson.ReadString(MikanJson.Field(json, "remoteControlEvent"));
			this.parameters = MikanJson.ReadList(MikanJson.Field(json, "parameters"), this.parameters, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "remoteControlEvent": value = this.remoteControlEvent; return true;
				case "parameters": value = this.parameters; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "remoteControlEvent": return MikanJson.TryAssign(value, ref this.remoteControlEvent);
				case "parameters": return MikanJson.TryAssign(value, ref this.parameters);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
