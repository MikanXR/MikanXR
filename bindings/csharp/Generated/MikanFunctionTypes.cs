// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanFunctionDescriptor : MikanSerializableObject
	{
		public string ownerSystemClass;
		public string ownerComponentClass;
		public string functionName;
		public string displayName;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystemClass", MikanJson.WriteString(this.ownerSystemClass));
			json.Add("ownerComponentClass", MikanJson.WriteString(this.ownerComponentClass));
			json.Add("functionName", MikanJson.WriteString(this.functionName));
			json.Add("displayName", MikanJson.WriteString(this.displayName));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystemClass = MikanJson.ReadString(MikanJson.Field(json, "ownerSystemClass"));
			this.ownerComponentClass = MikanJson.ReadString(MikanJson.Field(json, "ownerComponentClass"));
			this.functionName = MikanJson.ReadString(MikanJson.Field(json, "functionName"));
			this.displayName = MikanJson.ReadString(MikanJson.Field(json, "displayName"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystemClass": value = this.ownerSystemClass; return true;
				case "ownerComponentClass": value = this.ownerComponentClass; return true;
				case "functionName": value = this.functionName; return true;
				case "displayName": value = this.displayName; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystemClass": return MikanJson.TryAssign(value, ref this.ownerSystemClass);
				case "ownerComponentClass": return MikanJson.TryAssign(value, ref this.ownerComponentClass);
				case "functionName": return MikanJson.TryAssign(value, ref this.functionName);
				case "displayName": return MikanJson.TryAssign(value, ref this.displayName);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
