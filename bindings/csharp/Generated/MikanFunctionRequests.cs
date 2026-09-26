// This file is auto generated. DO NO EDIT.
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class FunctionDescriptorResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public List<MikanFunctionDescriptor> descriptor_list;

		public FunctionDescriptorResponse()
		{
			responseTypeName = "FunctionDescriptorResponse";
		}
	};

	public class GetFunctionListRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string systemFilter;
		[MikanFieldOrder(1)] public string componentFilter;

		public GetFunctionListRequest()
		{
			requestTypeName = "GetFunctionListRequest";
		}
	};

	public class InvokeComponentFunctionRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public int componentId;
		[MikanFieldOrder(2)] public string functionName;

		public InvokeComponentFunctionRequest()
		{
			requestTypeName = "InvokeComponentFunctionRequest";
		}
	};

	public class InvokeSystemFunctionRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public string functionName;

		public InvokeSystemFunctionRequest()
		{
			requestTypeName = "InvokeSystemFunctionRequest";
		}
	};

}
