// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanEditorSystemValues : MikanSystemValues
	{
		public const string k_systemName = "EditorObjectSystem";

		public bool render_origin;
		public bool render_anchors;
		public bool render_quad_stencils;
		public bool render_box_stencils;
		public bool render_model_stencils;
		public float camera_speed;
		public string selected_language;
		public List<string> available_language_list;

		public override string RuntimeClassName => "MikanEditorSystemValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("render_origin", MikanJson.WriteBool(this.render_origin));
			json.Add("render_anchors", MikanJson.WriteBool(this.render_anchors));
			json.Add("render_quad_stencils", MikanJson.WriteBool(this.render_quad_stencils));
			json.Add("render_box_stencils", MikanJson.WriteBool(this.render_box_stencils));
			json.Add("render_model_stencils", MikanJson.WriteBool(this.render_model_stencils));
			json.Add("camera_speed", MikanJson.WriteFloat(this.camera_speed));
			json.Add("selected_language", MikanJson.WriteString(this.selected_language));
			json.Add("available_language_list", MikanJson.WriteList(this.available_language_list, e0 => MikanJson.WriteString(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.render_origin = MikanJson.ReadBool(MikanJson.Field(json, "render_origin"));
			this.render_anchors = MikanJson.ReadBool(MikanJson.Field(json, "render_anchors"));
			this.render_quad_stencils = MikanJson.ReadBool(MikanJson.Field(json, "render_quad_stencils"));
			this.render_box_stencils = MikanJson.ReadBool(MikanJson.Field(json, "render_box_stencils"));
			this.render_model_stencils = MikanJson.ReadBool(MikanJson.Field(json, "render_model_stencils"));
			this.camera_speed = MikanJson.ReadFloat(MikanJson.Field(json, "camera_speed"));
			this.selected_language = MikanJson.ReadString(MikanJson.Field(json, "selected_language"));
			this.available_language_list = MikanJson.ReadList(MikanJson.Field(json, "available_language_list"), this.available_language_list, e0 => MikanJson.ReadString(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "render_origin": value = this.render_origin; return true;
				case "render_anchors": value = this.render_anchors; return true;
				case "render_quad_stencils": value = this.render_quad_stencils; return true;
				case "render_box_stencils": value = this.render_box_stencils; return true;
				case "render_model_stencils": value = this.render_model_stencils; return true;
				case "camera_speed": value = this.camera_speed; return true;
				case "selected_language": value = this.selected_language; return true;
				case "available_language_list": value = this.available_language_list; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "render_origin": return MikanJson.TryAssign(value, ref this.render_origin);
				case "render_anchors": return MikanJson.TryAssign(value, ref this.render_anchors);
				case "render_quad_stencils": return MikanJson.TryAssign(value, ref this.render_quad_stencils);
				case "render_box_stencils": return MikanJson.TryAssign(value, ref this.render_box_stencils);
				case "render_model_stencils": return MikanJson.TryAssign(value, ref this.render_model_stencils);
				case "camera_speed": return MikanJson.TryAssign(value, ref this.camera_speed);
				case "selected_language": return MikanJson.TryAssign(value, ref this.selected_language);
				case "available_language_list": return MikanJson.TryAssign(value, ref this.available_language_list);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
