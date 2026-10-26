// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanClientInfo : MikanSerializableObject
	{
		public string clientId;
		public string engineName;
		public string engineVersion;
		public string applicationName;
		public string applicationVersion;
		public string xrDeviceName;
		public MikanClientGraphicsApi graphicsAPI;
		public bool supportsRGB24;
		public bool supportsRGBA32;
		public bool supportsBGRA32;
		public bool supportsDepth;

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("clientId", MikanJson.WriteString(this.clientId));
			json.Add("engineName", MikanJson.WriteString(this.engineName));
			json.Add("engineVersion", MikanJson.WriteString(this.engineVersion));
			json.Add("applicationName", MikanJson.WriteString(this.applicationName));
			json.Add("applicationVersion", MikanJson.WriteString(this.applicationVersion));
			json.Add("xrDeviceName", MikanJson.WriteString(this.xrDeviceName));
			json.Add("graphicsAPI", MikanClientGraphicsApiJson.ToJson(this.graphicsAPI));
			json.Add("supportsRGB24", MikanJson.WriteBool(this.supportsRGB24));
			json.Add("supportsRGBA32", MikanJson.WriteBool(this.supportsRGBA32));
			json.Add("supportsBGRA32", MikanJson.WriteBool(this.supportsBGRA32));
			json.Add("supportsDepth", MikanJson.WriteBool(this.supportsDepth));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.clientId = MikanJson.ReadString(MikanJson.Field(json, "clientId"));
			this.engineName = MikanJson.ReadString(MikanJson.Field(json, "engineName"));
			this.engineVersion = MikanJson.ReadString(MikanJson.Field(json, "engineVersion"));
			this.applicationName = MikanJson.ReadString(MikanJson.Field(json, "applicationName"));
			this.applicationVersion = MikanJson.ReadString(MikanJson.Field(json, "applicationVersion"));
			this.xrDeviceName = MikanJson.ReadString(MikanJson.Field(json, "xrDeviceName"));
			this.graphicsAPI = MikanClientGraphicsApiJson.FromJson(MikanJson.Field(json, "graphicsAPI"));
			this.supportsRGB24 = MikanJson.ReadBool(MikanJson.Field(json, "supportsRGB24"));
			this.supportsRGBA32 = MikanJson.ReadBool(MikanJson.Field(json, "supportsRGBA32"));
			this.supportsBGRA32 = MikanJson.ReadBool(MikanJson.Field(json, "supportsBGRA32"));
			this.supportsDepth = MikanJson.ReadBool(MikanJson.Field(json, "supportsDepth"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "clientId": value = this.clientId; return true;
				case "engineName": value = this.engineName; return true;
				case "engineVersion": value = this.engineVersion; return true;
				case "applicationName": value = this.applicationName; return true;
				case "applicationVersion": value = this.applicationVersion; return true;
				case "xrDeviceName": value = this.xrDeviceName; return true;
				case "graphicsAPI": value = this.graphicsAPI; return true;
				case "supportsRGB24": value = this.supportsRGB24; return true;
				case "supportsRGBA32": value = this.supportsRGBA32; return true;
				case "supportsBGRA32": value = this.supportsBGRA32; return true;
				case "supportsDepth": value = this.supportsDepth; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "clientId": return MikanJson.TryAssign(value, ref this.clientId);
				case "engineName": return MikanJson.TryAssign(value, ref this.engineName);
				case "engineVersion": return MikanJson.TryAssign(value, ref this.engineVersion);
				case "applicationName": return MikanJson.TryAssign(value, ref this.applicationName);
				case "applicationVersion": return MikanJson.TryAssign(value, ref this.applicationVersion);
				case "xrDeviceName": return MikanJson.TryAssign(value, ref this.xrDeviceName);
				case "graphicsAPI": return MikanJson.TryAssign(value, ref this.graphicsAPI);
				case "supportsRGB24": return MikanJson.TryAssign(value, ref this.supportsRGB24);
				case "supportsRGBA32": return MikanJson.TryAssign(value, ref this.supportsRGBA32);
				case "supportsBGRA32": return MikanJson.TryAssign(value, ref this.supportsBGRA32);
				case "supportsDepth": return MikanJson.TryAssign(value, ref this.supportsDepth);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
