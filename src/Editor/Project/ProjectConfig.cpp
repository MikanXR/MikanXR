// -- includes -----
#include "AnchorObjectSystem.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceSystem.h"
#include "BoxShapeSystem.h"
#include "BoxStencilSystem.h"
#include "CameraObjectSystem.h"
#include "ClientTextureSourceSystem.h"
#include "CompositorObjectSystem.h"
#include "DMXObjectSystem.h"
#include "EditorObjectSystem.h"
#include "MathUtility.h"
#include "MarkerObjectSystem.h"
#include "MarkerTrackingVolumeSystem.h"
#include "ModelShapeSystem.h"
#include "ModelStencilSystem.h"
#include "NetworkVideoSourceSystem.h"
#include "ProjectConfig.h"
#include "ProjectConfigConstants.h"
#include "PathUtils.h"
#include "QuadShapeSystem.h"
#include "QuadStencilSystem.h"
#include "RGBSpotLightSystem.h"
#include "RGBPixelGridSystem.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetSystem.h"
#include "DMXSequenceSystem.h"
#include "LightEnvironmentSystem.h"
#include "SceneObjectSystem.h"
#include "ScriptObjectSystem.h"
#include "StageObjectSystem.h"
#include "SpoutTextureSourceSystem.h"
#include "CEFTextureSourceSystem.h"
#include "StringUtils.h"
#include "SinglecastDelegate.h"
#include "TrackingMountObjectSystem.h"
#include "USBVideoSourceSystem.h"
#include "VRObjectSystem.h"
#include "VRTrackingVolumeSystem.h"

// -- Profile Config
const int ProjectConfig::k_transientIdStart= 0;
const int ProjectConfig::k_transientIdMaxRange= 1000;
const std::string ProjectConfig::k_componentIdAllocatorPropertyId= "persistentIDAllocator";

ProjectConfig::ProjectConfig(const std::string& fnamebase)
	: CommonConfig(fnamebase)
{
	// Initialize the transient ID allocator for runtime-only components
	transientIDAllocator= std::make_shared<MonotonicIDAllocator>(k_transientIdStart, k_transientIdMaxRange);

	// Create a shared component ID allocator that all object systems will use
	// This ensures unique component IDs across the entire project
	persistentIDAllocator= std::make_shared<PersistentIDAllocator>(k_transientIdStart + k_transientIdMaxRange + 1);

	// Create object system definitions for object systems with persistent components, using the shared persistent ID
	// allocator
	anchorConfig= addTypedDefinition<AnchorObjectSystemDefinition, AnchorObjectSystem>(persistentIDAllocator);
	arkitVideoSourceSystemConfig=
		addTypedDefinition<ARKitVideoSourceSystemDefinition, ARKitVideoSourceSystem>(persistentIDAllocator);
	fileVideoSourceSystemConfig=
		addTypedDefinition<FileVideoSourceSystemDefinition, FileVideoSourceSystem>(persistentIDAllocator);
	boxShapeSystemDefinition= addTypedDefinition<BoxShapeSystemDefinition, BoxShapeSystem>(persistentIDAllocator);
	boxStencilSystemDefinition= addTypedDefinition<BoxStencilSystemDefinition, BoxStencilSystem>(persistentIDAllocator);
	cameraConfig= addTypedDefinition<CameraObjectSystemDefinition, CameraObjectSystem>(persistentIDAllocator);
	clientConfig=
		addTypedDefinition<ClientTextureSourceSystemDefinition, ClientTextureSourceSystem>(persistentIDAllocator);
	compositorConfig=
		addTypedDefinition<CompositorObjectSystemDefinition, CompositorObjectSystem>(persistentIDAllocator);
	editorConfig= addTypedDefinition<EditorObjectSystemDefinition, EditorObjectSystem>(persistentIDAllocator);
	markerSystemDefinition= addTypedDefinition<MarkerObjectSystemDefinition, MarkerObjectSystem>(persistentIDAllocator);
	markerTrackingVolumeConfig=
		addTypedDefinition<MarkerTrackingVolumeSystemDefinition, MarkerTrackingVolumeSystem>(persistentIDAllocator);
	modelShapeSystemDefinition= addTypedDefinition<ModelShapeSystemDefinition, ModelShapeSystem>(persistentIDAllocator);
	modelStencilSystemDefinition=
		addTypedDefinition<ModelStencilSystemDefinition, ModelStencilSystem>(persistentIDAllocator);
	quadShapeSystemDefinition= addTypedDefinition<QuadShapeSystemDefinition, QuadShapeSystem>(persistentIDAllocator);
	quadStencilSystemDefinition=
		addTypedDefinition<QuadStencilSystemDefinition, QuadStencilSystem>(persistentIDAllocator);
	sceneConfig= addTypedDefinition<SceneObjectSystemDefinition, SceneObjectSystem>(persistentIDAllocator);
	stageConfig= addTypedDefinition<StageObjectSystemDefinition, StageObjectSystem>(persistentIDAllocator);
	spoutConfig=
		addTypedDefinition<SpoutTextureSourceSystemDefinition, SpoutTextureSourceSystem>(persistentIDAllocator);
	cefConfig= addTypedDefinition<CEFTextureSourceSystemDefinition, CEFTextureSourceSystem>(persistentIDAllocator);
	trackingMountSystemConfig=
		addTypedDefinition<TrackingMountObjectSystemDefinition, TrackingMountObjectSystem>(persistentIDAllocator);
	networkVideoSourceSystemConfig=
		addTypedDefinition<NetworkVideoSourceSystemDefinition, NetworkVideoSourceSystem>(persistentIDAllocator);
	usbVideoSourceSystemConfig=
		addTypedDefinition<USBVideoSourceSystemDefinition, USBVideoSourceSystem>(persistentIDAllocator);
	vrTrackingVolumeConfig=
		addTypedDefinition<VRTrackingVolumeSystemDefinition, VRTrackingVolumeSystem>(persistentIDAllocator);
	dmxObjectSystemDefinition= addTypedDefinition<DMXObjectSystemDefinition, DMXObjectSystem>(persistentIDAllocator);
	rgbSpotLightSystemDefinition=
		addTypedDefinition<RGBSpotLightSystemDefinition, RGBSpotLightSystem>(persistentIDAllocator);
	rgbPixelGridSystemDefinition=
		addTypedDefinition<RGBPixelGridSystemDefinition, RGBPixelGridSystem>(persistentIDAllocator);
	dmxFixtureGroupSystemDefinition=
		addTypedDefinition<DMXFixtureGroupSystemDefinition, DMXFixtureGroupSystem>(persistentIDAllocator);
	dmxPresetSystemDefinition= addTypedDefinition<DMXPresetSystemDefinition, DMXPresetSystem>(persistentIDAllocator);
	dmxSequenceSystemDefinition=
		addTypedDefinition<DMXSequenceSystemDefinition, DMXSequenceSystem>(persistentIDAllocator);
	lightEnvironmentSystemDefinition=
		addTypedDefinition<LightEnvironmentSystemDefinition, LightEnvironmentSystem>(persistentIDAllocator);
	scriptSystemDefinition= addTypedDefinition<ScriptObjectSystemDefinition, ScriptObjectSystem>(persistentIDAllocator);

	// Create object system definitions for the runtime only components, using the transient ID allocator
	vrObjectConfig= addTypedDefinition<VRObjectSystemDefinition, VRObjectSystem>(transientIDAllocator);
};

configuru::Config ProjectConfig::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	// Write out the shared component ID allocator
	pt[k_componentIdAllocatorPropertyId]= persistentIDAllocator->writeToJSON();

	// Write out all child object system definitions
	for (const auto& childConfigPtr : m_childConfigs)
	{
		if (childConfigPtr->wantsConfigSerialization())
		{
			pt[childConfigPtr->getConfigName()]= childConfigPtr->writeToJSON();
		}
	}

	return pt;
}

void ProjectConfig::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	// Read the shared component ID allocator config
	if (pt.has_key(k_componentIdAllocatorPropertyId))
	{
		persistentIDAllocator->readFromJSON(pt[k_componentIdAllocatorPropertyId]);
	}

	// Read all child object system definitions
	for (const auto& childConfigPtr : m_childConfigs)
	{
		if (pt.has_key(childConfigPtr->getConfigName()))
		{
			const configuru::Config& child_pt= pt[childConfigPtr->getConfigName()];

			if (childConfigPtr->wantsConfigSerialization())
			{
				childConfigPtr->readFromJSON(child_pt);
			}
		}
	}

	// Mark the component ID allocator as safe to allocate
	persistentIDAllocator->markSafeToAllocate();
}

MikanObjectSystemDefinitionPtr ProjectConfig::getDefinitionForSystem(MikanObjectSystemPtr systemPtr) const
{
	const std::string systemClassName= systemPtr->getObjectSystemClassName();
	std::shared_ptr<CommonConfig> systemConfig= getChildConfig(systemClassName);

	if (systemConfig)
	{
		return std::static_pointer_cast<MikanObjectSystemDefinition>(systemConfig);
	}

	return nullptr;
}