// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public class FunctionDescriptorResponse : MikanResponse
	{
		public List<MikanFunctionDescriptor> descriptor_list;

		public FunctionDescriptorResponse()
		{
			responseTypeName = "FunctionDescriptorResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("descriptor_list", MikanJson.WriteList(this.descriptor_list, e0 => MikanJson.WriteObject(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.descriptor_list = MikanJson.ReadList(MikanJson.Field(json, "descriptor_list"), this.descriptor_list, e0 => MikanJson.ReadObject(e0, new MikanFunctionDescriptor()));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "descriptor_list": value = this.descriptor_list; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "descriptor_list": return MikanJson.TryAssign(value, ref this.descriptor_list);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetFunctionListRequest : MikanRequest
	{
		public string systemFilter;
		public string componentFilter;

		public GetFunctionListRequest()
		{
			requestTypeName = "GetFunctionListRequest";
		}

		public override string ResponseTypeName => nameof(FunctionDescriptorResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("systemFilter", MikanJson.WriteString(this.systemFilter));
			json.Add("componentFilter", MikanJson.WriteString(this.componentFilter));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.systemFilter = MikanJson.ReadString(MikanJson.Field(json, "systemFilter"));
			this.componentFilter = MikanJson.ReadString(MikanJson.Field(json, "componentFilter"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "systemFilter": value = this.systemFilter; return true;
				case "componentFilter": value = this.componentFilter; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "systemFilter": return MikanJson.TryAssign(value, ref this.systemFilter);
				case "componentFilter": return MikanJson.TryAssign(value, ref this.componentFilter);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class InvokeComponentFunctionRequest : MikanRequest
	{
		public string ownerSystem;
		public int componentId;
		public string functionName;

		public InvokeComponentFunctionRequest()
		{
			requestTypeName = "InvokeComponentFunctionRequest";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
			json.Add("functionName", MikanJson.WriteString(this.functionName));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
			this.functionName = MikanJson.ReadString(MikanJson.Field(json, "functionName"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentId": value = this.componentId; return true;
				case "functionName": value = this.functionName; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
				case "functionName": return MikanJson.TryAssign(value, ref this.functionName);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class InvokeSystemFunctionRequest : MikanRequest
	{
		public string ownerSystem;
		public string functionName;

		public InvokeSystemFunctionRequest()
		{
			requestTypeName = "InvokeSystemFunctionRequest";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("functionName", MikanJson.WriteString(this.functionName));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.functionName = MikanJson.ReadString(MikanJson.Field(json, "functionName"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "functionName": value = this.functionName; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "functionName": return MikanJson.TryAssign(value, ref this.functionName);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
