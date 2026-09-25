// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class MikanConnectedEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public MikanClientAPIVersion serverVersion;
		[MikanFieldOrder(1)] public MikanClientAPIVersion minClientVersion;
		[MikanFieldOrder(2)] public bool isClientCompatible;

		public MikanConnectedEvent()
		{
			eventTypeName = "MikanConnectedEvent";
		}
	};

	public class MikanDisconnectedEvent : MikanEvent
	{
		[MikanFieldOrder(0)] public MikanDisconnectCode code;
		[MikanFieldOrder(1)] public string reason;

		public MikanDisconnectedEvent()
		{
			eventTypeName = "MikanDisconnectedEvent";
		}
	};

}
