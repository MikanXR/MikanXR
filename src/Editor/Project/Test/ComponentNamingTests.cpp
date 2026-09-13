#include "ComponentNamingTests.h"
#include "AppSettingsConfig.h"
#include "ComponentNaming.h"
#include "unit_test.h"

#include "Camera/CameraComponent.h"
#include "Light/RGBSpotLightComponent.h"
#include "Scene/SceneComponent.h"
#include "TrackingMount/TrackingMountComponent.h"

#include <memory>
#include <set>
#include <stdio.h>
#include <string>

bool run_component_naming_tests()
{
	UNIT_TEST_MODULE_BEGIN("component_naming")
	UNIT_TEST_MODULE_CALL_TEST(component_naming_test_default_name_format);
	UNIT_TEST_MODULE_CALL_TEST(component_naming_test_default_table);
	UNIT_TEST_MODULE_CALL_TEST(component_naming_test_settings_store_only_departures);
	UNIT_TEST_MODULE_CALL_TEST(component_naming_test_settings_round_trip);
	UNIT_TEST_MODULE_END()
}

bool component_naming_test_default_name_format()
{
	UNIT_TEST_BEGIN("prefix_id with a prefix, className_id without")

	success&= makeDefaultComponentName("CAM", CameraComponent::k_componentClassName, 1096) == "CAM_1096";
	success&= makeDefaultComponentName("", CameraComponent::k_componentClassName, 1096) == "CameraComponent_1096";

	// No App runs under MikanCmd, so the resolver falls back to the built-in table
	success&= resolveDefaultComponentName(CameraComponent::k_componentClassName, 7) == "CAM_7";
	success&=
		resolveDefaultComponentName(TrackingMountComponent::k_componentClassName, 7) == "TrackingMountComponent_7";

	UNIT_TEST_COMPLETE()
}

bool component_naming_test_default_table()
{
	UNIT_TEST_BEGIN("default table lists each class once and matches the constants")

	std::set<std::string> seen;
	for (const ComponentNamePrefixEntry& entry : getComponentNamePrefixEntries())
	{
		if (!seen.insert(entry.componentClassName).second)
		{
			fprintf(stdout, "    FAILED: %s listed twice\n", entry.componentClassName.c_str());
			success= false;
		}
		if (getDefaultComponentNamePrefix(entry.componentClassName) != entry.defaultPrefix)
		{
			fprintf(stdout, "    FAILED: lookup disagrees for %s\n", entry.componentClassName.c_str());
			success= false;
		}
	}

	success&= getDefaultComponentNamePrefix(RGBSpotLightComponent::k_componentClassName) == "LIGHT";
	success&= getDefaultComponentNamePrefix(TrackingMountComponent::k_componentClassName).empty();
	success&= getDefaultComponentNamePrefix("NoSuchComponent").empty();

	UNIT_TEST_COMPLETE()
}

bool component_naming_test_settings_store_only_departures()
{
	UNIT_TEST_BEGIN("settings keep custom and empty prefixes, drop a default")

	auto settings= std::make_shared<AppSettingsConfig>("ComponentNamingTest");
	const std::string& cameraClass= CameraComponent::k_componentClassName;
	const std::string& mountClass= TrackingMountComponent::k_componentClassName;

	// Untouched classes read their defaults and write nothing
	success&= settings->getComponentNamePrefix(cameraClass) == "CAM";
	success&= settings->getComponentNamePrefix(mountClass).empty();
	success&= settings->writeToJSON()[AppSettingsConfig::k_componentNamePrefixesPropertyId].object_size() == 0;

	// A custom prefix and an explicit empty one both persist
	settings->setComponentNamePrefix(cameraClass, "CAMERA");
	settings->setComponentNamePrefix(mountClass, "MNT");
	success&= settings->getComponentNamePrefix(cameraClass) == "CAMERA";
	success&= settings->getComponentNamePrefix(mountClass) == "MNT";
	settings->setComponentNamePrefix(cameraClass, "");
	success&= settings->getComponentNamePrefix(cameraClass).empty();
	success&= settings->writeToJSON()[AppSettingsConfig::k_componentNamePrefixesPropertyId].object_size() == 2;

	// Setting a prefix back to its default removes the entry
	settings->setComponentNamePrefix(cameraClass, "CAM");
	settings->setComponentNamePrefix(mountClass, "");
	success&= settings->writeToJSON()[AppSettingsConfig::k_componentNamePrefixesPropertyId].object_size() == 0;

	// Reset clears every departure
	settings->setComponentNamePrefix(cameraClass, "CAMERA");
	settings->resetComponentNamePrefixes();
	success&= settings->getComponentNamePrefix(cameraClass) == "CAM";

	UNIT_TEST_COMPLETE()
}

bool component_naming_test_settings_round_trip()
{
	UNIT_TEST_BEGIN("prefix table survives writeToJSON and readFromJSON")

	const std::string& cameraClass= CameraComponent::k_componentClassName;
	const std::string& lightClass= RGBSpotLightComponent::k_componentClassName;
	const std::string& mountClass= TrackingMountComponent::k_componentClassName;

	auto source= std::make_shared<AppSettingsConfig>("ComponentNamingTest");
	source->setComponentNamePrefix(cameraClass, "CAMERA");
	source->setComponentNamePrefix(lightClass, "");
	const configuru::Config json= source->writeToJSON();

	auto loaded= std::make_shared<AppSettingsConfig>("ComponentNamingTest");
	loaded->readFromJSON(json);
	success&= loaded->getComponentNamePrefix(cameraClass) == "CAMERA";
	success&= loaded->getComponentNamePrefix(lightClass).empty();
	success&= loaded->getComponentNamePrefix(mountClass).empty();
	success&= loaded->getComponentNamePrefix(SceneComponent::k_componentClassName) == "SCN";

	UNIT_TEST_COMPLETE()
}
