// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanClientAPIVersion
	{
		[MikanFieldOrder(0)] public int version;
	};

	public class MikanRenderTargetDescriptor
	{
		[MikanFieldOrder(0)] public MikanColorBufferType color_buffer_type;
		[MikanFieldOrder(1)] public MikanDepthBufferType depth_buffer_type;
		[MikanFieldOrder(2)] public MikanShadowBufferType shadow_buffer_type;
		[MikanFieldOrder(3)] public uint width;
		[MikanFieldOrder(4)] public uint height;
		[MikanFieldOrder(5)] public uint aux_width;
		[MikanFieldOrder(6)] public uint aux_height;
		[MikanFieldOrder(7)] public MikanClientGraphicsApi graphicsAPI;
	};

}
