// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class InvokeScriptTrigger : MikanRequest
	{
		public string script_name;
		public string trigger_name;
		public Dictionary<string, string> trigger_args;

		public InvokeScriptTrigger()
		{
			requestTypeName = "InvokeScriptTrigger";
		}
	};

	public class SendScriptMessage : MikanRequest
	{
		public MikanScriptMessageInfo message;

		public SendScriptMessage()
		{
			requestTypeName = "SendScriptMessage";
		}
	};

}
