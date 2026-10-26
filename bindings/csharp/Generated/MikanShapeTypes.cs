// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanBoxShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "BoxShapeComponent";
		public const string k_ownerSystemName = "BoxShapeSystem";

		public float box_x_size;
		public float box_y_size;
		public float box_z_size;

		public override string RuntimeClassName => "MikanBoxShapeComponentValues";

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

	public class MikanBoxShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "BoxShapeSystem";


		public override string RuntimeClassName => "MikanBoxShapeSystemValues";
	};

	public class MikanModelShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "ModelShapeComponent";
		public const string k_ownerSystemName = "ModelShapeSystem";

		public string model_path;
		public string geometry_url;

		public override string RuntimeClassName => "MikanModelShapeComponentValues";

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

	public class MikanModelShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "ModelShapeSystem";


		public override string RuntimeClassName => "MikanModelShapeSystemValues";
	};

	public class MikanQuadShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "QuadShapeComponent";
		public const string k_ownerSystemName = "QuadShapeSystem";

		public float quad_width;
		public float quad_height;
		public bool is_double_sided;

		public override string RuntimeClassName => "MikanQuadShapeComponentValues";

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

	public class MikanQuadShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "QuadShapeSystem";


		public override string RuntimeClassName => "MikanQuadShapeSystemValues";
	};

	public class MikanShapeComponentValues : MikanTransformComponentValues
	{
		public string shape_graph_path;

		public override string RuntimeClassName => "MikanShapeComponentValues";

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("shape_graph_path", MikanJson.WriteString(this.shape_graph_path));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.shape_graph_path = MikanJson.ReadString(MikanJson.Field(json, "shape_graph_path"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "shape_graph_path": value = this.shape_graph_path; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "shape_graph_path": return MikanJson.TryAssign(value, ref this.shape_graph_path);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
