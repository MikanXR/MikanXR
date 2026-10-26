// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanTrackingMountComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "TrackingMountComponent";
		public const string k_ownerSystemName = "TrackingMountObjectSystem";

		public string device_path;
		public string socket_name;
		public List<string> available_socket_names;

		public override string RuntimeClassName => "MikanTrackingMountComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("device_path", MikanJson.WriteString(this.device_path));
			json.Add("socket_name", MikanJson.WriteString(this.socket_name));
			json.Add("available_socket_names", MikanJson.WriteList(this.available_socket_names, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.device_path = MikanJson.ReadString(MikanJson.Field(json, "device_path"));
			this.socket_name = MikanJson.ReadString(MikanJson.Field(json, "socket_name"));
			this.available_socket_names = MikanJson.ReadList(MikanJson.Field(json, "available_socket_names"), this.available_socket_names, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "device_path": value = this.device_path; return true;
				case "socket_name": value = this.socket_name; return true;
				case "available_socket_names": value = this.available_socket_names; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "device_path": return MikanJson.TryAssign(value, ref this.device_path);
				case "socket_name": return MikanJson.TryAssign(value, ref this.socket_name);
				case "available_socket_names": return MikanJson.TryAssign(value, ref this.available_socket_names);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
