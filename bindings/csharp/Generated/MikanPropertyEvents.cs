// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanPropertyUpdateEvent : MikanEvent
	{
		public MikanPropertyValue propertyValue;

		public MikanPropertyUpdateEvent()
		{
			eventTypeName = "MikanPropertyUpdateEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("propertyValue", MikanJson.WriteObject(this.propertyValue));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.propertyValue = MikanJson.ReadObject(MikanJson.Field(json, "propertyValue"), this.propertyValue ?? new MikanPropertyValue());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "propertyValue": value = this.propertyValue; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "propertyValue": return MikanJson.TryAssign(value, ref this.propertyValue);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
