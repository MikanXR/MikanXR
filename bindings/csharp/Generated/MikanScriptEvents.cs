// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanScriptMessagePostedEvent : MikanEvent
	{
		public string message;

		public MikanScriptMessagePostedEvent()
		{
			eventTypeName = "MikanScriptMessagePostedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("message", MikanJson.WriteString(this.message));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.message = MikanJson.ReadString(MikanJson.Field(json, "message"));
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
