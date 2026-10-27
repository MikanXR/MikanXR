// The client API names exist twice: as the C++ statics in MikanAPI.cpp, for consumers built
// without reflection, and as the ClientComponentNames and ClientSystemName properties the
// codegen emits as constants for C# and TypeScript. These tests hold the two in step: every
// listed struct's statics must equal its property, and every struct that carries a property
// must be listed. The last test holds the requests' ResponseType property to the response structs.
#include "MikanAPITypes.h"
#include "MikanAnchorTypes.h"
#include "MikanCameraTypes.h"
#include "MikanCompositorTypes.h"
#include "MikanEditorTypes.h"
#include "MikanLightTypes.h"
#include "MikanMarkerTypes.h"
#include "MikanSceneTypes.h"
#include "MikanScriptTypes.h"
#include "MikanShapeTypes.h"
#include "MikanStageTypes.h"
#include "MikanStencilTypes.h"
#include "MikanTextureSourceTypes.h"
#include "MikanTrackingMountTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "MikanVRDeviceTypes.h"
#include "MikanVideoSourceTypes.h"
#include "ReflectionHandles.h"
#include "SerializationProperty.h"
#include "TypeRegistry.h"

#include "unit_test.h"

#include "Refureku/Refureku.h"

#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{
struct ComponentNamesEntry
{
	const char* label;
	rfk::Struct const* archetype;
	const char* componentClassName;
	const char* ownerSystemName;
};

struct SystemNameEntry
{
	const char* label;
	rfk::Struct const* archetype;
	const char* systemName;
};

#define COMPONENT_NAMES(ValuesStruct)                                                                                  \
	{#ValuesStruct, &ValuesStruct::staticGetArchetype(), ValuesStruct::k_componentClassName,                           \
	 ValuesStruct::k_ownerSystemName}
#define SYSTEM_NAME(ValuesStruct) {#ValuesStruct, &ValuesStruct::staticGetArchetype(), ValuesStruct::k_systemName}

const ComponentNamesEntry k_componentEntries[]= {
	COMPONENT_NAMES(MikanAnchorComponentValues),
	COMPONENT_NAMES(MikanCameraComponentValues),
	COMPONENT_NAMES(MikanCompositorComponentValues),
	COMPONENT_NAMES(MikanMarkerComponentValues),
	COMPONENT_NAMES(MikanSceneComponentValues),
	COMPONENT_NAMES(MikanStageComponentValues),
	COMPONENT_NAMES(MikanQuadStencilComponentValues),
	COMPONENT_NAMES(MikanBoxStencilComponentValues),
	COMPONENT_NAMES(MikanModelStencilComponentValues),
	COMPONENT_NAMES(MikanRGBSpotLightComponentValues),
	COMPONENT_NAMES(MikanRGBPixelGridComponentValues),
	COMPONENT_NAMES(MikanDMXFixtureGroupComponentValues),
	COMPONENT_NAMES(MikanDMXPresetComponentValues),
	COMPONENT_NAMES(MikanDMXSequenceComponentValues),
	COMPONENT_NAMES(MikanLightEnvironmentComponentValues),
	COMPONENT_NAMES(MikanQuadShapeComponentValues),
	COMPONENT_NAMES(MikanBoxShapeComponentValues),
	COMPONENT_NAMES(MikanModelShapeComponentValues),
	COMPONENT_NAMES(MikanMarkerTrackingVolumeComponentValues),
	COMPONENT_NAMES(MikanVRTrackingVolumeComponentValues),
	COMPONENT_NAMES(MikanTrackingMountComponentValues),
	COMPONENT_NAMES(MikanScriptComponentValues),
	COMPONENT_NAMES(MikanClientTextureSourceValues),
	COMPONENT_NAMES(MikanSpoutTextureSourceValues),
	COMPONENT_NAMES(MikanCEFTextureSourceValues),
	COMPONENT_NAMES(MikanNetworkVideoSourceValues),
	COMPONENT_NAMES(MikanARKitVideoSourceValues),
	COMPONENT_NAMES(MikanFileVideoSourceValues),
	COMPONENT_NAMES(MikanUSBVideoSourceValues),
	COMPONENT_NAMES(MikanVRDeviceComponentValues),
};

const SystemNameEntry k_systemEntries[]= {
	SYSTEM_NAME(MikanAnchorSystemValues),           SYSTEM_NAME(MikanEditorSystemValues),
	SYSTEM_NAME(MikanBoxStencilSystemValues),       SYSTEM_NAME(MikanQuadStencilSystemValues),
	SYSTEM_NAME(MikanModelStencilSystemValues),     SYSTEM_NAME(MikanQuadShapeSystemValues),
	SYSTEM_NAME(MikanBoxShapeSystemValues),         SYSTEM_NAME(MikanModelShapeSystemValues),
	SYSTEM_NAME(MikanMarkerSystemValues),           SYSTEM_NAME(MikanSceneSystemValues),
	SYSTEM_NAME(MikanDMXObjectSystemValues),        SYSTEM_NAME(MikanRGBSpotLightSystemValues),
	SYSTEM_NAME(MikanRGBPixelGridSystemValues),     SYSTEM_NAME(MikanDMXFixtureGroupSystemValues),
	SYSTEM_NAME(MikanDMXPresetSystemValues),        SYSTEM_NAME(MikanDMXSequenceSystemValues),
	SYSTEM_NAME(MikanLightEnvironmentSystemValues), SYSTEM_NAME(MikanVRObjectSystemValues),
	SYSTEM_NAME(MikanUSBVideoSourceSystemValues),
};

#undef COMPONENT_NAMES
#undef SYSTEM_NAME

bool sameString(const char* a, const char* b) { return a != nullptr && b != nullptr && std::strcmp(a, b) == 0; }

// How many file level structs in the reflection database carry the property
template <typename TProperty>
size_t countAnnotatedStructs()
{
	size_t count= 0;
	rfk::getDatabase().foreachFileLevelStruct(
		[](rfk::Struct const& entity, void* userData) -> bool
		{
			if (entity.getProperty<TProperty>() != nullptr)
			{
				++(*reinterpret_cast<size_t*>(userData));
			}
			return true;
		},
		&count);

	return count;
}
} // namespace

bool client_names_test_component_statics_match_properties()
{
	UNIT_TEST_BEGIN("component class and owner system statics match the ClientComponentNames property")

	for (const ComponentNamesEntry& entry : k_componentEntries)
	{
		auto const* names= entry.archetype->getProperty<Serialization::ClientComponentNames>();
		if (names == nullptr)
		{
			fprintf(stdout, "      %s carries no ClientComponentNames property\n", entry.label);
			success= false;
			continue;
		}

		if (!sameString(names->getComponentClassName(), entry.componentClassName)
			|| !sameString(names->getOwnerSystemName(), entry.ownerSystemName))
		{
			fprintf(stdout, "      %s: property (%s, %s) differs from statics (%s, %s)\n", entry.label,
					names->getComponentClassName(), names->getOwnerSystemName(), entry.componentClassName,
					entry.ownerSystemName);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool client_names_test_system_statics_match_properties()
{
	UNIT_TEST_BEGIN("system name statics match the ClientSystemName property")

	for (const SystemNameEntry& entry : k_systemEntries)
	{
		auto const* systemName= entry.archetype->getProperty<Serialization::ClientSystemName>();
		if (systemName == nullptr)
		{
			fprintf(stdout, "      %s carries no ClientSystemName property\n", entry.label);
			success= false;
			continue;
		}

		if (!sameString(systemName->getSystemName(), entry.systemName))
		{
			fprintf(stdout, "      %s: property %s differs from static %s\n", entry.label, systemName->getSystemName(),
					entry.systemName);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

// A struct annotated but not listed above would pass the checks above by omission
bool client_names_test_every_annotated_struct_is_listed()
{
	UNIT_TEST_BEGIN("every struct carrying a names property is listed")

	const size_t annotatedComponents= countAnnotatedStructs<Serialization::ClientComponentNames>();
	const size_t annotatedSystems= countAnnotatedStructs<Serialization::ClientSystemName>();
	const size_t listedComponents= sizeof(k_componentEntries) / sizeof(k_componentEntries[0]);
	const size_t listedSystems= sizeof(k_systemEntries) / sizeof(k_systemEntries[0]);

	if (annotatedComponents != listedComponents)
	{
		fprintf(stdout, "      %zu structs carry ClientComponentNames, %zu are listed in client_names_unit_tests.cpp\n",
				annotatedComponents, listedComponents);
		success= false;
	}

	if (annotatedSystems != listedSystems)
	{
		fprintf(stdout, "      %zu structs carry ClientSystemName, %zu are listed in client_names_unit_tests.cpp\n",
				annotatedSystems, listedSystems);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

// A response carries no type name on the wire, so a client reads a result into the struct the
// request's ResponseType property names. Every name must be a response struct, and every
// response struct must be named, or a client could never receive it.
bool client_names_test_response_types_resolve()
{
	UNIT_TEST_BEGIN("every ResponseType names a response struct and every response struct is named")

	Serialization::TypeRegistry::build();

	Serialization::StructTypeHandle baseResponse= &MikanResponse::staticGetArchetype();
	std::set<Serialization::StructTypeHandle> responseStructs;
	Serialization::forEachSubclass(baseResponse, [&responseStructs](Serialization::StructTypeHandle subclass)
								   { responseStructs.insert(subclass); });

	std::vector<Serialization::StructTypeHandle> requestStructs;
	Serialization::forEachSubclass(&MikanRequest::staticGetArchetype(),
								   [&requestStructs](Serialization::StructTypeHandle subclass)
								   { requestStructs.push_back(subclass); });

	if (requestStructs.empty() || responseStructs.empty())
	{
		fprintf(stdout, "      found %zu request and %zu response structs\n", requestStructs.size(),
				responseStructs.size());
		success= false;
	}

	std::set<Serialization::StructTypeHandle> namedResponses;
	for (Serialization::StructTypeHandle requestStruct : requestStructs)
	{
		const char* responseTypeName= Serialization::getResponseTypeName(requestStruct);
		if (responseTypeName == nullptr)
		{
			continue;
		}

		Serialization::StructTypeHandle responseStruct= Serialization::TypeRegistry::getStructByName(responseTypeName);
		if (responseStruct == nullptr || responseStructs.count(responseStruct) == 0)
		{
			fprintf(stdout, "      %s: ResponseType %s is not a reflected MikanResponse subclass\n",
					Serialization::getStructTypeName(requestStruct), responseTypeName);
			success= false;
			continue;
		}

		namedResponses.insert(responseStruct);
	}

	for (Serialization::StructTypeHandle responseStruct : responseStructs)
	{
		if (namedResponses.count(responseStruct) == 0)
		{
			fprintf(stdout, "      %s is named by no request's ResponseType\n",
					Serialization::getStructTypeName(responseStruct));
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool run_client_names_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("client_names")
	UNIT_TEST_MODULE_CALL_TEST(client_names_test_component_statics_match_properties);
	UNIT_TEST_MODULE_CALL_TEST(client_names_test_system_statics_match_properties);
	UNIT_TEST_MODULE_CALL_TEST(client_names_test_every_annotated_struct_is_listed);
	UNIT_TEST_MODULE_CALL_TEST(client_names_test_response_types_resolve);
	UNIT_TEST_MODULE_END()
}
