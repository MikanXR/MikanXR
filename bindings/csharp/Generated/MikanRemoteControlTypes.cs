// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanAppStageInfo : MikanSerializableObject
	{
		public string app_state_name;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("app_state_name", MikanJson.WriteString(this.app_state_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.app_state_name = MikanJson.ReadString(MikanJson.Field(json, "app_state_name"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "app_state_name": value = this.app_state_name; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "app_state_name": return MikanJson.TryAssign(value, ref this.app_state_name);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
