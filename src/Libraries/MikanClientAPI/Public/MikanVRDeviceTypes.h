#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanTransformTypes.h"
#include "MikanPropertyTypes.h"
#include "SerializableList.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanVRDeviceTypes.rfkh.h"
#endif

// Shared Constants
//-----------------

/// The list of possible vr device drivers used by MikanXR Client API
enum class ENUM(Serialization::CodeGenModule("MikanVRDeviceTypes")) MikanVRDeviceApi
{
	INVALID ENUMVALUE_STRING("INVALID"),
	STEAM_VR ENUMVALUE_STRING("STEAM_VR"),
};

/// The list of possible vr device types used by MikanXR Client API
enum class ENUM(Serialization::CodeGenModule("MikanVRDeviceTypes")) MikanVRDeviceType
{
	INVALID ENUMVALUE_STRING("INVALID"),
	HMD ENUMVALUE_STRING("HMD"),
	CONTROLLER ENUMVALUE_STRING("CONTROLLER"),
	TRACKER ENUMVALUE_STRING("TRACKER")
};

// VR System Response Types
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanVRDeviceTypes"),
						Serialization::ClientSystemName("VRObjectSystem")) MikanVRObjectSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

	FIELD() Serialization::List<Serialization::String> vr_device_path_list;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVRObjectSystemValues_GENERATED
#endif
};

// VR Device Response Types
struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanVRDeviceTypes"),
		   Serialization::ClientComponentNames("VRDeviceComponent", "VRObjectSystem")) MikanVRDeviceComponentValues
	: public MikanTransformComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanVRDeviceApi vr_device_api= MikanVRDeviceApi::INVALID;
	FIELD() MikanVRDeviceType vr_device_type= MikanVRDeviceType::INVALID;
	FIELD() int vr_device_index= -1;
	FIELD() Serialization::String vr_device_path;
	FIELD() Serialization::List<Serialization::String> socket_names;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVRDeviceComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanVRDeviceTypes_GENERATED
#endif