// This file is auto generated. DO NO EDIT.
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

	public class ComponentGetValuesRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public int componentId;

		public ComponentGetValuesRequest()
		{
			requestTypeName = "ComponentGetValuesRequest";
		}
	};

	public class ComponentGetValuesResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public string componentClassName;
		[MikanFieldOrder(2)] public PolymorphicObject valuesObject;

		public ComponentGetValuesResponse()
		{
			responseTypeName = "ComponentGetValuesResponse";
		}
	};

	public class ComponentListResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public List<int> componentIdList;

		public ComponentListResponse()
		{
			responseTypeName = "ComponentListResponse";
		}
	};

	public class GetComponentListRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public string componentClassName;

		public GetComponentListRequest()
		{
			requestTypeName = "GetComponentListRequest";
		}
	};

	public class GetPropertyDescriptors : MikanRequest
	{
		[MikanFieldOrder(0)] public string systemFilter;
		[MikanFieldOrder(1)] public string componentFilter;
		[MikanFieldOrder(2)] public string propertyFilter;

		public GetPropertyDescriptors()
		{
			requestTypeName = "GetPropertyDescriptors";
		}
	};

	public class PropertyDescriptorResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public List<MikanPropertyDescriptor> descriptor_list;

		public PropertyDescriptorResponse()
		{
			responseTypeName = "PropertyDescriptorResponse";
		}
	};

	public class PropertyGetValueRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public int componentId;
		[MikanFieldOrder(2)] public string fieldName;

		public PropertyGetValueRequest()
		{
			requestTypeName = "PropertyGetValueRequest";
		}
	};

	public class PropertyGetValueResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public MikanPropertyValue propertyValue;

		public PropertyGetValueResponse()
		{
			responseTypeName = "PropertyGetValueResponse";
		}
	};

	public class PropertySetValueRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public int componentId;
		[MikanFieldOrder(2)] public string fieldName;
		[MikanFieldOrder(3)] public MikanVariant fieldValue;

		public PropertySetValueRequest()
		{
			requestTypeName = "PropertySetValueRequest";
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
		[MikanFieldOrder(0)] public string systemFilter;
		[MikanFieldOrder(1)] public string componentFilter;
		[MikanFieldOrder(2)] public string propertyFilter;
		[MikanFieldOrder(3)] public MikanPropertyNotifyMode notifyMode;

		public SetPropertyNotifyMode()
		{
			requestTypeName = "SetPropertyNotifyMode";
		}
	};

	public class SystemCreateObjectRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public string componentClassName;
		[MikanFieldOrder(2)] public PolymorphicObject initParams;

		public SystemCreateObjectRequest()
		{
			requestTypeName = "SystemCreateObjectRequest";
		}
	};

	public class SystemDestroyObjectRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public string componentClassName;
		[MikanFieldOrder(2)] public int componentId;

		public SystemDestroyObjectRequest()
		{
			requestTypeName = "SystemDestroyObjectRequest";
		}
	};

	public class SystemGetValuesRequest : MikanRequest
	{
		[MikanFieldOrder(0)] public string ownerSystem;

		public SystemGetValuesRequest()
		{
			requestTypeName = "SystemGetValuesRequest";
		}
	};

	public class SystemGetValuesResponse : MikanResponse
	{
		[MikanFieldOrder(0)] public string ownerSystem;
		[MikanFieldOrder(1)] public PolymorphicObject valuesObject;

		public SystemGetValuesResponse()
		{
			responseTypeName = "SystemGetValuesResponse";
		}
	};

}
