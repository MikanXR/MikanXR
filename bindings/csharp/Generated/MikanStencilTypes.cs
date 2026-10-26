// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanStencilCullMode
	{
		NONE= 0,
		Z_Axis= 1,
		Y_Axis= 2,
		X_Axis= 3,
	};

	internal static class MikanStencilCullModeJson
	{
		public static JToken ToJson(MikanStencilCullMode value)
		{
			switch (value)
			{
				case MikanStencilCullMode.NONE: return MikanJson.WriteString("NONE");
				case MikanStencilCullMode.Z_Axis: return MikanJson.WriteString("Z_Axis");
				case MikanStencilCullMode.Y_Axis: return MikanJson.WriteString("Y_Axis");
				case MikanStencilCullMode.X_Axis: return MikanJson.WriteString("X_Axis");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanStencilCullMode has no wire name for " + (long)value);
		}

		public static MikanStencilCullMode FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanStencilCullMode)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "NONE": return MikanStencilCullMode.NONE;
				case "Z_Axis": return MikanStencilCullMode.Z_Axis;
				case "Y_Axis": return MikanStencilCullMode.Y_Axis;
				case "X_Axis": return MikanStencilCullMode.X_Axis;
			}
			throw new Exception("MikanStencilCullMode has no value named " + name);
		}
	};

	public class MikanBoxStencilComponentValues : MikanStencilComponentValues
	{
		public const string k_componentClassName = "BoxStencilComponent";
		public const string k_ownerSystemName = "BoxStencilSystem";

		public float box_x_size;
		public float box_y_size;
		public float box_z_size;

		public override string RuntimeClassName => "MikanBoxStencilComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("box_x_size", MikanJson.WriteFloat(this.box_x_size));
			json.Add("box_y_size", MikanJson.WriteFloat(this.box_y_size));
			json.Add("box_z_size", MikanJson.WriteFloat(this.box_z_size));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.box_x_size = MikanJson.ReadFloat(MikanJson.Field(json, "box_x_size"));
			this.box_y_size = MikanJson.ReadFloat(MikanJson.Field(json, "box_y_size"));
			this.box_z_size = MikanJson.ReadFloat(MikanJson.Field(json, "box_z_size"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "box_x_size": value = this.box_x_size; return true;
				case "box_y_size": value = this.box_y_size; return true;
				case "box_z_size": value = this.box_z_size; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "box_x_size": return MikanJson.TryAssign(value, ref this.box_x_size);
				case "box_y_size": return MikanJson.TryAssign(value, ref this.box_y_size);
				case "box_z_size": return MikanJson.TryAssign(value, ref this.box_z_size);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanBoxStencilSystemValues : MikanSystemValues
	{
		public const string k_systemName = "BoxStencilSystem";


		public override string RuntimeClassName => "MikanBoxStencilSystemValues";
	};

	public class MikanModelStencilComponentValues : MikanStencilComponentValues
	{
		public const string k_componentClassName = "ModelStencilComponent";
		public const string k_ownerSystemName = "ModelStencilSystem";

		public string model_path;
		public string geometry_url;

		public override string RuntimeClassName => "MikanModelStencilComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("model_path", MikanJson.WriteString(this.model_path));
			json.Add("geometry_url", MikanJson.WriteString(this.geometry_url));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.model_path = MikanJson.ReadString(MikanJson.Field(json, "model_path"));
			this.geometry_url = MikanJson.ReadString(MikanJson.Field(json, "geometry_url"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "model_path": value = this.model_path; return true;
				case "geometry_url": value = this.geometry_url; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "model_path": return MikanJson.TryAssign(value, ref this.model_path);
				case "geometry_url": return MikanJson.TryAssign(value, ref this.geometry_url);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanModelStencilSystemValues : MikanSystemValues
	{
		public const string k_systemName = "ModelStencilSystem";


		public override string RuntimeClassName => "MikanModelStencilSystemValues";
	};

	public class MikanQuadStencilComponentValues : MikanStencilComponentValues
	{
		public const string k_componentClassName = "QuadStencilComponent";
		public const string k_ownerSystemName = "QuadStencilSystem";

		public float quad_width;
		public float quad_height;
		public bool is_double_sided;

		public override string RuntimeClassName => "MikanQuadStencilComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("quad_width", MikanJson.WriteFloat(this.quad_width));
			json.Add("quad_height", MikanJson.WriteFloat(this.quad_height));
			json.Add("is_double_sided", MikanJson.WriteBool(this.is_double_sided));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.quad_width = MikanJson.ReadFloat(MikanJson.Field(json, "quad_width"));
			this.quad_height = MikanJson.ReadFloat(MikanJson.Field(json, "quad_height"));
			this.is_double_sided = MikanJson.ReadBool(MikanJson.Field(json, "is_double_sided"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "quad_width": value = this.quad_width; return true;
				case "quad_height": value = this.quad_height; return true;
				case "is_double_sided": value = this.is_double_sided; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "quad_width": return MikanJson.TryAssign(value, ref this.quad_width);
				case "quad_height": return MikanJson.TryAssign(value, ref this.quad_height);
				case "is_double_sided": return MikanJson.TryAssign(value, ref this.is_double_sided);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class MikanQuadStencilSystemValues : MikanSystemValues
	{
		public const string k_systemName = "QuadStencilSystem";


		public override string RuntimeClassName => "MikanQuadStencilSystemValues";
	};

	public class MikanStencilComponentValues : MikanTransformComponentValues
	{
		public bool is_disabled;
		public MikanStencilCullMode cull_mode;

		public override string RuntimeClassName => "MikanStencilComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("is_disabled", MikanJson.WriteBool(this.is_disabled));
			json.Add("cull_mode", MikanStencilCullModeJson.ToJson(this.cull_mode));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.is_disabled = MikanJson.ReadBool(MikanJson.Field(json, "is_disabled"));
			this.cull_mode = MikanStencilCullModeJson.FromJson(MikanJson.Field(json, "cull_mode"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "is_disabled": value = this.is_disabled; return true;
				case "cull_mode": value = this.cull_mode; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "is_disabled": return MikanJson.TryAssign(value, ref this.is_disabled);
				case "cull_mode": return MikanJson.TryAssign(value, ref this.cull_mode);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
