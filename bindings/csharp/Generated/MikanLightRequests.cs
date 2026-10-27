// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetDMXData : MikanRequest
	{
		public List<int> dmx_universe_ids;

		public GetDMXData()
		{
			requestTypeName = "GetDMXData";
		}

		public override string ResponseTypeName => nameof(MikanDMXDataResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("dmx_universe_ids", MikanJson.WriteList(this.dmx_universe_ids, e0 => MikanJson.WriteInt(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.dmx_universe_ids = MikanJson.ReadList(MikanJson.Field(json, "dmx_universe_ids"), this.dmx_universe_ids, e0 => MikanJson.ReadInt(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "dmx_universe_ids": value = this.dmx_universe_ids; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "dmx_universe_ids": return MikanJson.TryAssign(value, ref this.dmx_universe_ids);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanDMXDataResponse : MikanResponse
	{
		public MikanDMXData dmx_data;

		public MikanDMXDataResponse()
		{
			responseTypeName = "MikanDMXDataResponse";
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

	public class SetLightDMXDataSubcription : MikanRequest
	{
		public List<int> light_ids;
		public bool subscribe;

		public SetLightDMXDataSubcription()
		{
			requestTypeName = "SetLightDMXDataSubcription";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("light_ids", MikanJson.WriteList(this.light_ids, e0 => MikanJson.WriteInt(e0)));
			json.Add("subscribe", MikanJson.WriteBool(this.subscribe));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.light_ids = MikanJson.ReadList(MikanJson.Field(json, "light_ids"), this.light_ids, e0 => MikanJson.ReadInt(e0));
			this.subscribe = MikanJson.ReadBool(MikanJson.Field(json, "subscribe"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "light_ids": value = this.light_ids; return true;
				case "subscribe": value = this.subscribe; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "light_ids": return MikanJson.TryAssign(value, ref this.light_ids);
				case "subscribe": return MikanJson.TryAssign(value, ref this.subscribe);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
