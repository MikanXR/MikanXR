// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanVRDeviceApi
	{
		INVALID= 0,
		STEAM_VR= 1,
	};

	internal static class MikanVRDeviceApiJson
	{
		public static JToken ToJson(MikanVRDeviceApi value)
		{
			switch (value)
			{
				case MikanVRDeviceApi.INVALID: return MikanJson.WriteString("INVALID");
				case MikanVRDeviceApi.STEAM_VR: return MikanJson.WriteString("STEAM_VR");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanVRDeviceApi has no wire name for " + (long)value);
		}

		public static MikanVRDeviceApi FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanVRDeviceApi)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanVRDeviceApi.INVALID;
				case "STEAM_VR": return MikanVRDeviceApi.STEAM_VR;
			}
			throw new Exception("MikanVRDeviceApi has no value named " + name);
		}
	};

	public enum MikanVRDeviceType
	{
		INVALID= 0,
		HMD= 1,
		CONTROLLER= 2,
		TRACKER= 3,
	};

	internal static class MikanVRDeviceTypeJson
	{
		public static JToken ToJson(MikanVRDeviceType value)
		{
			switch (value)
			{
				case MikanVRDeviceType.INVALID: return MikanJson.WriteString("INVALID");
				case MikanVRDeviceType.HMD: return MikanJson.WriteString("HMD");
				case MikanVRDeviceType.CONTROLLER: return MikanJson.WriteString("CONTROLLER");
				case MikanVRDeviceType.TRACKER: return MikanJson.WriteString("TRACKER");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanVRDeviceType has no wire name for " + (long)value);
		}

		public static MikanVRDeviceType FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanVRDeviceType)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "INVALID": return MikanVRDeviceType.INVALID;
				case "HMD": return MikanVRDeviceType.HMD;
				case "CONTROLLER": return MikanVRDeviceType.CONTROLLER;
				case "TRACKER": return MikanVRDeviceType.TRACKER;
			}
			throw new Exception("MikanVRDeviceType has no value named " + name);
		}
	};

	public class MikanVRDeviceComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "VRDeviceComponent";
		public const string k_ownerSystemName = "VRObjectSystem";

		public MikanVRDeviceApi vr_device_api;
		public MikanVRDeviceType vr_device_type;
		public int vr_device_index;
		public string vr_device_path;
		public List<string> socket_names;

		public override string RuntimeClassName => "MikanVRDeviceComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("vr_device_api", MikanVRDeviceApiJson.ToJson(this.vr_device_api));
			json.Add("vr_device_type", MikanVRDeviceTypeJson.ToJson(this.vr_device_type));
			json.Add("vr_device_index", MikanJson.WriteInt(this.vr_device_index));
			json.Add("vr_device_path", MikanJson.WriteString(this.vr_device_path));
			json.Add("socket_names", MikanJson.WriteList(this.socket_names, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.vr_device_api = MikanVRDeviceApiJson.FromJson(MikanJson.Field(json, "vr_device_api"));
			this.vr_device_type = MikanVRDeviceTypeJson.FromJson(MikanJson.Field(json, "vr_device_type"));
			this.vr_device_index = MikanJson.ReadInt(MikanJson.Field(json, "vr_device_index"));
			this.vr_device_path = MikanJson.ReadString(MikanJson.Field(json, "vr_device_path"));
			this.socket_names = MikanJson.ReadList(MikanJson.Field(json, "socket_names"), this.socket_names, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "vr_device_api": value = this.vr_device_api; return true;
				case "vr_device_type": value = this.vr_device_type; return true;
				case "vr_device_index": value = this.vr_device_index; return true;
				case "vr_device_path": value = this.vr_device_path; return true;
				case "socket_names": value = this.socket_names; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "vr_device_api": return MikanJson.TryAssign(value, ref this.vr_device_api);
				case "vr_device_type": return MikanJson.TryAssign(value, ref this.vr_device_type);
				case "vr_device_index": return MikanJson.TryAssign(value, ref this.vr_device_index);
				case "vr_device_path": return MikanJson.TryAssign(value, ref this.vr_device_path);
				case "socket_names": return MikanJson.TryAssign(value, ref this.socket_names);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanVRObjectSystemValues : MikanSystemValues
	{
		public const string k_systemName = "VRObjectSystem";

		public List<string> vr_device_path_list;

		public override string RuntimeClassName => "MikanVRObjectSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("vr_device_path_list", MikanJson.WriteList(this.vr_device_path_list, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.vr_device_path_list = MikanJson.ReadList(MikanJson.Field(json, "vr_device_path_list"), this.vr_device_path_list, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "vr_device_path_list": value = this.vr_device_path_list; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "vr_device_path_list": return MikanJson.TryAssign(value, ref this.vr_device_path_list);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
