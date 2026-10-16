#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanComponentTypes.h"
#include "SerializableString.h"
#include "SerializableList.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanTrackingMountTypes.rfkh.h"
#endif

#include <assert.h>

// Structures
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTrackingMountTypes"),
						Serialization::ClientComponentNames(
							"TrackingMountComponent", "TrackingMountObjectSystem")) MikanTrackingMountComponentValues
	: public MikanComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String device_path;
	FIELD() Serialization::String socket_name;
	FIELD() Serialization::List<Serialization::String> available_socket_names;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanTrackingMountComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanTrackingMountTypes_GENERATED
#endif
