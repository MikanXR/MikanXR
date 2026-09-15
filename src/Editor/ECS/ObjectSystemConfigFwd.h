#pragma once

#include <memory>

class AppSettingsConfig;
using AppSettingsConfigPtr= std::shared_ptr<AppSettingsConfig>;
using AppSettingsConfigConstPtr= std::shared_ptr<const AppSettingsConfig>;
using AppSettingsConfigWeakPtr= std::weak_ptr<AppSettingsConfig>;

class ProjectConfig;
using ProjectConfigPtr= std::shared_ptr<ProjectConfig>;
using ProjectConfigConstPtr= std::shared_ptr<const ProjectConfig>;
using ProjectConfigWeakPtr= std::weak_ptr<ProjectConfig>;

class MikanObjectSystemDefinition;
using MikanObjectSystemDefinitionPtr= std::shared_ptr<MikanObjectSystemDefinition>;
using MikanObjectSystemDefinitionConstPtr= std::shared_ptr<const MikanObjectSystemDefinition>;
using MikanObjectSystemDefinitionWeakPtr= std::weak_ptr<MikanObjectSystemDefinition>;

class AnchorObjectSystemDefinition;
using AnchorObjectSystemDefinitionPtr= std::shared_ptr<AnchorObjectSystemDefinition>;
using AnchorObjectSystemDefinitionConstPtr= std::shared_ptr<const AnchorObjectSystemDefinition>;
using AnchorObjectSystemDefinitionWeakPtr= std::weak_ptr<AnchorObjectSystemDefinition>;

class ARKitVideoSourceSystemDefinition;
using ARKitVideoSourceSystemDefinitionPtr= std::shared_ptr<ARKitVideoSourceSystemDefinition>;
using ARKitVideoSourceSystemDefinitionConstPtr= std::shared_ptr<const ARKitVideoSourceSystemDefinition>;
using ARKitVideoSourceSystemDefinitionWeakPtr= std::weak_ptr<ARKitVideoSourceSystemDefinition>;

class FileVideoSourceSystemDefinition;
using FileVideoSourceSystemDefinitionPtr= std::shared_ptr<FileVideoSourceSystemDefinition>;
using FileVideoSourceSystemDefinitionConstPtr= std::shared_ptr<const FileVideoSourceSystemDefinition>;
using FileVideoSourceSystemDefinitionWeakPtr= std::weak_ptr<FileVideoSourceSystemDefinition>;

class CameraObjectSystemDefinition;
using CameraObjectSystemDefinitionPtr= std::shared_ptr<CameraObjectSystemDefinition>;
using CameraObjectSystemDefinitionConstPtr= std::shared_ptr<const CameraObjectSystemDefinition>;
using CameraObjectSystemDefinitionWeakPtr= std::weak_ptr<CameraObjectSystemDefinition>;

class ClientTextureSourceSystemDefinition;
using ClientTextureSourceSystemDefinitionPtr= std::shared_ptr<ClientTextureSourceSystemDefinition>;
using ClientTextureSourceSystemDefinitionConstPtr= std::shared_ptr<const ClientTextureSourceSystemDefinition>;
using ClientTextureSourceSystemDefinitionWeakPtr= std::weak_ptr<ClientTextureSourceSystemDefinition>;

class CompositorObjectSystemDefinition;
using CompositorObjectSystemDefinitionPtr= std::shared_ptr<CompositorObjectSystemDefinition>;
using CompositorObjectSystemDefinitionConstPtr= std::shared_ptr<const CompositorObjectSystemDefinition>;
using CompositorObjectSystemDefinitionWeakPtr= std::weak_ptr<CompositorObjectSystemDefinition>;

class EditorObjectSystemDefinition;
using EditorObjectSystemDefinitionPtr= std::shared_ptr<EditorObjectSystemDefinition>;
using EditorObjectSystemDefinitionConstPtr= std::shared_ptr<const EditorObjectSystemDefinition>;
using EditorObjectSystemDefinitionWeakPtr= std::weak_ptr<EditorObjectSystemDefinition>;

class MarkerObjectSystemDefinition;
using MarkerObjectSystemDefinitionPtr= std::shared_ptr<MarkerObjectSystemDefinition>;
using MarkerObjectSystemDefinitionConstPtr= std::shared_ptr<const MarkerObjectSystemDefinition>;
using MarkerObjectSystemDefinitionWeakPtr= std::weak_ptr<MarkerObjectSystemDefinition>;

class MarkerTrackingVolumeSystemDefinition;
using MarkerTrackingVolumeSystemDefinitionPtr= std::shared_ptr<MarkerTrackingVolumeSystemDefinition>;
using MarkerTrackingVolumeSystemDefinitionConstPtr= std::shared_ptr<const MarkerTrackingVolumeSystemDefinition>;
using MarkerTrackingVolumeSystemDefinitionWeakPtr= std::weak_ptr<MarkerTrackingVolumeSystemDefinition>;

class NetworkVideoSourceSystemDefinition;
using NetworkVideoSourceSystemDefinitionPtr= std::shared_ptr<NetworkVideoSourceSystemDefinition>;
using NetworkVideoSourceSystemDefinitionConstPtr= std::shared_ptr<const NetworkVideoSourceSystemDefinition>;
using NetworkVideoSourceSystemDefinitionWeakPtr= std::weak_ptr<NetworkVideoSourceSystemDefinition>;

class SceneObjectSystemDefinition;
using SceneObjectSystemDefinitionPtr= std::shared_ptr<SceneObjectSystemDefinition>;
using SceneObjectSystemDefinitionConstPtr= std::shared_ptr<const SceneObjectSystemDefinition>;
using SceneObjectSystemDefinitionWeakPtr= std::weak_ptr<SceneObjectSystemDefinition>;

class SpoutTextureSourceSystemDefinition;
using SpoutTextureSourceSystemDefinitionPtr= std::shared_ptr<SpoutTextureSourceSystemDefinition>;
using SpoutTextureSourceSystemDefinitionConstPtr= std::shared_ptr<const SpoutTextureSourceSystemDefinition>;
using SpoutTextureSourceSystemDefinitionWeakPtr= std::weak_ptr<SpoutTextureSourceSystemDefinition>;

class CEFTextureSourceSystemDefinition;
using CEFTextureSourceSystemDefinitionPtr= std::shared_ptr<CEFTextureSourceSystemDefinition>;
using CEFTextureSourceSystemDefinitionConstPtr= std::shared_ptr<const CEFTextureSourceSystemDefinition>;
using CEFTextureSourceSystemDefinitionWeakPtr= std::weak_ptr<CEFTextureSourceSystemDefinition>;

class StageObjectSystemDefinition;
using StageObjectSystemDefinitionPtr= std::shared_ptr<StageObjectSystemDefinition>;
using StageObjectSystemDefinitionConstPtr= std::shared_ptr<const StageObjectSystemDefinition>;
using StageObjectSystemDefinitionWeakPtr= std::weak_ptr<StageObjectSystemDefinition>;

class BoxShapeSystemDefinition;
using BoxShapeSystemDefinitionPtr= std::shared_ptr<BoxShapeSystemDefinition>;
using BoxShapeSystemDefinitionConstPtr= std::shared_ptr<const BoxShapeSystemDefinition>;
using BoxShapeSystemDefinitionWeakPtr= std::weak_ptr<BoxShapeSystemDefinition>;

class ModelShapeSystemDefinition;
using ModelShapeSystemDefinitionPtr= std::shared_ptr<ModelShapeSystemDefinition>;
using ModelShapeSystemDefinitionConstPtr= std::shared_ptr<const ModelShapeSystemDefinition>;
using ModelShapeSystemDefinitionWeakPtr= std::weak_ptr<ModelShapeSystemDefinition>;

class QuadShapeSystemDefinition;
using QuadShapeSystemDefinitionPtr= std::shared_ptr<QuadShapeSystemDefinition>;
using QuadShapeSystemDefinitionConstPtr= std::shared_ptr<const QuadShapeSystemDefinition>;
using QuadShapeSystemDefinitionWeakPtr= std::weak_ptr<QuadShapeSystemDefinition>;

class BoxStencilSystemDefinition;
using BoxStencilSystemDefinitionPtr= std::shared_ptr<BoxStencilSystemDefinition>;
using BoxStencilSystemDefinitionConstPtr= std::shared_ptr<const BoxStencilSystemDefinition>;
using BoxStencilSystemDefinitionWeakPtr= std::weak_ptr<BoxStencilSystemDefinition>;

class ModelStencilSystemDefinition;
using ModelStencilSystemDefinitionPtr= std::shared_ptr<ModelStencilSystemDefinition>;
using ModelStencilSystemDefinitionConstPtr= std::shared_ptr<const ModelStencilSystemDefinition>;
using ModelStencilSystemDefinitionWeakPtr= std::weak_ptr<ModelStencilSystemDefinition>;

class QuadStencilSystemDefinition;
using QuadStencilSystemDefinitionPtr= std::shared_ptr<QuadStencilSystemDefinition>;
using QuadStencilSystemDefinitionConstPtr= std::shared_ptr<const QuadStencilSystemDefinition>;
using QuadStencilSystemDefinitionWeakPtr= std::weak_ptr<QuadStencilSystemDefinition>;

class ScriptObjectSystemDefinition;
using ScriptObjectSystemDefinitionPtr= std::shared_ptr<ScriptObjectSystemDefinition>;
using ScriptObjectSystemDefinitionConstPtr= std::shared_ptr<const ScriptObjectSystemDefinition>;
using ScriptObjectSystemDefinitionWeakPtr= std::weak_ptr<ScriptObjectSystemDefinition>;

class TrackingMountObjectSystemDefinition;
using TrackingMountObjectSystemDefinitionPtr= std::shared_ptr<TrackingMountObjectSystemDefinition>;
using TrackingMountObjectSystemDefinitionConstPtr= std::shared_ptr<const TrackingMountObjectSystemDefinition>;
using TrackingMountObjectSystemDefinitionWeakPtr= std::weak_ptr<TrackingMountObjectSystemDefinition>;

class USBVideoSourceSystemDefinition;
using USBVideoSourceSystemDefinitionPtr= std::shared_ptr<USBVideoSourceSystemDefinition>;
using USBVideoSourceSystemDefinitionConstPtr= std::shared_ptr<const USBVideoSourceSystemDefinition>;
using USBVideoSourceSystemDefinitionWeakPtr= std::weak_ptr<USBVideoSourceSystemDefinition>;

class VRObjectSystemDefinition;
using VRObjectSystemDefinitionPtr= std::shared_ptr<VRObjectSystemDefinition>;
using VRObjectSystemDefinitionConstPtr= std::shared_ptr<const VRObjectSystemDefinition>;
using VRObjectSystemDefinitionWeakPtr= std::weak_ptr<VRObjectSystemDefinition>;

class VRTrackingVolumeSystemDefinition;
using VRTrackingVolumeSystemDefinitionPtr= std::shared_ptr<VRTrackingVolumeSystemDefinition>;
using VRTrackingVolumeSystemDefinitionConstPtr= std::shared_ptr<const VRTrackingVolumeSystemDefinition>;
using VRTrackingVolumeSystemDefinitionWeakPtr= std::weak_ptr<VRTrackingVolumeSystemDefinition>;

class RGBSpotLightSystemDefinition;
using RGBSpotLightSystemDefinitionPtr= std::shared_ptr<RGBSpotLightSystemDefinition>;
using RGBSpotLightSystemDefinitionConstPtr= std::shared_ptr<const RGBSpotLightSystemDefinition>;
using RGBSpotLightSystemDefinitionWeakPtr= std::weak_ptr<RGBSpotLightSystemDefinition>;

class RGBPixelGridSystemDefinition;
using RGBPixelGridSystemDefinitionPtr= std::shared_ptr<RGBPixelGridSystemDefinition>;
using RGBPixelGridSystemDefinitionConstPtr= std::shared_ptr<const RGBPixelGridSystemDefinition>;
using RGBPixelGridSystemDefinitionWeakPtr= std::weak_ptr<RGBPixelGridSystemDefinition>;

class DMXFixtureGroupSystemDefinition;
using DMXFixtureGroupSystemDefinitionPtr= std::shared_ptr<DMXFixtureGroupSystemDefinition>;
using DMXFixtureGroupSystemDefinitionConstPtr= std::shared_ptr<const DMXFixtureGroupSystemDefinition>;
using DMXFixtureGroupSystemDefinitionWeakPtr= std::weak_ptr<DMXFixtureGroupSystemDefinition>;

class DMXPresetSystemDefinition;
using DMXPresetSystemDefinitionPtr= std::shared_ptr<DMXPresetSystemDefinition>;
using DMXPresetSystemDefinitionConstPtr= std::shared_ptr<const DMXPresetSystemDefinition>;
using DMXPresetSystemDefinitionWeakPtr= std::weak_ptr<DMXPresetSystemDefinition>;

class DMXSequenceSystemDefinition;
using DMXSequenceSystemDefinitionPtr= std::shared_ptr<DMXSequenceSystemDefinition>;
using DMXSequenceSystemDefinitionConstPtr= std::shared_ptr<const DMXSequenceSystemDefinition>;
using DMXSequenceSystemDefinitionWeakPtr= std::weak_ptr<DMXSequenceSystemDefinition>;
