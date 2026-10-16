#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanTransformTypes.h"
#include "SerializableList.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanStageTypes.rfkh.h"
#endif

enum class ENUM(Serialization::CodeGenModule("MikanStageTypes")) MikanStageTrackingVolume
{
	StaticMarker ENUMVALUE_STRING("StaticMarker")= 0,
	SteamVR ENUMVALUE_STRING("SteamVR")= 1
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanStageTypes"),
		   Serialization::ClientComponentNames("StageComponent", "StageObjectSystem")) MikanStageComponentValues
	: public MikanTransformComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanTrackingVolumeID tracking_volume_id= INVALID_MIKAN_ID;

	FIELD() MikanVector3f stage_bounds_min;

	FIELD() MikanVector3f stage_bounds_max;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanStageComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanStageTypes_GENERATED
#endif