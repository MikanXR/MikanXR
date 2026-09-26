// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanAPIResult
	{
		Success= 0,
		GeneralError= 1,
		Uninitialized= 2,
		NullParam= 3,
		InvalidParam= 4,
		RequestFailed= 5,
		NotConnected= 6,
		AlreadyConnected= 7,
		SocketError= 8,
		Timeout= 9,
		Canceled= 10,
		NoData= 11,
		BufferTooSmall= 12,
		UnknownClient= 13,
		UnknownFunction= 14,
		MalformedParameters= 15,
		MalformedResponse= 16,
		NoVideoSource= 100,
		NoVideoSourceAssignedTracker= 101,
		InvalidDeviceId= 102,
		InvalidStencilID= 103,
		InvalidAnchorID= 104,
		InvalidCameraID= 105,
		InvalidSceneID= 106,
		InvalidStageID= 106,
		InvalidShapeID= 107,
	};

	public class MikanEvent
	{
		[MikanFieldOrder(0)] public string eventTypeName;

		public MikanEvent()
		{
			eventTypeName = "MikanEvent";
		}
	};

	public class MikanRequest
	{
		[MikanFieldOrder(0)] public string requestTypeName;
		[MikanFieldOrder(1)] public int requestId;

		public MikanRequest()
		{
			requestTypeName = "MikanRequest";
		}
	};

	public class MikanResponse
	{
		[MikanFieldOrder(0)] public string responseTypeName;
		[MikanFieldOrder(1)] public int requestId;
		[MikanFieldOrder(2)] public MikanAPIResult resultCode;

		public MikanResponse()
		{
			responseTypeName = "MikanResponse";
		}
	};

}
