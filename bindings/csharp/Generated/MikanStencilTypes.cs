// This file is auto generated. DO NO EDIT.
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

	public class MikanBoxStencilComponentValues : MikanStencilComponentValues
	{
		[MikanFieldOrder(0)] public float box_x_size;
		[MikanFieldOrder(1)] public float box_y_size;
		[MikanFieldOrder(2)] public float box_z_size;
	};

	public class MikanBoxStencilSystemValues : MikanSystemValues
	{
	};

	public class MikanModelStencilComponentValues : MikanStencilComponentValues
	{
		[MikanFieldOrder(0)] public string model_path;
	};

	public class MikanModelStencilSystemValues : MikanSystemValues
	{
	};

	public class MikanQuadStencilComponentValues : MikanStencilComponentValues
	{
		[MikanFieldOrder(0)] public float quad_width;
		[MikanFieldOrder(1)] public float quad_height;
		[MikanFieldOrder(2)] public bool is_double_sided;
	};

	public class MikanQuadStencilSystemValues : MikanSystemValues
	{
	};

	public class MikanStencilComponentValues : MikanTransformComponentValues
	{
		[MikanFieldOrder(0)] public bool is_disabled;
		[MikanFieldOrder(1)] public MikanStencilCullMode cull_mode;
	};

	public class MikanStencilModelRenderGeometry
	{
		[MikanFieldOrder(0)] public List<MikanTriagulatedMesh> meshes;
	};

	public class MikanTriagulatedMesh
	{
		[MikanFieldOrder(0)] public List<MikanVector3f> vertices;
		[MikanFieldOrder(1)] public List<MikanVector3f> normals;
		[MikanFieldOrder(2)] public List<MikanVector2f> texels;
		[MikanFieldOrder(3)] public List<int> indices;
	};

}
