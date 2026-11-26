#pragma once

#include "MikanAPIExport.h"
#include "MikanComponentTypes.h"
#include "SerializableString.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanOutputTypes.rfkh.h"
#endif

// -- Structures -----

// What every project output shares. An output belongs to the project, not to a
// scene: the current scene's compositor feeds every enabled one.
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanOutputTypes")) MikanOutputComponentValues
	: public MikanComponentValues
{
	FIELD() bool enabled= true; ///< Whether the output takes the composited frame

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanOutputComponentValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanOutputTypes"),
						Serialization::ClientComponentNames("SpoutOutputComponent",
															"SpoutOutputSystem")) MikanSpoutOutputComponentValues
	: public MikanOutputComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() Serialization::String spout_output_name; ///< The name receivers find the sender by

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanSpoutOutputComponentValues_GENERATED
#endif // MIKANAPI_REFLECTION_ENABLED
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanOutputTypes_GENERATED
#endif
