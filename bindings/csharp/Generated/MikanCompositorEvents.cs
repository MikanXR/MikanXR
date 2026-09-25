// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanCompositorStartedEvent : MikanEvent
	{
		public int compositor_id;
		public int camera_id;

		public MikanCompositorStartedEvent()
		{
			eventTypeName = "MikanCompositorStartedEvent";
		}
	};

	public class MikanCompositorStoppedEvent : MikanEvent
	{
		public int compositor_id;
		public int camera_id;

		public MikanCompositorStoppedEvent()
		{
			eventTypeName = "MikanCompositorStoppedEvent";
		}
	};

}
