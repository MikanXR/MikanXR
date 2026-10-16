// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCEFTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "CEFTextureSourceComponent";
		public const string k_ownerSystemName = "CEFTextureSourceSystem";

		[MikanFieldOrder(0)] public string url;
		[MikanFieldOrder(1)] public int width;
		[MikanFieldOrder(2)] public int height;
	};

	public class MikanClientTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "ClientTextureSourceComponent";
		public const string k_ownerSystemName = "ClientTextureSourceSystem";

		[MikanFieldOrder(0)] public string client_source;
	};

	public class MikanSpoutTextureSourceValues : MikanTextureSourceValues
	{
		public const string k_componentClassName = "SpoutTextureSourceComponent";
		public const string k_ownerSystemName = "SpoutTextureSourceSystem";

		[MikanFieldOrder(0)] public string spout_source;
	};

	public class MikanTextureSourceValues : MikanComponentValues
	{
	};

}
