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
