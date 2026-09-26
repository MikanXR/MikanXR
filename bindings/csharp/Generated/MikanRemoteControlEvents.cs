// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanAppStageChangedEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public string new_app_state_name;
		[MikanFieldOrder(1)] public string old_app_state_name;

		public MikanAppStageChangedEvent()
		{
			eventTypeName = "MikanAppStageChangedEvent";
		}
	};

	public class MikanRemoteControlEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public string remoteControlEvent;
		[MikanFieldOrder(1)] public List<string> parameters;

		public MikanRemoteControlEvent()
		{
			eventTypeName = "MikanRemoteControlEvent";
		}
	};

}
