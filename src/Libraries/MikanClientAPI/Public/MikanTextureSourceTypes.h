#pragma once

#include "MikanAPIExport.h"
#include "MikanComponentTypes.h"
#include "SerializableString.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanTextureSourceTypes.rfkh.h"
#endif

// -- Structures -----
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTextureSourceTypes")) MikanTextureSourceValues
	: public MikanComponentValues
{
#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanTextureSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTextureSourceTypes"),
						Serialization::ClientComponentNames("ClientTextureSourceComponent",
															"ClientTextureSourceSystem")) MikanClientTextureSourceValues
	: public MikanTextureSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String client_source; ///< The name of the client video source

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanClientTextureSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTextureSourceTypes"),
						Serialization::ClientComponentNames("SpoutTextureSourceComponent",
															"SpoutTextureSourceSystem")) MikanSpoutTextureSourceValues
	: public MikanTextureSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String spout_source; ///< The name of the spout video source

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanSpoutTextureSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanTextureSourceTypes"),
						Serialization::ClientComponentNames("CEFTextureSourceComponent",
															"CEFTextureSourceSystem")) MikanCEFTextureSourceValues
	: public MikanTextureSourceValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String url; ///< The URL of the web page to render

	FIELD() int width= 1280; ///< Pixel buffer width

	FIELD() int height= 720; ///< Pixel buffer height

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanCEFTextureSourceValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanTextureSourceTypes_GENERATED
#endif