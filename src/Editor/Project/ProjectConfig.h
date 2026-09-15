#pragma once

// -- includes -----
#include "CommonConfig.h"
#include "MikanMathTypes.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "LightSystemFwd.h"
#include "ProjectConfigConstants.h"
#include "MonotonicIDAllocator.h"
#include "PersistentIDAllocator.h"

#include <filesystem>

// -- definitions -----
class ProjectConfig : public CommonConfig
{
public:
	ProjectConfig(const std::string& fnamebase= "ProfileConfig");

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	template <class t_system_config_type, class t_system_type>
	std::shared_ptr<t_system_config_type> addTypedDefinition(IEntityIDAllocatorPtr idAllocator)
	{
		std::shared_ptr<t_system_config_type> newConfig=
			std::make_shared<t_system_config_type>(t_system_type::k_objectSystemClassName, idAllocator);
		addChildConfig(newConfig);
		return newConfig;
	}
	MikanObjectSystemDefinitionPtr getDefinitionForSystem(MikanObjectSystemPtr systemPtr) const;

	// Transient Component ID allocator for all runtime-only components in the project
	// (e.g., components on runtime-only object systems like VRObjectSystem)
	MonotonicIDAllocatorPtr transientIDAllocator;

	// True for ids in the runtime-only range (never serialized, skipped by
	// the transaction recorder)
	static bool isTransientComponentId(int componentId)
	{
		return componentId >= k_transientIdStart && componentId < k_transientIdStart + k_transientIdMaxRange;
	}

	// Persistent Component ID allocator for all persistent components in the project
	static const std::string k_componentIdAllocatorPropertyId;
	PersistentIDAllocatorPtr persistentIDAllocator;

	// Object system definitions
	AnchorObjectSystemDefinitionPtr anchorConfig;
	ARKitVideoSourceSystemDefinitionPtr arkitVideoSourceSystemConfig;
	FileVideoSourceSystemDefinitionPtr fileVideoSourceSystemConfig;
	BoxShapeSystemDefinitionPtr boxShapeSystemDefinition;
	BoxStencilSystemDefinitionPtr boxStencilSystemDefinition;
	CameraObjectSystemDefinitionPtr cameraConfig;
	ClientTextureSourceSystemDefinitionPtr clientConfig;
	CompositorObjectSystemDefinitionPtr compositorConfig;
	EditorObjectSystemDefinitionPtr editorConfig;
	MarkerObjectSystemDefinitionPtr markerSystemDefinition;
	MarkerTrackingVolumeSystemDefinitionPtr markerTrackingVolumeConfig;
	ModelShapeSystemDefinitionPtr modelShapeSystemDefinition;
	ModelStencilSystemDefinitionPtr modelStencilSystemDefinition;
	QuadShapeSystemDefinitionPtr quadShapeSystemDefinition;
	QuadStencilSystemDefinitionPtr quadStencilSystemDefinition;
	SceneObjectSystemDefinitionPtr sceneConfig;
	SpoutTextureSourceSystemDefinitionPtr spoutConfig;
	CEFTextureSourceSystemDefinitionPtr cefConfig;
	StageObjectSystemDefinitionPtr stageConfig;
	TrackingMountObjectSystemDefinitionPtr trackingMountSystemConfig;
	NetworkVideoSourceSystemDefinitionPtr networkVideoSourceSystemConfig;
	USBVideoSourceSystemDefinitionPtr usbVideoSourceSystemConfig;
	VRObjectSystemDefinitionPtr vrObjectConfig;
	VRTrackingVolumeSystemDefinitionPtr vrTrackingVolumeConfig;
	DMXObjectSystemDefinitionPtr dmxObjectSystemDefinition;
	RGBSpotLightSystemDefinitionPtr rgbSpotLightSystemDefinition;
	RGBPixelGridSystemDefinitionPtr rgbPixelGridSystemDefinition;
	DMXFixtureGroupSystemDefinitionPtr dmxFixtureGroupSystemDefinition;
	DMXPresetSystemDefinitionPtr dmxPresetSystemDefinition;
	DMXSequenceSystemDefinitionPtr dmxSequenceSystemDefinition;
	LightEnvironmentSystemDefinitionPtr lightEnvironmentSystemDefinition;
	ScriptObjectSystemDefinitionPtr scriptSystemDefinition;

protected:
	// Max transient component ID (used for runtime-only components that aren't saved to the project file)
	const static int k_transientIdStart;
	const static int k_transientIdMaxRange;
};