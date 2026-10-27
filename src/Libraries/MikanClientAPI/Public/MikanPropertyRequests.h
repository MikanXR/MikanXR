#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanPropertyTypes.h"
#include "MikanVariantTypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanPropertyRequests.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("PropertySetValueResponse")) PropertySetValueRequest
	: public MikanRequest
{
public:
	PropertySetValueRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(PropertySetValueRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() int componentId= INVALID_MIKAN_ID;
	FIELD() Serialization::String fieldName;
	FIELD() MikanVariant fieldValue;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PropertySetValueRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("PropertyGetValueResponse")) PropertyGetValueRequest
	: public MikanRequest
{
public:
	PropertyGetValueRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(PropertyGetValueRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() int componentId= INVALID_MIKAN_ID;
	FIELD() Serialization::String fieldName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PropertyGetValueRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("ComponentGetValuesResponse")) ComponentGetValuesRequest
	: public MikanRequest
{
public:
	ComponentGetValuesRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(ComponentGetValuesRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() int componentId= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	ComponentGetValuesRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("ComponentListResponse")) GetComponentListRequest
	: public MikanRequest
{
public:
	GetComponentListRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(GetComponentListRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::String componentClassName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetComponentListRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("SystemGetValuesResponse")) SystemGetValuesRequest
	: public MikanRequest
{
public:
	SystemGetValuesRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(SystemGetValuesRequest)}

	FIELD() Serialization::String ownerSystem;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SystemGetValuesRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) SystemCreateObjectRequest
	: public MikanRequest
{
public:
	SystemCreateObjectRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(SystemCreateObjectRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::String componentClassName;
	FIELD() Serialization::PolymorphicObjectPtr initParams;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SystemCreateObjectRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) SystemDestroyObjectRequest
	: public MikanRequest
{
public:
	SystemDestroyObjectRequest(){MIKAN_REQUEST_TYPE_INFO_INIT(SystemDestroyObjectRequest)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::String componentClassName;
	FIELD() int componentId= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SystemDestroyObjectRequest_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) SetPropertyNotifyMode
	: public MikanRequest
{
public:
	SetPropertyNotifyMode(){MIKAN_REQUEST_TYPE_INFO_INIT(SetPropertyNotifyMode)}

	FIELD() Serialization::String systemFilter;

	FIELD() Serialization::String componentFilter;

	FIELD() Serialization::String propertyFilter;

	FIELD() MikanPropertyNotifyMode notifyMode= MikanPropertyNotifyMode::NONE;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SetPropertyNotifyMode_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests"),
						Serialization::ResponseType("PropertyDescriptorResponse")) GetPropertyDescriptors
	: public MikanRequest
{
public:
	GetPropertyDescriptors(){MIKAN_REQUEST_TYPE_INFO_INIT(GetPropertyDescriptors)}

	FIELD() Serialization::String systemFilter;

	FIELD() Serialization::String componentFilter;

	FIELD() Serialization::String propertyFilter;

#ifdef MIKANAPI_REFLECTION_ENABLED
	GetPropertyDescriptors_GENERATED
#endif
};

// Property Response Types
// ------

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) PropertySetValueResponse
	: public MikanResponse
{
public:
	PropertySetValueResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(PropertySetValueResponse)}

#ifdef MIKANAPI_REFLECTION_ENABLED
	PropertySetValueResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) PropertyGetValueResponse
	: public MikanResponse
{
public:
	PropertyGetValueResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(PropertyGetValueResponse)}

	FIELD() MikanPropertyValue propertyValue;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PropertyGetValueResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) ComponentGetValuesResponse
	: public MikanResponse
{
public:
	ComponentGetValuesResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(ComponentGetValuesResponse)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::String componentClassName;
	FIELD() Serialization::PolymorphicObjectPtr valuesObject;

#ifdef MIKANAPI_REFLECTION_ENABLED
	ComponentGetValuesResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) ComponentListResponse
	: public MikanResponse
{

	ComponentListResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(ComponentListResponse)}

	FIELD() Serialization::List<MikanComponentID> componentIdList;

#ifdef MIKANAPI_REFLECTION_ENABLED
	ComponentListResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) SystemGetValuesResponse
	: public MikanResponse
{
public:
	SystemGetValuesResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(SystemGetValuesResponse)}

	FIELD() Serialization::String ownerSystem;
	FIELD() Serialization::PolymorphicObjectPtr valuesObject;

#ifdef MIKANAPI_REFLECTION_ENABLED
	SystemGetValuesResponse_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanPropertyRequests")) PropertyDescriptorResponse
	: public MikanResponse
{
	PropertyDescriptorResponse(){MIKAN_RESPONSE_TYPE_INFO_INIT(PropertyDescriptorResponse)}

	FIELD() Serialization::List<MikanPropertyDescriptor> descriptor_list;

#ifdef MIKANAPI_REFLECTION_ENABLED
	PropertyDescriptorResponse_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanPropertyRequests_GENERATED
#endif
