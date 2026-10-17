#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanTransformTypes.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanShapeTypes.rfkh.h"
#endif

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
						Serialization::ClientSystemName("QuadShapeSystem")) MikanQuadShapeSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanQuadShapeSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
						Serialization::ClientSystemName("BoxShapeSystem")) MikanBoxShapeSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanBoxShapeSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
						Serialization::ClientSystemName("ModelShapeSystem")) MikanModelShapeSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanModelShapeSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanShapeTypes")) MikanShapeComponentValues
	: public MikanTransformComponentValues
{
	FIELD() Serialization::String shape_graph_path;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanShapeComponentValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
		   Serialization::ClientComponentNames("QuadShapeComponent", "QuadShapeSystem")) MikanQuadShapeComponentValues
	: public MikanShapeComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() float quad_width= 0.0f;
	FIELD() float quad_height= 0.0f;
	FIELD() bool is_double_sided= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanQuadShapeComponentValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
		   Serialization::ClientComponentNames("BoxShapeComponent", "BoxShapeSystem")) MikanBoxShapeComponentValues
	: public MikanShapeComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() float box_x_size= 0.0f;
	FIELD() float box_y_size= 0.0f;
	FIELD() float box_z_size= 0.0f;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanBoxShapeComponentValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanShapeTypes"),
						Serialization::ClientComponentNames("ModelShapeComponent",
															"ModelShapeSystem")) MikanModelShapeComponentValues
	: public MikanShapeComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String model_path;
	// Read-only. The server-relative path of the model's GLB, joined to the HTTP base URL, or empty
	// when no model is loaded. Its ?v= query changes whenever the served model does.
	FIELD() Serialization::String geometry_url;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanModelShapeComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanShapeTypes_GENERATED
#endif
