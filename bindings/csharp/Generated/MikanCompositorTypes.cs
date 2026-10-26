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

		public override string RuntimeClassName => "MikanCompositorComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("owner_scene_id", MikanJson.WriteInt(this.owner_scene_id));
			json.Add("camera_id", MikanJson.WriteInt(this.camera_id));
			json.Add("compositor_graph_path", MikanJson.WriteString(this.compositor_graph_path));
			json.Add("spout_enable_output", MikanJson.WriteBool(this.spout_enable_output));
			json.Add("spout_output_name", MikanJson.WriteString(this.spout_output_name));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.owner_scene_id = MikanJson.ReadInt(MikanJson.Field(json, "owner_scene_id"));
			this.camera_id = MikanJson.ReadInt(MikanJson.Field(json, "camera_id"));
			this.compositor_graph_path = MikanJson.ReadString(MikanJson.Field(json, "compositor_graph_path"));
			this.spout_enable_output = MikanJson.ReadBool(MikanJson.Field(json, "spout_enable_output"));
			this.spout_output_name = MikanJson.ReadString(MikanJson.Field(json, "spout_output_name"));
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
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
