// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetModelShapeRenderGeometry : MikanRequest
	{
		[MikanFieldOrder(0)] public int shapeId;

		public GetModelShapeRenderGeometry()
		{
			requestTypeName = "GetModelShapeRenderGeometry";
		}
	};

	public class MikanShapeModelRenderGeometryResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanStencilModelRenderGeometry render_geometry;

		public MikanShapeModelRenderGeometryResponse()
		{
			responseTypeName = "MikanShapeModelRenderGeometryResponse";
		}
	};

}
