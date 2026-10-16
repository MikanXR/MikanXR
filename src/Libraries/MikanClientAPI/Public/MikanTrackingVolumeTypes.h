#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanComponentTypes.h"
#include "MikanMathTypes.h"
#include "SerializableList.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanTrackingVolumeTypes.rfkh.h"
#endif

#include <assert.h>

// Enums
enum ENUM(Serialization::CodeGenModule("MikanTrackingVolumeTypes")) MikanTrackingVolumeType
{
	MikanTrackingVolumeType_INVALID ENUMVALUE_STRING("INVALID")= -1,
	MikanTrackingVolumeType_marker ENUMVALUE_STRING("marker"),
	MikanTrackingVolumeType_vr ENUMVALUE_STRING("vr"),
};

enum ENUM(Serialization::CodeGenModule("MikanTrackingVolumeTypes")) MikanTrackingRuntime
{
	MikanTrackingRuntime_INVALID ENUMVALUE_STRING("INVALID")= -1,
	MikanTrackingRuntime_SteamVR ENUMVALUE_STRING("SteamVR"),
};

enum ENUM(Serialization::CodeGenModule("MikanTrackingVolumeTypes")) MikanTrackingSpace
{
	MikanTrackingSpace_INVALID ENUMVALUE_STRING("INVALID")= -1,
	MikanTrackingSpace_Stage ENUMVALUE_STRING("StageSpace"),
	MikanTrackingSpace_VR ENUMVALUE_STRING("VRSpace"),
};

// Structures
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTrackingVolumeTypes")) MikanTrackingVolumeComponentValues
	: public MikanComponentValues
{
	FIELD() MikanMarkerID origin_marker_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanTrackingVolumeComponentValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanTrackingVolumeTypes"),
		   Serialization::ClientComponentNames("MarkerTrackingVolumeComponent",
											   "MarkerTrackingVolumeSystem")) MikanMarkerTrackingVolumeComponentValues
	: public MikanTrackingVolumeComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanMarkerTrackingVolumeComponentValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTrackingVolumeTypes"),
						Serialization::ClientComponentNames(
							"VRTrackingVolumeComponent", "VRTrackingVolumeSystem")) MikanVRTrackingVolumeComponentValues
	: public MikanTrackingVolumeComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanTrackingRuntime tracking_runtime= MikanTrackingRuntime_INVALID;
	FIELD() MikanTrackingMountID charuco_mount_id= INVALID_MIKAN_ID;
	FIELD() MikanVector3f charuco_mount_offset_mm;
	FIELD() MikanMarkerID utility_marker_id= INVALID_MIKAN_ID;
	FIELD() Serialization::List<MikanTrackingMountID> tracking_mount_ids;
	FIELD() MikanMatrix4f vr_space_to_stage_space;
	FIELD() MikanTrackingSpace display_tracking_space= MikanTrackingSpace_INVALID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanVRTrackingVolumeComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanTrackingVolumeTypes_GENERATED
#endif
