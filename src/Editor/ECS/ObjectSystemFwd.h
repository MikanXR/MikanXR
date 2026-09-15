#pragma once

#include <memory>

class AnchorObjectSystem;
using AnchorObjectSystemPtr= std::shared_ptr<AnchorObjectSystem>;
using AnchorObjectSystemWeakPtr= std::weak_ptr<AnchorObjectSystem>;

class ARKitVideoSourceSystem;
using ARKitVideoSourceSystemPtr= std::shared_ptr<ARKitVideoSourceSystem>;
using ARKitVideoSourceSystemWeakPtr= std::weak_ptr<ARKitVideoSourceSystem>;

class FileVideoSourceSystem;
using FileVideoSourceSystemPtr= std::shared_ptr<FileVideoSourceSystem>;
using FileVideoSourceSystemWeakPtr= std::weak_ptr<FileVideoSourceSystem>;

class CameraObjectSystem;
using CameraObjectSystemPtr= std::shared_ptr<CameraObjectSystem>;
using CameraObjectSystemWeakPtr= std::weak_ptr<CameraObjectSystem>;

class ClientTextureSourceSystem;
using ClientTextureSourceSystemPtr= std::shared_ptr<ClientTextureSourceSystem>;
using ClientTextureSourceSystemWeakPtr= std::weak_ptr<ClientTextureSourceSystem>;

class CompositorObjectSystem;
using CompositorObjectSystemPtr= std::shared_ptr<CompositorObjectSystem>;
using CompositorObjectSystemWeakPtr= std::weak_ptr<CompositorObjectSystem>;

class MikanObjectSystem;
using MikanObjectSystemPtr= std::shared_ptr<MikanObjectSystem>;
using MikanObjectSystemWeakPtr= std::weak_ptr<MikanObjectSystem>;

class EditorObjectSystem;
using EditorObjectSystemPtr= std::shared_ptr<EditorObjectSystem>;
using EditorObjectSystemWeakPtr= std::weak_ptr<EditorObjectSystem>;

class MarkerSystem;
using MarkerSystemPtr= std::shared_ptr<MarkerSystem>;
using MarkerSystemWeakPtr= std::weak_ptr<MarkerSystem>;

class MarkerObjectSystem;
using MarkerObjectSystemPtr= std::shared_ptr<MarkerObjectSystem>;
using MarkerObjectSystemWeakPtr= std::weak_ptr<MarkerObjectSystem>;

class MarkerTrackingVolumeSystem;
using MarkerTrackingVolumeSystemPtr= std::shared_ptr<MarkerTrackingVolumeSystem>;
using MarkerTrackingVolumeSystemWeakPtr= std::weak_ptr<MarkerTrackingVolumeSystem>;

class NetworkVideoSourceSystem;
using NetworkVideoSourceSystemPtr= std::shared_ptr<NetworkVideoSourceSystem>;
using NetworkVideoSourceSystemWeakPtr= std::weak_ptr<NetworkVideoSourceSystem>;

class ProjectManager;
using ProjectManagerPtr= std::shared_ptr<ProjectManager>;
using ProjectManagerConstPtr= std::shared_ptr<const ProjectManager>;
using ProjectManagerWeakPtr= std::weak_ptr<ProjectManager>;

class SceneObjectSystem;
using SceneObjectSystemPtr= std::shared_ptr<SceneObjectSystem>;
using SceneObjectSystemWeakPtr= std::weak_ptr<SceneObjectSystem>;

class SpoutTextureSourceSystem;
using SpoutTextureSourceSystemPtr= std::shared_ptr<SpoutTextureSourceSystem>;
using SpoutTextureSourceSystemWeakPtr= std::weak_ptr<SpoutTextureSourceSystem>;

class CEFTextureSourceSystem;
using CEFTextureSourceSystemPtr= std::shared_ptr<CEFTextureSourceSystem>;
using CEFTextureSourceSystemWeakPtr= std::weak_ptr<CEFTextureSourceSystem>;

class StageObjectSystem;
using StageObjectSystemPtr= std::shared_ptr<StageObjectSystem>;
using StageObjectSystemWeakPtr= std::weak_ptr<StageObjectSystem>;

class BoxShapeSystem;
using BoxShapeSystemPtr= std::shared_ptr<BoxShapeSystem>;
using BoxShapeSystemWeakPtr= std::weak_ptr<BoxShapeSystem>;

class ModelShapeSystem;
using ModelShapeSystemPtr= std::shared_ptr<ModelShapeSystem>;
using ModelShapeSystemWeakPtr= std::weak_ptr<ModelShapeSystem>;

class QuadShapeSystem;
using QuadShapeSystemPtr= std::shared_ptr<QuadShapeSystem>;
using QuadShapeSystemWeakPtr= std::weak_ptr<QuadShapeSystem>;

class BoxStencilSystem;
using BoxStencilSystemPtr= std::shared_ptr<BoxStencilSystem>;
using BoxStencilSystemWeakPtr= std::weak_ptr<BoxStencilSystem>;

class ModelStencilSystem;
using ModelStencilSystemPtr= std::shared_ptr<ModelStencilSystem>;
using ModelStencilSystemWeakPtr= std::weak_ptr<ModelStencilSystem>;

class QuadStencilSystem;
using QuadStencilSystemPtr= std::shared_ptr<QuadStencilSystem>;
using QuadStencilSystemWeakPtr= std::weak_ptr<QuadStencilSystem>;

class ScriptObjectSystem;
using ScriptObjectSystemPtr= std::shared_ptr<ScriptObjectSystem>;
using ScriptObjectSystemWeakPtr= std::weak_ptr<ScriptObjectSystem>;

class TrackingMountObjectSystem;
using TrackingMountObjectSystemPtr= std::shared_ptr<TrackingMountObjectSystem>;
using TrackingMountObjectSystemWeakPtr= std::weak_ptr<TrackingMountObjectSystem>;

class USBVideoSourceSystem;
using USBVideoSourceSystemPtr= std::shared_ptr<USBVideoSourceSystem>;
using USBVideoSourceSystemWeakPtr= std::weak_ptr<USBVideoSourceSystem>;

class VRObjectSystem;
using VRObjectSystemPtr= std::shared_ptr<VRObjectSystem>;
using VRObjectSystemWeakPtr= std::weak_ptr<VRObjectSystem>;

class VRTrackingVolumeSystem;
using VRTrackingVolumeSystemPtr= std::shared_ptr<VRTrackingVolumeSystem>;
using VRTrackingVolumeSystemWeakPtr= std::weak_ptr<VRTrackingVolumeSystem>;