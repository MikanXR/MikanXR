#pragma once

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/matrix_float4x4.hpp"

#include <vector>

// The marker alignment of a frame-coupled camera, one whose pose arrives with
// every video frame. A static camera's aperture-relative marker poses can be
// averaged directly. A handheld one moves between samples, so each sample is
// first converted into the quantity that is constant across them, the offset
// from the source's own world space to stage space, and those are averaged.
// Shared by the Align Camera stage and the automatic alignment the phone's
// stream prompt relies on.
namespace FrameCoupledAlignment
{
// One sample of the world-to-stage offset. The marker is the stage origin, so
// inverting its aperture-relative pose places the camera in stage space, and
// cameraInStage = worldToStage * cameraInWorld, solved for worldToStage.
// cameraInWorld is the pose in the source's own world space, never the
// offset-applied pose, or a second alignment compounds the first.
glm::dmat4 computeWorldToStageSample(const glm::dmat4& markerInCamera, const glm::mat4& cameraInWorld);

// The average offset: translation componentwise, rotation through a running
// quaternion slerp with each sample first flipped into the running mean's
// hemisphere, since q and -q are one rotation. Identity for no samples.
glm::dmat4 averageWorldToStage(const std::vector<glm::dmat4>& samples);
} // namespace FrameCoupledAlignment
