// This file is auto generated. DO NO EDIT.
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;

namespace MikanXR
{
	public enum MikanPropertyNotifyMode
	{
		NONE= 0,
		NAME= 1,
		NAME_AND_VALUE= 2,
	};

	internal static class MikanPropertyNotifyModeJson
	{
		public static JToken ToJson(MikanPropertyNotifyMode value)
		{
			switch (value)
			{
				case MikanPropertyNotifyMode.NONE: return MikanJson.WriteString("NONE");
				case MikanPropertyNotifyMode.NAME: return MikanJson.WriteString("NAME");
				case MikanPropertyNotifyMode.NAME_AND_VALUE: return MikanJson.WriteString("NAME_AND_VALUE");
			}
			throw new ArgumentOutOfRangeException(nameof(value), "MikanPropertyNotifyMode has no wire name for " + (long)value);
		}

		public static MikanPropertyNotifyMode FromJson(JToken token)
		{
			if (token.Type == JTokenType.Integer)
			{
				return (MikanPropertyNotifyMode)(long)token;
			}

			string name = MikanJson.ReadEnumName(token);
			switch (name)
			{
				case "NONE": return MikanPropertyNotifyMode.NONE;
				case "NAME": return MikanPropertyNotifyMode.NAME;
				case "NAME_AND_VALUE": return MikanPropertyNotifyMode.NAME_AND_VALUE;
			}
			throw new Exception("MikanPropertyNotifyMode has no value named " + name);
		}
	};

	public class ComponentGetValuesRequest : MikanRequest
	{
		public string ownerSystem;
		public int componentId;

		public ComponentGetValuesRequest()
		{
			requestTypeName = "ComponentGetValuesRequest";
		}

		public override string ResponseTypeName => nameof(ComponentGetValuesResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentId": value = this.componentId; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class ComponentGetValuesResponse : MikanResponse
	{
		public string ownerSystem;
		public string componentClassName;
		public PolymorphicObject valuesObject;

		public ComponentGetValuesResponse()
		{
			responseTypeName = "ComponentGetValuesResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentClassName", MikanJson.WriteString(this.componentClassName));
			json.Add("valuesObject", MikanJson.WritePolymorphic(this.valuesObject));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentClassName = MikanJson.ReadString(MikanJson.Field(json, "componentClassName"));
			this.valuesObject = MikanJson.ReadPolymorphic(MikanJson.Field(json, "valuesObject"), this.valuesObject);
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentClassName": value = this.componentClassName; return true;
				case "valuesObject": value = this.valuesObject; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentClassName": return MikanJson.TryAssign(value, ref this.componentClassName);
				case "valuesObject": return MikanJson.TryAssign(value, ref this.valuesObject);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class ComponentListResponse : MikanResponse
	{
		public List<int> componentIdList;

		public ComponentListResponse()
		{
			responseTypeName = "ComponentListResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("componentIdList", MikanJson.WriteList(this.componentIdList, e0 => MikanJson.WriteInt(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.componentIdList = MikanJson.ReadList(MikanJson.Field(json, "componentIdList"), this.componentIdList, e0 => MikanJson.ReadInt(e0));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "componentIdList": value = this.componentIdList; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "componentIdList": return MikanJson.TryAssign(value, ref this.componentIdList);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetComponentListRequest : MikanRequest
	{
		public string ownerSystem;
		public string componentClassName;

		public GetComponentListRequest()
		{
			requestTypeName = "GetComponentListRequest";
		}

		public override string ResponseTypeName => nameof(ComponentListResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentClassName", MikanJson.WriteString(this.componentClassName));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentClassName = MikanJson.ReadString(MikanJson.Field(json, "componentClassName"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentClassName": value = this.componentClassName; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentClassName": return MikanJson.TryAssign(value, ref this.componentClassName);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class GetPropertyDescriptors : MikanRequest
	{
		public string systemFilter;
		public string componentFilter;
		public string propertyFilter;

		public GetPropertyDescriptors()
		{
			requestTypeName = "GetPropertyDescriptors";
		}

		public override string ResponseTypeName => nameof(PropertyDescriptorResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("systemFilter", MikanJson.WriteString(this.systemFilter));
			json.Add("componentFilter", MikanJson.WriteString(this.componentFilter));
			json.Add("propertyFilter", MikanJson.WriteString(this.propertyFilter));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.systemFilter = MikanJson.ReadString(MikanJson.Field(json, "systemFilter"));
			this.componentFilter = MikanJson.ReadString(MikanJson.Field(json, "componentFilter"));
			this.propertyFilter = MikanJson.ReadString(MikanJson.Field(json, "propertyFilter"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "systemFilter": value = this.systemFilter; return true;
				case "componentFilter": value = this.componentFilter; return true;
				case "propertyFilter": value = this.propertyFilter; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "systemFilter": return MikanJson.TryAssign(value, ref this.systemFilter);
				case "componentFilter": return MikanJson.TryAssign(value, ref this.componentFilter);
				case "propertyFilter": return MikanJson.TryAssign(value, ref this.propertyFilter);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class PropertyDescriptorResponse : MikanResponse
	{
		public List<MikanPropertyDescriptor> descriptor_list;

		public PropertyDescriptorResponse()
		{
			responseTypeName = "PropertyDescriptorResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("descriptor_list", MikanJson.WriteList(this.descriptor_list, e0 => MikanJson.WriteObject(e0)));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.descriptor_list = MikanJson.ReadList(MikanJson.Field(json, "descriptor_list"), this.descriptor_list, e0 => MikanJson.ReadObject(e0, new MikanPropertyDescriptor()));
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

	public class PropertyGetValueRequest : MikanRequest
	{
		public string ownerSystem;
		public int componentId;
		public string fieldName;

		public PropertyGetValueRequest()
		{
			requestTypeName = "PropertyGetValueRequest";
		}

		public override string ResponseTypeName => nameof(PropertyGetValueResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
			json.Add("fieldName", MikanJson.WriteString(this.fieldName));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
			this.fieldName = MikanJson.ReadString(MikanJson.Field(json, "fieldName"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentId": value = this.componentId; return true;
				case "fieldName": value = this.fieldName; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
				case "fieldName": return MikanJson.TryAssign(value, ref this.fieldName);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class PropertyGetValueResponse : MikanResponse
	{
		public MikanPropertyValue propertyValue;

		public PropertyGetValueResponse()
		{
			responseTypeName = "PropertyGetValueResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("propertyValue", MikanJson.WriteObject(this.propertyValue));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.propertyValue = MikanJson.ReadObject(MikanJson.Field(json, "propertyValue"), this.propertyValue ?? new MikanPropertyValue());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "propertyValue": value = this.propertyValue; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "propertyValue": return MikanJson.TryAssign(value, ref this.propertyValue);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class PropertySetValueRequest : MikanRequest
	{
		public string ownerSystem;
		public int componentId;
		public string fieldName;
		public MikanVariant fieldValue;

		public PropertySetValueRequest()
		{
			requestTypeName = "PropertySetValueRequest";
		}

		public override string ResponseTypeName => nameof(PropertySetValueResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
			json.Add("fieldName", MikanJson.WriteString(this.fieldName));
			json.Add("fieldValue", MikanJson.WriteObject(this.fieldValue));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
			this.fieldName = MikanJson.ReadString(MikanJson.Field(json, "fieldName"));
			this.fieldValue = MikanJson.ReadObject(MikanJson.Field(json, "fieldValue"), this.fieldValue ?? new MikanVariant());
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentId": value = this.componentId; return true;
				case "fieldName": value = this.fieldName; return true;
				case "fieldValue": value = this.fieldValue; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
				case "fieldName": return MikanJson.TryAssign(value, ref this.fieldName);
				case "fieldValue": return MikanJson.TryAssign(value, ref this.fieldValue);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class PropertySetValueResponse : MikanResponse
	{

		public PropertySetValueResponse()
		{
			responseTypeName = "PropertySetValueResponse";
		}
	};

	public class SetPropertyNotifyMode : MikanRequest
	{
		public string systemFilter;
		public string componentFilter;
		public string propertyFilter;
		public MikanPropertyNotifyMode notifyMode;

		public SetPropertyNotifyMode()
		{
			requestTypeName = "SetPropertyNotifyMode";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("systemFilter", MikanJson.WriteString(this.systemFilter));
			json.Add("componentFilter", MikanJson.WriteString(this.componentFilter));
			json.Add("propertyFilter", MikanJson.WriteString(this.propertyFilter));
			json.Add("notifyMode", MikanPropertyNotifyModeJson.ToJson(this.notifyMode));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.systemFilter = MikanJson.ReadString(MikanJson.Field(json, "systemFilter"));
			this.componentFilter = MikanJson.ReadString(MikanJson.Field(json, "componentFilter"));
			this.propertyFilter = MikanJson.ReadString(MikanJson.Field(json, "propertyFilter"));
			this.notifyMode = MikanPropertyNotifyModeJson.FromJson(MikanJson.Field(json, "notifyMode"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "systemFilter": value = this.systemFilter; return true;
				case "componentFilter": value = this.componentFilter; return true;
				case "propertyFilter": value = this.propertyFilter; return true;
				case "notifyMode": value = this.notifyMode; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "systemFilter": return MikanJson.TryAssign(value, ref this.systemFilter);
				case "componentFilter": return MikanJson.TryAssign(value, ref this.componentFilter);
				case "propertyFilter": return MikanJson.TryAssign(value, ref this.propertyFilter);
				case "notifyMode": return MikanJson.TryAssign(value, ref this.notifyMode);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SystemCreateObjectRequest : MikanRequest
	{
		public string ownerSystem;
		public string componentClassName;
		public PolymorphicObject initParams;

		public SystemCreateObjectRequest()
		{
			requestTypeName = "SystemCreateObjectRequest";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentClassName", MikanJson.WriteString(this.componentClassName));
			json.Add("initParams", MikanJson.WritePolymorphic(this.initParams));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentClassName = MikanJson.ReadString(MikanJson.Field(json, "componentClassName"));
			this.initParams = MikanJson.ReadPolymorphic(MikanJson.Field(json, "initParams"), this.initParams);
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentClassName": value = this.componentClassName; return true;
				case "initParams": value = this.initParams; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentClassName": return MikanJson.TryAssign(value, ref this.componentClassName);
				case "initParams": return MikanJson.TryAssign(value, ref this.initParams);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SystemDestroyObjectRequest : MikanRequest
	{
		public string ownerSystem;
		public string componentClassName;
		public int componentId;

		public SystemDestroyObjectRequest()
		{
			requestTypeName = "SystemDestroyObjectRequest";
		}

		public override string ResponseTypeName => nameof(MikanResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("componentClassName", MikanJson.WriteString(this.componentClassName));
			json.Add("componentId", MikanJson.WriteInt(this.componentId));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.componentClassName = MikanJson.ReadString(MikanJson.Field(json, "componentClassName"));
			this.componentId = MikanJson.ReadInt(MikanJson.Field(json, "componentId"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "componentClassName": value = this.componentClassName; return true;
				case "componentId": value = this.componentId; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "componentClassName": return MikanJson.TryAssign(value, ref this.componentClassName);
				case "componentId": return MikanJson.TryAssign(value, ref this.componentId);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SystemGetValuesRequest : MikanRequest
	{
		public string ownerSystem;

		public SystemGetValuesRequest()
		{
			requestTypeName = "SystemGetValuesRequest";
		}

		public override string ResponseTypeName => nameof(SystemGetValuesResponse);

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
			}
			return base.TrySetField(fieldName, value);
		}
	};

	public class SystemGetValuesResponse : MikanResponse
	{
		public string ownerSystem;
		public PolymorphicObject valuesObject;

		public SystemGetValuesResponse()
		{
			responseTypeName = "SystemGetValuesResponse";
		}

		public override void WriteFields(JObject json)
		{
			base.WriteFields(json);
			json.Add("ownerSystem", MikanJson.WriteString(this.ownerSystem));
			json.Add("valuesObject", MikanJson.WritePolymorphic(this.valuesObject));
		}

		public override void ReadFields(JObject json)
		{
			base.ReadFields(json);
			this.ownerSystem = MikanJson.ReadString(MikanJson.Field(json, "ownerSystem"));
			this.valuesObject = MikanJson.ReadPolymorphic(MikanJson.Field(json, "valuesObject"), this.valuesObject);
		}

		public override bool TryGetField(string fieldName, out object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": value = this.ownerSystem; return true;
				case "valuesObject": value = this.valuesObject; return true;
			}
			return base.TryGetField(fieldName, out value);
		}

		public override bool TrySetField(string fieldName, object value)
		{
			switch (fieldName)
			{
				case "ownerSystem": return MikanJson.TryAssign(value, ref this.ownerSystem);
				case "valuesObject": return MikanJson.TryAssign(value, ref this.valuesObject);
			}
			return base.TrySetField(fieldName, value);
		}
	};

}
