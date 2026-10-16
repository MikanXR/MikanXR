// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanSceneComponentValues : MikanTransformComponentValues
	{
		public const string k_componentClassName = "SceneComponent";
		public const string k_ownerSystemName = "SceneObjectSystem";

		[MikanFieldOrder(0)] public int display_compositor_id;
		[MikanFieldOrder(1)] public bool force_render;
	};

	public class MikanSceneSystemValues : MikanSystemValues
	{
		public const string k_systemName = "SceneObjectSystem";

		[MikanFieldOrder(0)] public int current_scene_id;
	};

}
