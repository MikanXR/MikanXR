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
		public string compositor_graph_path;

		public override string RuntimeClassName => "MikanCompositorComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("owner_scene_id", MikanJson.WriteInt(this.owner_scene_id));
			json.Add("compositor_graph_path", MikanJson.WriteString(this.compositor_graph_path));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.owner_scene_id = MikanJson.ReadInt(MikanJson.Field(json, "owner_scene_id"));
			this.compositor_graph_path = MikanJson.ReadString(MikanJson.Field(json, "compositor_graph_path"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "owner_scene_id": value = this.owner_scene_id; return true;
				case "compositor_graph_path": value = this.compositor_graph_path; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "owner_scene_id": return MikanJson.TryAssign(value, ref this.owner_scene_id);
				case "compositor_graph_path": return MikanJson.TryAssign(value, ref this.compositor_graph_path);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
