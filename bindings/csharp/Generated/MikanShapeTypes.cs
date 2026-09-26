// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanBoxShapeComponentValues : MikanShapeComponentValues
	{
		[MikanFieldOrder(0)] public float box_x_size;
		[MikanFieldOrder(1)] public float box_y_size;
		[MikanFieldOrder(2)] public float box_z_size;
	};

	public class MikanBoxShapeSystemValues : MikanSystemValues
	{
	};

	public class MikanModelShapeComponentValues : MikanShapeComponentValues
	{
		[MikanFieldOrder(0)] public string model_path;
	};

	public class MikanModelShapeSystemValues : MikanSystemValues
	{
	};

	public class MikanQuadShapeComponentValues : MikanShapeComponentValues
	{
		[MikanFieldOrder(0)] public float quad_width;
		[MikanFieldOrder(1)] public float quad_height;
		[MikanFieldOrder(2)] public bool is_double_sided;
	};

	public class MikanQuadShapeSystemValues : MikanSystemValues
	{
	};

	public class MikanShapeComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public string shape_graph_path;
	};

}
