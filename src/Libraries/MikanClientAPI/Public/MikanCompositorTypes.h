#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanComponentTypes.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanCompositorTypes.rfkh.h"
#endif

#include <assert.h>

// Structures
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanCompositorTypes"),
						Serialization::ClientComponentNames("CompositorComponent",
															"CompositorObjectSystem")) MikanCompositorComponentValues
	: public MikanComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanSceneID owner_scene_id= INVALID_MIKAN_ID;
	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;
	FIELD() Serialization::String compositor_graph_path;
	FIELD() bool spout_enable_output= false;
	FIELD() Serialization::String spout_output_name;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCompositorComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanCompositorTypes_GENERATED
#endif
