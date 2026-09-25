// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class DisposeClientRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string clientId;

		public DisposeClientRequest()
		{
			requestTypeName = "DisposeClientRequest";
		}
	};

	public class InitClientRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public MikanClientInfo clientInfo;

		public InitClientRequest()
		{
			requestTypeName = "InitClientRequest";
		}
	};

}
