// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class GetModelStencilRenderGeometry : MikanRequest
	{
		[MikanFieldOrder(0)] public int stencilId;

		public GetModelStencilRenderGeometry()
		{
			requestTypeName = "GetModelStencilRenderGeometry";
		}
	};

	public class MikanStencilModelRenderGeometryResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanStencilModelRenderGeometry render_geometry;

		public MikanStencilModelRenderGeometryResponse()
		{
			responseTypeName = "MikanStencilModelRenderGeometryResponse";
		}
	};

}
