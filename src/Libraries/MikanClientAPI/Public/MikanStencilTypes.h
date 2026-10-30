#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanPropertyTypes.h"
#include "MikanTransformTypes.h"
#include "SerializableList.h"
#include "SerializableString.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanStencilTypes.rfkh.h"
#endif

enum ENUM(Serialization::CodeGenModule("MikanStencilTypes")) MikanStencilCullMode
{
	MikanStencilCullMode_NONE ENUMVALUE_STRING("NONE"),
	MikanStencilCullMode_Z_AXIS ENUMVALUE_STRING("Z_Axis"),
	MikanStencilCullMode_Y_AXIS ENUMVALUE_STRING("Y_Axis"),
	MikanStencilCullMode_X_AXIS ENUMVALUE_STRING("X_Axis"),
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientSystemName("QuadStencilSystem")) MikanQuadStencilSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanQuadStencilSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientSystemName("BoxStencilSystem")) MikanBoxStencilSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanBoxStencilSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientSystemName("ModelStencilSystem")) MikanModelStencilSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanModelStencilSystemValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes")) MikanStencilComponentValues
	: public MikanTransformComponentValues
{
	FIELD() bool is_disabled= false;
	FIELD() MikanStencilCullMode cull_mode= MikanStencilCullMode_NONE;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanStencilComponentValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientComponentNames("QuadStencilComponent",
															"QuadStencilSystem")) MikanQuadStencilComponentValues
	: public MikanStencilComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() float quad_width= 0.0f;
	FIELD() float quad_height= 0.0f;
	FIELD() bool is_double_sided= false;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanQuadStencilComponentValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientComponentNames("BoxStencilComponent",
															"BoxStencilSystem")) MikanBoxStencilComponentValues
	: public MikanStencilComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() float box_x_size= 0.0f;
	FIELD() float box_y_size= 0.0f;
	FIELD() float box_z_size= 0.0f;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanBoxStencilComponentValues_GENERATED
#endif
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanStencilTypes"),
						Serialization::ClientComponentNames("ModelStencilComponent",
															"ModelStencilSystem")) MikanModelStencilComponentValues
	: public MikanStencilComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String model_path;
	// Read-only. The server-relative path of the model's GLB, joined to the HTTP base URL, or empty
	// when no model is loaded. Its ?v= query changes whenever the served model does.
	FIELD() Serialization::String geometry_url;
	// When true the client draws the stencil as a holdout occluder that writes depth and clears
	// coverage, and in its published depth. When false the client draws neither, keeps its shadow
	// catcher, and the editor's compositor decides occlusion per pixel against the client depth.
	FIELD() bool client_holdout= true;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanModelStencilComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanStencilTypes_GENERATED
#endif