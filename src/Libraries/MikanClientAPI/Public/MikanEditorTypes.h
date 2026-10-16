#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanPropertyTypes.h"
#include "SerializableString.h"
#include "SerializableList.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanEditorTypes.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanEditorTypes"),
						Serialization::ClientSystemName("EditorObjectSystem")) MikanEditorSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

	FIELD() bool render_origin= false;
	FIELD() bool render_anchors= false;
	FIELD() bool render_quad_stencils= false;
	FIELD() bool render_box_stencils= false;
	FIELD() bool render_model_stencils= false;
	FIELD() float camera_speed= 0.f;
	FIELD() Serialization::String selected_language;
	FIELD() Serialization::List<Serialization::String> available_language_list;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanEditorSystemValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanEditorTypes_GENERATED
#endif