#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanTransformTypes.h"
#include "MikanPropertyTypes.h"
#include "SerializableList.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanSceneTypes.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanSceneTypes"),
						Serialization::ClientSystemName("SceneObjectSystem")) MikanSceneSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

	FIELD() MikanSceneID current_scene_id= INVALID_MIKAN_ID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanSceneSystemValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanSceneTypes"),
		   Serialization::ClientComponentNames("SceneComponent", "SceneObjectSystem")) MikanSceneComponentValues
	: public MikanTransformComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() MikanCompositorID display_compositor_id= INVALID_MIKAN_ID;

	// Draws this scene's geometry even while another scene is the current one.
	// Without it only the current scene's subtree renders.
	FIELD() bool force_render= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanSceneComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanSceneTypes_GENERATED
#endif