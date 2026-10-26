// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanLightDMXDataChangedEvent : MikanEvent
	{
		public MikanDMXData dmx_data;

		public MikanLightDMXDataChangedEvent()
		{
			eventTypeName = "MikanLightDMXDataChangedEvent";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("dmx_data", MikanJson.WriteObject(this.dmx_data));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.dmx_data = MikanJson.ReadObject(MikanJson.Field(json, "dmx_data"), this.dmx_data ?? new MikanDMXData());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "dmx_data": value = this.dmx_data; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "dmx_data": return MikanJson.TryAssign(value, ref this.dmx_data);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
