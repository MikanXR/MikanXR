#pragma once

#include <memory>
#include <future>
#include <functional>
#include <string>

/// The ID of a any Mikan Component
using MikanComponentID= int32_t;

/// The ID of a any transform
using MikanTransformID= int32_t;

/// The ID of a Compositor
using MikanCompositorID= int32_t;

/// The ID of a camera
using MikanCameraID= int32_t;

/// The ID of a marker
using MikanMarkerID= int32_t;

/// The ID of a scene
using MikanSceneID= int32_t;

/// The ID of a spatial anchor
using MikanSpatialAnchorID= int32_t;

/// The ID of a stage
using MikanStageID= int32_t;

/// The ID of a project script
using MikanScriptID= int32_t;

/// The ID of a shape
using MikanShapeID= int32_t;

/// The ID of a stencil
using MikanStencilID= int32_t;

/// The ID of a project output
using MikanOutputID= int32_t;

/// The ID of a texture source
using MikanTextureSourceID= int32_t;

/// The ID of a tracking mount
using MikanTrackingMountID= int32_t;

/// The ID of a tracking volume
using MikanTrackingVolumeID= int32_t;

/// The ID of a video source
using MikanVideoSourceID= int32_t;

/// The ID of a VR Device
using MikanVRDeviceID= int32_t;

/// The ID of a light fixture (RGB spot light or RGB pixel grid)
using MikanLightID= int32_t;

/// The ID of a DMX fixture group
using MikanDMXFixtureGroupID= int32_t;

/// The ID of a DMX preset
using MikanDMXPresetID= int32_t;

/// The ID of a DMX sequence
using MikanDMXSequenceID= int32_t;

using MikanResponsePtr= std::shared_ptr<struct MikanResponse>;
using MikanResponsePromise= std::promise<MikanResponsePtr>;

using MikanEventPtr= std::shared_ptr<struct MikanEvent>;
