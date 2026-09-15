#pragma once

// The inventory of component and object system classes that expose descriptors
// to both the client API and the editor panels. Shared by the client API schema
// guard and the localization label coverage guard, so a class added to one is
// checked by both.
//
// Every entry holds plain function pointers to static class functions, so a
// consumer needs no instances, no GL context, and no running editor. The
// Refureku archetype is only needed by the schema guard.

#include "FunctionInterface.h"
#include "PropertyInterface.h"

// -- Client API values struct definitions --
#include "MikanAnchorTypes.h"
#include "MikanCameraTypes.h"
#include "MikanCompositorTypes.h"
#include "MikanEditorTypes.h"
#include "MikanLightTypes.h"
#include "MikanMarkerTypes.h"
#include "MikanSceneTypes.h"
#include "MikanShapeTypes.h"
#include "MikanStageTypes.h"
#include "MikanStencilTypes.h"
#include "MikanTextureSourceTypes.h"
#include "MikanScriptTypes.h"
#include "MikanTrackingMountTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "MikanTransformTypes.h"
#include "MikanVideoSourceTypes.h"
#include "MikanVRDeviceTypes.h"

// -- Component / object-system classes that expose client API values structs --
#include "Anchor/AnchorComponent.h"
#include "Camera/CameraComponent.h"
#include "Compositor/CompositorComponent.h"
#include "Editor/EditorObjectSystem.h"
#include "Light/DMXFixtureComponent.h"
#include "Light/DMXFixtureGroupComponent.h"
#include "Light/DMXFixtureGroupSystem.h"
#include "Light/DMXPresetComponent.h"
#include "Light/DMXPresetSystem.h"
#include "Light/DMXSequenceComponent.h"
#include "Light/DMXSequenceSystem.h"
#include "Light/DMXObjectSystem.h"
#include "Light/RGBPixelGridComponent.h"
#include "Light/LightEnvironmentComponent.h"
#include "Light/RGBSpotLightComponent.h"
#include "Marker/MarkerComponent.h"
#include "Marker/MarkerObjectSystem.h"
#include "Scene/SceneComponent.h"
#include "Scene/TransformComponent.h"
#include "Script/ScriptComponent.h"
#include "Shape/BoxShapeComponent.h"
#include "Shape/BoxShapeSystem.h"
#include "Shape/ModelShapeComponent.h"
#include "Shape/ModelShapeSystem.h"
#include "Shape/QuadShapeComponent.h"
#include "Shape/QuadShapeSystem.h"
#include "Stage/StageComponent.h"
#include "Stencil/BoxStencilComponent.h"
#include "Stencil/ModelStencilComponent.h"
#include "Stencil/QuadStencilComponent.h"
#include "Stencil/StencilComponent.h"
#include "TextureSource/CEFTextureSourceComponent.h"
#include "TextureSource/ClientTextureSourceComponent.h"
#include "TextureSource/SpoutTextureSourceComponent.h"
#include "TextureSource/TextureSourceComponent.h"
#include "TrackingMount/TrackingMountComponent.h"
#include "TrackingVolume/MarkerTrackingVolumeComponent.h"
#include "TrackingVolume/TrackingVolumeComponent.h"
#include "TrackingVolume/VRTrackingVolumeComponent.h"
#include "VideoSource/ARKitVideoSourceComponent.h"
#include "VideoSource/FileVideoSourceComponent.h"
#include "VideoSource/NetworkVideoSourceComponent.h"
#include "VideoSource/USBVideoSourceComponent.h"
#include "VideoSource/USBVideoSourceSystem.h"
#include "VideoSource/VideoSourceComponent.h"
#include "VRObject/VRDeviceComponent.h"
#include "VRObject/VRObjectSystem.h"

#include "Refureku/Refureku.h"

#include <vector>

using GetDescriptorsFn= void (*)(std::vector<PropertyDescriptorConstPtr>&);
using GetFunctionDescriptorsFn= void (*)(std::vector<FunctionDescriptorConstPtr>&);

struct SchemaTestEntry
{
	const char* label;
	rfk::Struct const* valuesStruct;
	GetDescriptorsFn getDescriptors;
	GetFunctionDescriptorsFn getFunctionDescriptors;
};

#define SCHEMA_ENTRY(EditorClass, ValuesStruct)                                                                        \
	{#EditorClass, &ValuesStruct::staticGetArchetype(), &EditorClass::getPropertyDescriptors,                          \
	 &EditorClass::getFunctionDescriptors}

inline const SchemaTestEntry k_schemaTestEntries[]= {
	// -- Components --
	SCHEMA_ENTRY(AnchorComponent, MikanAnchorComponentValues),
	SCHEMA_ENTRY(CameraComponent, MikanCameraComponentValues),
	SCHEMA_ENTRY(CompositorComponent, MikanCompositorComponentValues),
	// DMXFixtureComponent is an abstract base (getPropertyDescriptors is protected and
	// it is never serialized directly); its fields are covered by the concrete
	// RGBSpotLightComponent / RGBPixelGridComponent entries below.
	SCHEMA_ENTRY(RGBPixelGridComponent, MikanRGBPixelGridComponentValues),
	SCHEMA_ENTRY(DMXFixtureGroupComponent, MikanDMXFixtureGroupComponentValues),
	SCHEMA_ENTRY(DMXPresetComponent, MikanDMXPresetComponentValues),
	SCHEMA_ENTRY(DMXSequenceComponent, MikanDMXSequenceComponentValues),
	SCHEMA_ENTRY(RGBSpotLightComponent, MikanRGBSpotLightComponentValues),
	SCHEMA_ENTRY(LightEnvironmentComponent, MikanLightEnvironmentComponentValues),
	SCHEMA_ENTRY(MarkerComponent, MikanMarkerComponentValues),
	SCHEMA_ENTRY(SceneComponent, MikanSceneComponentValues),
	SCHEMA_ENTRY(ScriptComponent, MikanScriptComponentValues),
	SCHEMA_ENTRY(TransformComponent, MikanTransformComponentValues),
	SCHEMA_ENTRY(BoxShapeComponent, MikanBoxShapeComponentValues),
	SCHEMA_ENTRY(ModelShapeComponent, MikanModelShapeComponentValues),
	SCHEMA_ENTRY(QuadShapeComponent, MikanQuadShapeComponentValues),
	SCHEMA_ENTRY(StageComponent, MikanStageComponentValues),
	SCHEMA_ENTRY(BoxStencilComponent, MikanBoxStencilComponentValues),
	SCHEMA_ENTRY(ModelStencilComponent, MikanModelStencilComponentValues),
	SCHEMA_ENTRY(QuadStencilComponent, MikanQuadStencilComponentValues),
	SCHEMA_ENTRY(StencilComponent, MikanStencilComponentValues),
	SCHEMA_ENTRY(CEFTextureSourceComponent, MikanCEFTextureSourceValues),
	SCHEMA_ENTRY(ClientTextureSourceComponent, MikanClientTextureSourceValues),
	SCHEMA_ENTRY(SpoutTextureSourceComponent, MikanSpoutTextureSourceValues),
	SCHEMA_ENTRY(TextureSourceComponent, MikanTextureSourceValues),
	SCHEMA_ENTRY(TrackingMountComponent, MikanTrackingMountComponentValues),
	SCHEMA_ENTRY(MarkerTrackingVolumeComponent, MikanMarkerTrackingVolumeComponentValues),
	SCHEMA_ENTRY(TrackingVolumeComponent, MikanTrackingVolumeComponentValues),
	SCHEMA_ENTRY(VRTrackingVolumeComponent, MikanVRTrackingVolumeComponentValues),
	SCHEMA_ENTRY(ARKitVideoSourceComponent, MikanARKitVideoSourceValues),
	SCHEMA_ENTRY(FileVideoSourceComponent, MikanFileVideoSourceValues),
	SCHEMA_ENTRY(NetworkVideoSourceComponent, MikanNetworkVideoSourceValues),
	SCHEMA_ENTRY(USBVideoSourceComponent, MikanUSBVideoSourceValues),
	SCHEMA_ENTRY(VideoSourceComponent, MikanVideoSourceValues),
	SCHEMA_ENTRY(VRDeviceComponent, MikanVRDeviceComponentValues),

	// -- Object systems --
	SCHEMA_ENTRY(EditorObjectSystem, MikanEditorSystemValues),
	SCHEMA_ENTRY(DMXObjectSystem, MikanDMXObjectSystemValues),
	SCHEMA_ENTRY(DMXFixtureGroupSystem, MikanDMXFixtureGroupSystemValues),
	SCHEMA_ENTRY(DMXPresetSystem, MikanDMXPresetSystemValues),
	SCHEMA_ENTRY(DMXSequenceSystem, MikanDMXSequenceSystemValues),
	SCHEMA_ENTRY(MarkerObjectSystem, MikanMarkerSystemValues),
	SCHEMA_ENTRY(BoxShapeSystem, MikanBoxShapeSystemValues),
	SCHEMA_ENTRY(ModelShapeSystem, MikanModelShapeSystemValues),
	SCHEMA_ENTRY(QuadShapeSystem, MikanQuadShapeSystemValues),
	SCHEMA_ENTRY(USBVideoSourceSystem, MikanUSBVideoSourceSystemValues),
	SCHEMA_ENTRY(VRObjectSystem, MikanVRObjectSystemValues),
};

#undef SCHEMA_ENTRY
