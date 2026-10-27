// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCompositorComponentValues : MikanComponentValues
	{
		public const string k_componentClassName = "CompositorComponent";
		public const string k_ownerSystemName = "CompositorObjectSystem";

		public int owner_scene_id;
		public int camera_id;
		public string compositor_graph_path;
		public bool spout_enable_output;
		public string spout_output_name;
		public bool rtp_enable_output;
		public string rtp_output_host;
		public int rtp_output_port;
		public int rtp_output_height;
		public int rtp_output_bitrate_kbps;

		public override string RuntimeClassName => "MikanCompositorComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("owner_scene_id", MikanJson.WriteInt(this.owner_scene_id));
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("compositor_graph_path", MikanJson.WriteString(this.compositor_graph_path));
			json.Add("spout_enable_output", MikanJson.WriteBool(this.spout_enable_output));
			json.Add("spout_output_name", MikanJson.WriteString(this.spout_output_name));
			json.Add("rtp_enable_output", MikanJson.WriteBool(this.rtp_enable_output));
			json.Add("rtp_output_host", MikanJson.WriteString(this.rtp_output_host));
			json.Add("rtp_output_port", MikanJson.WriteInt(this.rtp_output_port));
			json.Add("rtp_output_height", MikanJson.WriteInt(this.rtp_output_height));
			json.Add("rtp_output_bitrate_kbps", MikanJson.WriteInt(this.rtp_output_bitrate_kbps));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.owner_scene_id = MikanJson.ReadInt(MikanJson.Field(json, "owner_scene_id"));
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.compositor_graph_path = MikanJson.ReadString(MikanJson.Field(json, "compositor_graph_path"));
			this.spout_enable_output = MikanJson.ReadBool(MikanJson.Field(json, "spout_enable_output"));
			this.spout_output_name = MikanJson.ReadString(MikanJson.Field(json, "spout_output_name"));
			this.rtp_enable_output = MikanJson.ReadBool(MikanJson.Field(json, "rtp_enable_output"));
			this.rtp_output_host = MikanJson.ReadString(MikanJson.Field(json, "rtp_output_host"));
			this.rtp_output_port = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_port"));
			this.rtp_output_height = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_height"));
			this.rtp_output_bitrate_kbps = MikanJson.ReadInt(MikanJson.Field(json, "rtp_output_bitrate_kbps"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "owner_scene_id": value = this.owner_scene_id; return true;
				case "camera_id": value = this.camera_id; return true;
				case "compositor_graph_path": value = this.compositor_graph_path; return true;
				case "spout_enable_output": value = this.spout_enable_output; return true;
				case "spout_output_name": value = this.spout_output_name; return true;
				case "rtp_enable_output": value = this.rtp_enable_output; return true;
				case "rtp_output_host": value = this.rtp_output_host; return true;
				case "rtp_output_port": value = this.rtp_output_port; return true;
				case "rtp_output_height": value = this.rtp_output_height; return true;
				case "rtp_output_bitrate_kbps": value = this.rtp_output_bitrate_kbps; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "owner_scene_id": return MikanJson.TryAssign(value, ref this.owner_scene_id);
				case "camera_id": return MikanJson.TryAssign(value, ref this.camera_id);
				case "compositor_graph_path": return MikanJson.TryAssign(value, ref this.compositor_graph_path);
				case "spout_enable_output": return MikanJson.TryAssign(value, ref this.spout_enable_output);
				case "spout_output_name": return MikanJson.TryAssign(value, ref this.spout_output_name);
				case "rtp_enable_output": return MikanJson.TryAssign(value, ref this.rtp_enable_output);
				case "rtp_output_host": return MikanJson.TryAssign(value, ref this.rtp_output_host);
				case "rtp_output_port": return MikanJson.TryAssign(value, ref this.rtp_output_port);
				case "rtp_output_height": return MikanJson.TryAssign(value, ref this.rtp_output_height);
				case "rtp_output_bitrate_kbps": return MikanJson.TryAssign(value, ref this.rtp_output_bitrate_kbps);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
