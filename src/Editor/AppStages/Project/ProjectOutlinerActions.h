#pragma once

#include "ObjectSystemFwd.h"
#include "ProjectConfigConstants.h"
#include "ProjectOutlinerModel.h"
#include "SceneFwd.h"

// Object creation and deletion entry points for the project outliner. Every
// mutation here expects to run outside the ImGui frame (deferred gui events).
namespace ProjectOutlinerActions
{
// Creation. Each returns the new primary component id, or INVALID_MIKAN_ID.
int addUSBVideoSource(ProjectManagerPtr projectManager);
int addNetworkVideoSource(ProjectManagerPtr projectManager);
int addARKitVideoSource(ProjectManagerPtr projectManager);
int addFileVideoSource(ProjectManagerPtr projectManager);
int addClientTextureSource(ProjectManagerPtr projectManager);
int addSpoutTextureSource(ProjectManagerPtr projectManager);
int addCEFTextureSource(ProjectManagerPtr projectManager);
int addMarker(ProjectManagerPtr projectManager);
int addVRTrackingVolume(ProjectManagerPtr projectManager);
int addMarkerTrackingVolume(ProjectManagerPtr projectManager);
int addTrackingMount(ProjectManagerPtr projectManager, int vrVolumeId);
int addStage(ProjectManagerPtr projectManager, int trackingVolumeId);
int addScene(ProjectManagerPtr projectManager, int stageId);
int addCamera(ProjectManagerPtr projectManager, int stageId);
int addSpotLight(ProjectManagerPtr projectManager, int stageId);
int addPixelGrid(ProjectManagerPtr projectManager, int stageId);
int addLightGroup(ProjectManagerPtr projectManager, int stageId);
int addPreset(ProjectManagerPtr projectManager, int groupId);
int addSequence(ProjectManagerPtr projectManager, int groupId);
int addCompositor(ProjectManagerPtr projectManager, int sceneId);
int addAnchor(ProjectManagerPtr projectManager, int parentTransformId);
int addStencil(ProjectManagerPtr projectManager, eStencilType stencilType, int parentTransformId);
int addShape(ProjectManagerPtr projectManager, eShapeType shapeType, int parentTransformId);
int addScript(ProjectManagerPtr projectManager);

// Number of objects a cascade delete of this node would remove
int countSubtreeObjects(ProjectOutlinerNodeConstPtr node);

// Cascade delete: children before parents, every object through its typed
// system so the transaction recorder sees each destroy
bool deleteSubtree(ProjectManagerPtr projectManager, ProjectOutlinerNodeConstPtr node);

// Reparent a scene actor, preserving its world transform. Fails on a null or
// cyclic target. Callers bracket this in an undo gesture.
bool reparentSceneActor(TransformComponentPtr draggedComponent, TransformComponentPtr newParentComponent);
} // namespace ProjectOutlinerActions
