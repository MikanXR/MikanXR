// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanScriptComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "ScriptComponent";
		public const string k_ownerSystemName = "ScriptObjectSystem";

		public string script_path;

		public override string RuntimeClassName => "MikanScriptComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("script_path", MikanJson.WriteString(this.script_path));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.script_path = MikanJson.ReadString(MikanJson.Field(json, "script_path"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "script_path": value = this.script_path; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "script_path": return MikanJson.TryAssign(value, ref this.script_path);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanScriptMessageInfo : MikanSerializableObject
	{
		public string content;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("content", MikanJson.WriteString(this.content));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.content = MikanJson.ReadString(MikanJson.Field(json, "content"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "content": value = this.content; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "content": return MikanJson.TryAssign(value, ref this.content);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
