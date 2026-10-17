// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanBoxShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "BoxShapeComponent";
		public const string k_ownerSystemName = "BoxShapeSystem";

		[MikanFieldOrder(0)] public float box_x_size;
		[MikanFieldOrder(1)] public float box_y_size;
		[MikanFieldOrder(2)] public float box_z_size;
	};

	public class MikanBoxShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "BoxShapeSystem";

	};

	public class MikanModelShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "ModelShapeComponent";
		public const string k_ownerSystemName = "ModelShapeSystem";

		[MikanFieldOrder(0)] public string model_path;
		[MikanFieldOrder(1)] public string geometry_url;
	};

	public class MikanModelShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "ModelShapeSystem";

	};

	public class MikanQuadShapeComponentValues : MikanShapeComponentValues
	{
		public const string k_componentClassName = "QuadShapeComponent";
		public const string k_ownerSystemName = "QuadShapeSystem";

		[MikanFieldOrder(0)] public float quad_width;
		[MikanFieldOrder(1)] public float quad_height;
		[MikanFieldOrder(2)] public bool is_double_sided;
	};

	public class MikanQuadShapeSystemValues : MikanSystemValues
	{
		public const string k_systemName = "QuadShapeSystem";

	};

	public class MikanShapeComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public string shape_graph_path;
	};

}
