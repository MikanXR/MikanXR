// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class InvokeScriptTrigger : MikanRequest
	{
		[MikanFieldOrder(0)] public string script_name;
		[MikanFieldOrder(1)] public string trigger_name;
		[MikanFieldOrder(2)] public Dictionary<string, string> trigger_args;

		public InvokeScriptTrigger()
		{
			requestTypeName = "InvokeScriptTrigger";
		}
	};

	public class SendScriptMessage : MikanRequest
	{
		[MikanFieldOrder(0)] public MikanScriptMessageInfo message;

		public SendScriptMessage()
		{
			requestTypeName = "SendScriptMessage";
		}
	};

}
