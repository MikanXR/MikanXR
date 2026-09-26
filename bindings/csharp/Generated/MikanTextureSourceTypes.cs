// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCEFTextureSourceValues : MikanTextureSourceValues
	{
		[MikanFieldOrder(0)] public string url;
		[MikanFieldOrder(1)] public int width;
		[MikanFieldOrder(2)] public int height;
	};

	public class MikanClientTextureSourceValues : MikanTextureSourceValues
	{
		[MikanFieldOrder(0)] public string client_source;
	};

	public class MikanSpoutTextureSourceValues : MikanTextureSourceValues
	{
		[MikanFieldOrder(0)] public string spout_source;
	};

	public class MikanTextureSourceValues : MikanComponentValues
	{
	};

}
