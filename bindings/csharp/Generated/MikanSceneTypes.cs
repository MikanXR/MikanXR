// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanSceneComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "SceneComponent";
		public const string k_ownerSystemName = "SceneObjectSystem";

		public int display_compositor_id;
		public bool force_render;

		public override string RuntimeClassName => "MikanSceneComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("display_compositor_id", MikanJson.WriteInt(this.display_compositor_id));
			json.Add("force_render", MikanJson.WriteBool(this.force_render));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.display_compositor_id = MikanJson.ReadInt(MikanJson.Field(json, "display_compositor_id"));
			this.force_render = MikanJson.ReadBool(MikanJson.Field(json, "force_render"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "display_compositor_id": value = this.display_compositor_id; return true;
				case "force_render": value = this.force_render; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "display_compositor_id": return MikanJson.TryAssign(value, ref this.display_compositor_id);
				case "force_render": return MikanJson.TryAssign(value, ref this.force_render);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanSceneSystemValues : MikanSystemValues
	{
		public const string k_systemName = "SceneObjectSystem";

		public int current_scene_id;

		public override string RuntimeClassName => "MikanSceneSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("current_scene_id", MikanJson.WriteInt(this.current_scene_id));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.current_scene_id = MikanJson.ReadInt(MikanJson.Field(json, "current_scene_id"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "current_scene_id": value = this.current_scene_id; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "current_scene_id": return MikanJson.TryAssign(value, ref this.current_scene_id);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
