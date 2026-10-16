#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanTransformTypes.h"
#include "MikanPropertyTypes.h"
#include "SerializableList.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanAnchorTypes.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanAnchorTypes"),
						Serialization::ClientSystemName("AnchorObjectSystem")) MikanAnchorSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanAnchorSystemValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanAnchorTypes"),
		   Serialization::ClientComponentNames("AnchorComponent", "AnchorObjectSystem")) MikanAnchorComponentValues
	: public MikanTransformComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanAnchorComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanAnchorTypes_GENERATED
#endif