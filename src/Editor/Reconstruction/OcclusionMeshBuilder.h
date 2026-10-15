#pragma once

#include "FrameToModelRegistration.h"
#include "ReconstructionTypes.h"
#include "TsdfVolume.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_int3.hpp"

#include <string>
#include <vector>

class DepthTrackReader;
class PoseTrack;

enum class eOcclusionMeshBuildPhase : int
{
	idle,
	loadingTake,
	computingBounds,
	integrating,
	extracting,
	cleaning,
	complete,
};

// What the builder reports to while it runs, from the worker thread
class IOcclusionMeshBuildObserver
{
public:
	virtual ~IOcclusionMeshBuildObserver() {}
	virtual bool isCancelRequested() const= 0;
	// done and total are frames during integration, 0 elsewhere
	virtual void onProgress(eOcclusionMeshBuildPhase phase, int done, int total)= 0;
};

struct OcclusionMeshBuildSettings
{
	TsdfVolumeConfig volume;
	int keyframeStride= 3;    // every nth frame of the take is fused
	float boundsMargin= 0.3f; // metres added around the confident depth cloud
	int minComponentTriangles= 500;
	bool bFrameToModel= true; // register each frame against the model before fusing it
	int warmupFrames= 3;      // frames fused at their recorded pose before registration starts
	float normalStencilVoxels= 2.f;
	float raycastStepFraction= 0.5f;
	RegistrationConfig registration;
	// Metres from the nearest fused camera position beyond which the volume is
	// trimmed at extraction, 0 for no trim. The grid itself is never trimmed,
	// so the registration keeps the far walls as constraints.
	float cameraPathRadius= 2.f;
};

struct OcclusionMeshBuildStats
{
	float voxelSize= 0.f;
	glm::ivec3 gridDims= glm::ivec3(0);
	glm::vec3 boundsLo= glm::vec3(0.f);
	glm::vec3 boundsHi= glm::vec3(0.f);
	int keyframes= 0;
	int framesIntegrated= 0;
	size_t voxelsClipped= 0; // dropped by the camera-path trim, 0 when it is off
	int trianglesRaw= 0;
	int trianglesClean= 0;
	int trianglesRemovedAsIslands= 0;
	// Frame-to-model registration, zero when it did not run
	int registrationsAccepted= 0;
	int registrationsRejected= 0;
	float meanStepTranslationMm= 0.f;
	float meanStepRotationDeg= 0.f;
	float meanRmsBeforeMm= 0.f;
	float meanRmsAfterMm= 0.f;
	float finalCorrectionMm= 0.f;
	float finalCorrectionDeg= 0.f;
	float integrateSeconds= 0.f;
	float totalSeconds= 0.f;
};

struct OcclusionMeshBuildResult
{
	bool bSucceeded= false;
	bool bCancelled= false;
	std::string failureReason;
	// In the take's ARKit world space
	TriangleMesh mesh;
	// One refined camera-to-world per frame of the take, indexed by the
	// frame's file index, in the pose track's own camera convention. Empty
	// unless registration ran.
	bool bHasRefinedPoses= false;
	std::vector<glm::mat4> refinedCameraToWorld;
	OcclusionMeshBuildStats stats;
};

// Fuses a take's LiDAR depth into a mesh, refining the recorded poses
// against the model as it goes. Pure data in and out: the stage, the
// headless command, and the tests all call build() the same way.
class OcclusionMeshBuilder
{
public:
	// The one place a recorded pose changes camera convention. A pose track
	// stores camera-to-world with the camera's +Y up and -Z forward; the
	// volume works in OpenCV camera axes (+Y down, +Z forward). The two
	// differ by a half turn about the camera's X axis, applied on the camera
	// side of the transform.
	static glm::mat4 toOpenCvCameraToWorld(const glm::mat4& recordedCameraToWorld);

	// reader must be open with the track's declared depth size and record
	// count. observer may be null.
	static bool build(const PoseTrack& track, DepthTrackReader& reader, const OcclusionMeshBuildSettings& settings,
					  IOcclusionMeshBuildObserver* observer, OcclusionMeshBuildResult& outResult);
};
