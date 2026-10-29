#pragma once

#include "FrameToModelRegistration.h"
#include "MeshCoverage.h"
#include "MeshTexturing.h"
#include "ReconstructionTypes.h"
#include "TsdfVolume.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_int3.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

class DepthTrackReader;
class MovieDecoder;
class PoseTrack;

enum class eOcclusionMeshBuildPhase : int
{
	idle,
	loadingTake,
	computingBounds,
	integrating,
	extracting,
	cleaning,
	coverage,
	texturing,
	complete,
};

// What the builder reports to while it runs, from the worker thread
class IOcclusionMeshBuildObserver
{
public:
	virtual ~IOcclusionMeshBuildObserver() {}
	virtual bool isCancelRequested() const= 0;
	// done and total are frames during integration and coverage and views
	// during texturing, 0 elsewhere
	virtual void onProgress(eOcclusionMeshBuildPhase phase, int done, int total)= 0;
};

struct OcclusionMeshBuildSettings
{
	TsdfVolumeConfig volume;
	int keyframeStride= 3;    // every nth frame of the take is fused
	float boundsMargin= 0.3f; // metres added around the confident depth cloud
	// A box the operator placed, in the take's ARKit world, that replaces the
	// bounds computed from the depth cloud. A long walk otherwise widens the
	// cloud and grows the voxel to fit maxVoxels.
	bool bBoundsOverride= false;
	glm::vec3 boundsOverrideLo= glm::vec3(0.f);
	glm::vec3 boundsOverrideHi= glm::vec3(0.f);
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
	// Projected texture from the take's video, when build is given a decoder
	bool bTexture= true;
	int textureViews= 16;      // fused keyframes spread over the sweep that become atlas tiles
	int textureTileWidth= 640; // pixels, the tile height follows the frame's aspect
	MeshTexturingSettings texturing;
	MeshCoverageSettings coverage;
};

struct OcclusionMeshBuildStats
{
	float voxelSize= 0.f;
	float configuredVoxelSize= 0.f; // the voxel asked for, before any growth to fit maxVoxels
	glm::ivec3 gridDims= glm::ivec3(0);
	glm::vec3 boundsLo= glm::vec3(0.f);
	glm::vec3 boundsHi= glm::vec3(0.f);
	bool bBoundsOverridden= false; // the bounds are the operator's box
	bool bVoxelGrown= false;       // the voxel grew past the configured size to fit maxVoxels
	int keyframes= 0;
	int framesIntegrated= 0;
	size_t voxelsClipped= 0; // dropped by the camera-path trim, 0 when it is off
	int trianglesRaw= 0;
	int trianglesClean= 0;
	int trianglesRemovedAsIslands= 0;
	// Shares of the clean mesh's surface area in each coverage class
	float coverageGood= 0.f;
	float coverageWeak= 0.f;
	float coverageUnseen= 0.f;
	// Texturing, zero when it did not run
	int textureViews= 0;        // views decoded from the take
	int textureViewsUsed= 0;    // views at least one triangle took
	int trianglesUntextured= 0; // triangles no view saw
	int atlasWidth= 0;
	int atlasHeight= 0;
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
	// In the take's ARKit world space, with texcoords into atlas when it was
	// textured
	TriangleMesh mesh;
	// CV_8UC3 BGR, empty unless the mesh was textured. Written beside the
	// model under the model's own stem, which is where the clients look.
	cv::Mat atlas;
	// Track indices of the keyframes that were fused, in time order
	std::vector<size_t> fusedKeyframes;
	// One refined camera-to-world per frame of the take, indexed by the
	// frame's file index, in the pose track's own camera convention. Empty
	// unless registration ran.
	bool bHasRefinedPoses= false;
	std::vector<glm::mat4> refinedCameraToWorld;
	// One record per vertex of mesh, following the texturing's vertex split
	MeshCoverageResult coverage;
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
	// count. observer may be null. colourDecoder, when given and open on the
	// take's movie with settings.bTexture set, textures the mesh from the
	// take's video after cleanup; without one the mesh comes out untextured.
	static bool build(const PoseTrack& track, DepthTrackReader& reader, const OcclusionMeshBuildSettings& settings,
					  IOcclusionMeshBuildObserver* observer, OcclusionMeshBuildResult& outResult,
					  MovieDecoder* colourDecoder= nullptr);

	// Measure how well the fused keyframes observed each vertex of the
	// result's mesh, each frame at the pose it was fused at and through the
	// fusion's own cull and depth range. Returns false when cancelled.
	static bool measureCoverage(const PoseTrack& track, DepthTrackReader& reader,
								const OcclusionMeshBuildSettings& settings, float voxelSize,
								IOcclusionMeshBuildObserver* observer, OcclusionMeshBuildResult& result);

	// The views the texturing projects from: up to maxViews of the fused
	// keyframes, chosen greedily by how many of the result's triangles each
	// adds to the coverage under the texturing rules, each at its refined
	// pose when registration ran, with its depth record and its colour frame
	// decoded by time and shrunk to tileWidth. A keyframe whose colour frame
	// the decoder cannot land on is skipped. Returns the views collected.
	static int collectTextureViews(const PoseTrack& track, DepthTrackReader& reader, MovieDecoder& decoder,
								   const OcclusionMeshBuildResult& result, const MeshTexturingSettings& texturing,
								   int maxViews, int tileWidth, IOcclusionMeshBuildObserver* observer,
								   std::vector<TextureView>& outViews);
};
