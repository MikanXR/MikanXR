// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanComponentValues : PolymorphicStruct
	{
		public int component_id;
		public string component_name;

		public override string RuntimeClassName => "MikanComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("component_id", MikanJson.WriteInt(this.component_id));
			json.Add("component_name", MikanJson.WriteString(this.component_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.component_id = MikanJson.ReadInt(MikanJson.Field(json, "component_id"));
			this.component_name = MikanJson.ReadString(MikanJson.Field(json, "component_name"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "component_id": value = this.component_id; return true;
				case "component_name": value = this.component_name; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "component_id": return MikanJson.TryAssign(value, ref this.component_id);
				case "component_name": return MikanJson.TryAssign(value, ref this.component_name);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
