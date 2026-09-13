#include "ComponentNaming.h"
#include "App.h"
#include "AppSettingsConfig.h"

#include "Anchor/AnchorComponent.h"
#include "Camera/CameraComponent.h"
#include "Compositor/CompositorComponent.h"
#include "Light/DMXFixtureComponent.h"
#include "Light/DMXFixtureGroupComponent.h"
#include "Light/DMXPresetComponent.h"
#include "Light/DMXSequenceComponent.h"
#include "Light/LightEnvironmentComponent.h"
#include "Light/RGBPixelGridComponent.h"
#include "Light/RGBSpotLightComponent.h"
#include "Marker/MarkerComponent.h"
#include "Scene/SceneComponent.h"
#include "Script/ScriptComponent.h"
#include "Shape/BoxShapeComponent.h"
#include "Shape/ModelShapeComponent.h"
#include "Shape/QuadShapeComponent.h"
#include "Stage/StageComponent.h"
#include "Stencil/BoxStencilComponent.h"
#include "Stencil/ModelStencilComponent.h"
#include "Stencil/QuadStencilComponent.h"
#include "TextureSource/CEFTextureSourceComponent.h"
#include "TextureSource/ClientTextureSourceComponent.h"
#include "TextureSource/SpoutTextureSourceComponent.h"
#include "TrackingMount/TrackingMountComponent.h"
#include "TrackingVolume/MarkerTrackingVolumeComponent.h"
#include "TrackingVolume/VRTrackingVolumeComponent.h"
#include "VRObject/VRDeviceComponent.h"
#include "VideoSource/ARKitVideoSourceComponent.h"
#include "VideoSource/NetworkVideoSourceComponent.h"
#include "VideoSource/USBVideoSourceComponent.h"

const std::vector<ComponentNamePrefixEntry>& getComponentNamePrefixEntries()
{
	static const std::vector<ComponentNamePrefixEntry> k_entries= {
		{DMXFixtureGroupComponent::k_componentClassName, "GRP"},
		{RGBSpotLightComponent::k_componentClassName, "LIGHT"},
		{RGBPixelGridComponent::k_componentClassName, "LIGHT"},
		{CameraComponent::k_componentClassName, "CAM"},
		{CompositorComponent::k_componentClassName, "COMP"},
		{MarkerComponent::k_componentClassName, "MRK"},
		{USBVideoSourceComponent::k_componentClassName, "SRC"},
		{NetworkVideoSourceComponent::k_componentClassName, "SRC"},
		{ARKitVideoSourceComponent::k_componentClassName, "SRC"},
		{ClientTextureSourceComponent::k_componentClassName, "SRC"},
		{SpoutTextureSourceComponent::k_componentClassName, "SRC"},
		{CEFTextureSourceComponent::k_componentClassName, "SRC"},
		{VRTrackingVolumeComponent::k_componentClassName, "VOL"},
		{MarkerTrackingVolumeComponent::k_componentClassName, "VOL"},
		{DMXPresetComponent::k_componentClassName, "PRE"},
		{DMXSequenceComponent::k_componentClassName, "SEQ"},
		{SceneComponent::k_componentClassName, "SCN"},
		{StageComponent::k_componentClassName, "STG"},
		{ScriptComponent::k_componentClassName, "SCRIPT"},
		{QuadStencilComponent::k_componentClassName, "STENCIL"},
		{BoxStencilComponent::k_componentClassName, "STENCIL"},
		{ModelStencilComponent::k_componentClassName, "STENCIL"},
		{QuadShapeComponent::k_componentClassName, "SHP"},
		{BoxShapeComponent::k_componentClassName, "SHP"},
		{ModelShapeComponent::k_componentClassName, "SHP"},
		{AnchorComponent::k_componentClassName, "ANCHOR"},
		{TrackingMountComponent::k_componentClassName, "MOUNT"},
		{VRDeviceComponent::k_componentClassName, "VRDEVICE"},
		{LightEnvironmentComponent::k_componentClassName, "ENV"},
		{DMXFixtureComponent::k_componentClassName, "DMX"},
	};

	return k_entries;
}

std::string getDefaultComponentNamePrefix(const std::string& componentClassName)
{
	for (const ComponentNamePrefixEntry& entry : getComponentNamePrefixEntries())
	{
		if (entry.componentClassName == componentClassName)
			return entry.defaultPrefix;
	}

	return "";
}

std::string makeDefaultComponentName(const std::string& prefix, const std::string& componentClassName, int componentId)
{
	const std::string& stem= prefix.empty() ? componentClassName : prefix;

	return stem + "_" + std::to_string(componentId);
}

std::string resolveDefaultComponentName(const std::string& componentClassName, int componentId)
{
	App* app= App::getInstance();
	const std::string prefix= app ? app->getAppSettings()->getComponentNamePrefix(componentClassName)
								  : getDefaultComponentNamePrefix(componentClassName);

	return makeDefaultComponentName(prefix, componentClassName, componentId);
}
