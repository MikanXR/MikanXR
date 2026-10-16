#pragma once

#include "MikanAPIExport.h"
#include "MikanAPITypes.h"
#include "MikanComponentTypes.h"
#include "MikanPropertyTypes.h"
#include "SerializationProperty.h"

#ifdef MIKANAPI_REFLECTION_ENABLED
#include "MikanMarkerTypes.rfkh.h"
#endif

#include <assert.h>

enum class ENUM(Serialization::CodeGenModule("MikanMarkerTypes")) MikanMarkerDictionaryType : int
{
	INVALID ENUMVALUE_STRING("INVALID")= -1,

	DICT_4X4 ENUMVALUE_STRING("DICT_4x4"),
	DICT_5X5 ENUMVALUE_STRING("DICT_5x5"),
	DICT_6X6 ENUMVALUE_STRING("DICT_6x6"),
	DICT_7X7 ENUMVALUE_STRING("DICT_7x7"),
};

// Structures
struct MIKAN_API STRUCT(Serialization::CodeGenModule("MikanMarkerTypes"),
						Serialization::ClientSystemName("MarkerObjectSystem")) MikanMarkerSystemValues
	: public MikanSystemValues
{
	static const char* k_systemName;

	FIELD() Serialization::List<int> aruco_id_list;
	FIELD() MikanMarkerDictionaryType aruco_dictionary_type= MikanMarkerDictionaryType::INVALID;
	FIELD() int charuco_rows= 0;
	FIELD() int charuco_cols= 0;
	FIELD() float charuco_square_length_mm= 0.f;
	FIELD() float charuco_marker_length_mm= 0.f;
	FIELD() MikanMarkerDictionaryType charuco_dictionary_type= MikanMarkerDictionaryType::INVALID;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanMarkerSystemValues_GENERATED
#endif
};

struct MIKAN_API
	STRUCT(Serialization::CodeGenModule("MikanMarkerTypes"),
		   Serialization::ClientComponentNames("MarkerComponent", "MarkerObjectSystem")) MikanMarkerComponentValues
	: public MikanComponentValues
{
	static const char* k_componentClassName;
	static const char* k_ownerSystemName;

	FIELD() int aruco_id= INVALID_MIKAN_ID;
	FIELD() float length_mm= 0.f;

#ifdef MIKANAPI_REFLECTION_ENABLED
	MikanMarkerComponentValues_GENERATED
#endif
};

#ifdef MIKANAPI_REFLECTION_ENABLED
File_MikanMarkerTypes_GENERATED
#endif
