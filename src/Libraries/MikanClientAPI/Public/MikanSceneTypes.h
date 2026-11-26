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

	// The camera the scene is shot through. Its compositor takes video and pose from it.
	FIELD() MikanCameraID camera_id= INVALID_MIKAN_ID;

	// The lighting probe captured for the scene, or INVALID_MIKAN_ID before the first capture
	FIELD() MikanLightID light_environment_id= INVALID_MIKAN_ID;

	// The lighting preset the scene falls back to, or INVALID_MIKAN_ID for none. The editor
	// applies it when the scene becomes current and when a sequence on the preset's group ends.
	FIELD() MikanDMXPresetID lighting_preset_id= INVALID_MIKAN_ID;

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