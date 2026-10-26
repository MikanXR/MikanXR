// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCEFTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "CEFTextureSourceComponent";
		public const string k_ownerSystemName = "CEFTextureSourceSystem";

		public string url;
		public int width;
		public int height;

		public override string RuntimeClassName => "MikanCEFTextureSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("url", MikanJson.WriteString(this.url));
			json.Add("width", MikanJson.WriteInt(this.width));
			json.Add("height", MikanJson.WriteInt(this.height));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.url = MikanJson.ReadString(MikanJson.Field(json, "url"));
			this.width = MikanJson.ReadInt(MikanJson.Field(json, "width"));
			this.height = MikanJson.ReadInt(MikanJson.Field(json, "height"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "url": value = this.url; return true;
				case "width": value = this.width; return true;
				case "height": value = this.height; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "url": return MikanJson.TryAssign(value, ref this.url);
				case "width": return MikanJson.TryAssign(value, ref this.width);
				case "height": return MikanJson.TryAssign(value, ref this.height);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanClientTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "ClientTextureSourceComponent";
		public const string k_ownerSystemName = "ClientTextureSourceSystem";

		public string client_source;

		public override string RuntimeClassName => "MikanClientTextureSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("client_source", MikanJson.WriteString(this.client_source));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.client_source = MikanJson.ReadString(MikanJson.Field(json, "client_source"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "client_source": value = this.client_source; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "client_source": return MikanJson.TryAssign(value, ref this.client_source);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanSpoutTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "SpoutTextureSourceComponent";
		public const string k_ownerSystemName = "SpoutTextureSourceSystem";

		public string spout_source;

		public override string RuntimeClassName => "MikanSpoutTextureSourceValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("spout_source", MikanJson.WriteString(this.spout_source));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.spout_source = MikanJson.ReadString(MikanJson.Field(json, "spout_source"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "spout_source": value = this.spout_source; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "spout_source": return MikanJson.TryAssign(value, ref this.spout_source);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanTextureSourceValues : MikanComponentValues
	{

		public override string RuntimeClassName => "MikanTextureSourceValues";
	};

}
