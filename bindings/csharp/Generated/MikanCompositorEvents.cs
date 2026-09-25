// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCompositorStartedEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public int compositor_id;
		[MikanFieldOrder(1)] public int camera_id;

		public MikanCompositorStartedEvent()
		{
			eventTypeName = "MikanCompositorStartedEvent";
		}
	};

	public class MikanCompositorStoppedEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public int compositor_id;
		[MikanFieldOrder(1)] public int camera_id;

		public MikanCompositorStoppedEvent()
		{
			eventTypeName = "MikanCompositorStoppedEvent";
		}
	};

}
