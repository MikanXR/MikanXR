#include "OcclusionMeshBuilder.h"

#include "DepthTrackReader.h"
#include "FrameToModelRegistration.h"
#include "Logger.h"
#include "MeshCleanup.h"
#include "PoseTrack.h"
#include "SmallMotion.h"
#include "TsdfVolume.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>

namespace OcclusionMeshBuilderInternal
{
using Clock= std::chrono::steady_clock;

float secondsSince(const Clock::time_point& start)
{
	return std::chrono::duration<float>(Clock::now() - start).count();
}

bool isCancelled(IOcclusionMeshBuildObserver* observer) { return observer != nullptr && observer->isCancelRequested(); }

void reportProgress(IOcclusionMeshBuildObserver* observer, eOcclusionMeshBuildPhase phase, int done, int total)
{
	if (observer != nullptr)
		observer->onProgress(phase, done, total);
}

PinholeIntrinsics frameDepthIntrinsics(const PoseTrack& track, const PoseTrackFrame& frame)
{
	PinholeIntrinsics colour;
	colour.fx= frame.fx;
	colour.fy= frame.fy;
	colour.cx= frame.cx;
	colour.cy= frame.cy;
	return scaleIntrinsics(colour, track.getImageWidth(), track.getImageHeight(), track.getDepthWidth(),
						   track.getDepthHeight());
}

// Percentile of a sample set, destroying its order
float percentile(std::vector<float>& values, float fraction)
{
	if (values.empty())
		return 0.f;

	const size_t index= std::min(values.size() - 1, (size_t)std::floor(fraction * (float)(values.size() - 1)));
	std::nth_element(values.begin(), values.begin() + index, values.end());
	return values[index];
}

// Axis-aligned bounds of the confident depth cloud over a subset of the
// keyframes, trimmed to the 0.5 to 99.5 percentile per axis so a stray return
// does not inflate the grid, then padded by the margin
bool computeBounds(const PoseTrack& track, DepthTrackReader& reader, const std::vector<size_t>& keyframes,
				   const OcclusionMeshBuildSettings& settings, glm::vec3& outLo, glm::vec3& outHi)
{
	const float depthMax= settings.volume.depthMax;
	std::vector<float> axisValues[3];
	glm::vec3 lo(std::numeric_limits<float>::infinity());
	glm::vec3 hi(-std::numeric_limits<float>::infinity());
	bool bAnyPoints= false;

	cv::Mat depth, confidence;
	for (size_t keyframeIndex= 0; keyframeIndex < keyframes.size(); keyframeIndex+= 4)
	{
		const PoseTrackFrame& frame= track.getFrame(keyframes[keyframeIndex]);
		if (!reader.readRecord(frame.fileIndex, depth, confidence))
			return false;

		const PinholeIntrinsics intrinsics= frameDepthIntrinsics(track, frame);
		const glm::mat4 cameraToWorld= OcclusionMeshBuilder::toOpenCvCameraToWorld(frame.cameraToWorld);

		for (int axis= 0; axis < 3; ++axis)
			axisValues[axis].clear();

		for (int v= 0; v < depth.rows; ++v)
		{
			const float* depthRow= depth.ptr<float>(v);
			const uint8_t* confidenceRow= confidence.ptr<uint8_t>(v);
			for (int u= 0; u < depth.cols; ++u)
			{
				const float d= depthRow[u];
				if (confidenceRow[u] < 2 || !(d > 0.f) || d >= depthMax)
					continue;

				const glm::vec4 cameraPoint((float)(((double)u - intrinsics.cx) / intrinsics.fx) * d,
											(float)(((double)v - intrinsics.cy) / intrinsics.fy) * d, d, 1.f);
				const glm::vec3 worldPoint= glm::vec3(cameraToWorld * cameraPoint);
				for (int axis= 0; axis < 3; ++axis)
					axisValues[axis].push_back(worldPoint[axis]);
			}
		}

		if (axisValues[0].empty())
			continue;

		bAnyPoints= true;
		for (int axis= 0; axis < 3; ++axis)
		{
			lo[axis]= std::min(lo[axis], percentile(axisValues[axis], 0.005f));
			hi[axis]= std::max(hi[axis], percentile(axisValues[axis], 0.995f));
		}
	}

	if (!bAnyPoints)
		return false;

	outLo= lo - glm::vec3(settings.boundsMargin);
	outHi= hi + glm::vec3(settings.boundsMargin);
	return true;
}
} // namespace OcclusionMeshBuilderInternal

glm::mat4 OcclusionMeshBuilder::toOpenCvCameraToWorld(const glm::mat4& recordedCameraToWorld)
{
	// Negating the camera's Y and Z axes is the half turn about X; applied
	// on the camera side, so a camera-space point in OpenCV axes lands on
	// the same world point the recorded pose would put its own-axes twin at
	glm::mat4 flipCameraAxes(1.f);
	flipCameraAxes[1][1]= -1.f;
	flipCameraAxes[2][2]= -1.f;
	return recordedCameraToWorld * flipCameraAxes;
}

bool OcclusionMeshBuilder::build(const PoseTrack& track, DepthTrackReader& reader,
								 const OcclusionMeshBuildSettings& settings, IOcclusionMeshBuildObserver* observer,
								 OcclusionMeshBuildResult& outResult)
{
	using namespace OcclusionMeshBuilderInternal;

	outResult= OcclusionMeshBuildResult();
	const Clock::time_point startTime= Clock::now();

	if (!track.isLoaded() || !track.hasDepthTrack())
	{
		outResult.failureReason= "The pose track declares no depth sidecars";
		return false;
	}
	if (!reader.isOpen() || reader.getWidth() != track.getDepthWidth() || reader.getHeight() != track.getDepthHeight()
		|| reader.getRecordCount() < track.getFrameCount())
	{
		outResult.failureReason= "The depth sidecars do not match the pose track";
		return false;
	}
	if (track.getImageWidth() <= 0 || track.getImageHeight() <= 0)
	{
		outResult.failureReason= "The pose track has no image size";
		return false;
	}

	reportProgress(observer, eOcclusionMeshBuildPhase::loadingTake, 0, 0);

	// Keyframes in time order; each carries the file index its record lives at
	const int stride= std::max(settings.keyframeStride, 1);
	std::vector<size_t> keyframes;
	for (size_t trackIndex= 0; trackIndex < track.getFrameCount(); trackIndex+= (size_t)stride)
		keyframes.push_back(trackIndex);
	if (keyframes.empty())
	{
		outResult.failureReason= "The pose track has no frames";
		return false;
	}

	reportProgress(observer, eOcclusionMeshBuildPhase::computingBounds, 0, 0);
	glm::vec3 boundsLo, boundsHi;
	if (!computeBounds(track, reader, keyframes, settings, boundsLo, boundsHi))
	{
		outResult.failureReason= "No confident depth in the take";
		return false;
	}
	if (isCancelled(observer))
	{
		outResult.bCancelled= true;
		return false;
	}

	TsdfVolume volume(boundsLo, boundsHi, settings.volume);
	OcclusionMeshBuildStats& stats= outResult.stats;
	stats.voxelSize= volume.getVoxelSize();
	stats.gridDims= volume.getDims();
	stats.boundsLo= boundsLo;
	stats.boundsHi= boundsHi;
	stats.keyframes= (int)keyframes.size();

	MIKAN_LOG_INFO("OcclusionMeshBuilder::build")
		<< "Fusing " << keyframes.size() << " keyframes into a " << stats.gridDims.x << "x" << stats.gridDims.y << "x"
		<< stats.gridDims.z << " grid at " << stats.voxelSize * 100.f << " cm";

	// Integration, with frame-to-model registration when asked for. The
	// correction carried in delta turns the next frame's recorded pose into
	// its prediction, so the solve only has to find the new drift.
	RegistrationConfig registrationConfig= settings.registration;
	registrationConfig.depthMin= settings.volume.depthMin;
	registrationConfig.depthMax= settings.volume.depthMax;

	std::map<size_t, glm::mat4> corrections; // by file index, integrated frames only
	glm::mat4 delta(1.f);
	double sumStepTranslation= 0.0;
	double sumStepRotation= 0.0;
	double sumRmsBefore= 0.0;
	double sumRmsAfter= 0.0;
	int rmsSamples= 0;

	cv::Mat depth, confidence, weight;
	ModelMap modelMap;
	const Clock::time_point integrateStart= Clock::now();
	for (size_t keyframeIndex= 0; keyframeIndex < keyframes.size(); ++keyframeIndex)
	{
		if (isCancelled(observer))
		{
			outResult.bCancelled= true;
			return false;
		}
		reportProgress(observer, eOcclusionMeshBuildPhase::integrating, (int)keyframeIndex, (int)keyframes.size());

		const PoseTrackFrame& frame= track.getFrame(keyframes[keyframeIndex]);
		if (!reader.readRecord(frame.fileIndex, depth, confidence))
		{
			outResult.failureReason= "Failed to read depth record " + std::to_string(frame.fileIndex);
			return false;
		}
		DepthTrackReader::confidenceToWeight(confidence, weight);

		const PinholeIntrinsics intrinsics= frameDepthIntrinsics(track, frame);
		const glm::mat4 recordedCameraToWorld= toOpenCvCameraToWorld(frame.cameraToWorld);
		const glm::mat4 priorCameraToWorld= delta * recordedCameraToWorld;
		glm::mat4 cameraToWorld= priorCameraToWorld;

		if (settings.bFrameToModel && (int)keyframeIndex >= settings.warmupFrames)
		{
			volume.raycast(intrinsics, priorCameraToWorld, reader.getWidth(), reader.getHeight(),
						   settings.raycastStepFraction, settings.normalStencilVoxels, modelMap);
			const RegistrationResult registration= FrameToModelRegistration::registerFrame(
				depth, weight, intrinsics, priorCameraToWorld, modelMap, priorCameraToWorld, registrationConfig);

			if (!registration.bAccepted)
			{
				// Fusing the frame at a pose the data disagree with would smear
				// the model, so the frame is left out and the carried correction
				// stands for the next one
				++stats.registrationsRejected;
				continue;
			}

			++stats.registrationsAccepted;
			cameraToWorld= registration.cameraToWorld;
			sumStepTranslation+= registration.translationM;
			sumStepRotation+= registration.rotationDeg;
			if (std::isfinite(registration.rmsBeforeM) && std::isfinite(registration.rmsAfterM))
			{
				sumRmsBefore+= registration.rmsBeforeM;
				sumRmsAfter+= registration.rmsAfterM;
				++rmsSamples;
			}
		}

		volume.integrate(depth, weight, intrinsics, cameraToWorld);
		++stats.framesIntegrated;
		delta= cameraToWorld * glm::inverse(recordedCameraToWorld);
		corrections[frame.fileIndex]= delta;
	}
	stats.integrateSeconds= secondsSince(integrateStart);

	if (stats.framesIntegrated == 0)
	{
		outResult.failureReason= "No frame was fused";
		return false;
	}

	// Every frame of the take, fused or not, takes the correction of the
	// nearest fused frame by file index, so a frame is scored and played back
	// where the mesh around it was built. The correction is world-side, so
	// it composes onto the recorded pose without a camera-axis conversion.
	if (settings.bFrameToModel && !corrections.empty())
	{
		std::vector<size_t> anchorIndices;
		anchorIndices.reserve(corrections.size());
		for (const auto& entry : corrections)
			anchorIndices.push_back(entry.first);

		outResult.refinedCameraToWorld.assign(track.getFrameCount(), glm::mat4(1.f));
		for (size_t trackIndex= 0; trackIndex < track.getFrameCount(); ++trackIndex)
		{
			const PoseTrackFrame& frame= track.getFrame(trackIndex);
			if (frame.fileIndex >= outResult.refinedCameraToWorld.size())
				outResult.refinedCameraToWorld.resize(frame.fileIndex + 1, glm::mat4(1.f));

			auto upper= std::lower_bound(anchorIndices.begin(), anchorIndices.end(), frame.fileIndex);
			size_t nearest= (upper != anchorIndices.end()) ? *upper : anchorIndices.back();
			if (upper != anchorIndices.begin())
			{
				const size_t lower= *(upper - 1);
				// The lower anchor wins a tie
				if (upper == anchorIndices.end() || frame.fileIndex - lower <= nearest - frame.fileIndex)
					nearest= lower;
			}

			outResult.refinedCameraToWorld[frame.fileIndex]= corrections[nearest] * frame.cameraToWorld;
		}
		outResult.bHasRefinedPoses= true;

		const glm::mat4& finalCorrection= corrections.rbegin()->second;
		stats.finalCorrectionMm= glm::length(glm::vec3(finalCorrection[3])) * 1000.f;
		stats.finalCorrectionDeg= (float)SmallMotion::rotationAngleDeg(glm::dmat3(glm::mat3(finalCorrection)));
		if (stats.registrationsAccepted > 0)
		{
			stats.meanStepTranslationMm= (float)(sumStepTranslation / stats.registrationsAccepted * 1000.0);
			stats.meanStepRotationDeg= (float)(sumStepRotation / stats.registrationsAccepted);
		}
		if (rmsSamples > 0)
		{
			stats.meanRmsBeforeMm= (float)(sumRmsBefore / rmsSamples * 1000.0);
			stats.meanRmsAfterMm= (float)(sumRmsAfter / rmsSamples * 1000.0);
		}
	}

	if (isCancelled(observer))
	{
		outResult.bCancelled= true;
		return false;
	}

	reportProgress(observer, eOcclusionMeshBuildPhase::extracting, 0, 0);
	if (!volume.extractMesh(outResult.mesh) || outResult.mesh.getTriangleCount() == 0)
	{
		outResult.failureReason= "Nothing was fused";
		return false;
	}
	stats.trianglesRaw= (int)outResult.mesh.getTriangleCount();

	if (isCancelled(observer))
	{
		outResult.bCancelled= true;
		return false;
	}

	reportProgress(observer, eOcclusionMeshBuildPhase::cleaning, 0, 0);
	MeshCleanup::removeDegenerateTriangles(outResult.mesh);
	MeshCleanup::removeDuplicateTriangles(outResult.mesh);
	stats.trianglesRemovedAsIslands= MeshCleanup::removeSmallComponents(outResult.mesh, settings.minComponentTriangles);
	MeshCleanup::removeUnreferencedVertices(outResult.mesh);
	MeshCleanup::computeVertexNormals(outResult.mesh);
	stats.trianglesClean= (int)outResult.mesh.getTriangleCount();
	if (stats.trianglesClean == 0)
	{
		outResult.failureReason= "Nothing was left after cleanup";
		return false;
	}

	stats.totalSeconds= secondsSince(startTime);
	reportProgress(observer, eOcclusionMeshBuildPhase::complete, 0, 0);

	MIKAN_LOG_INFO("OcclusionMeshBuilder::build")
		<< "Fused " << stats.framesIntegrated << " frames into " << stats.trianglesClean << " triangles ("
		<< stats.trianglesRemovedAsIslands << " island triangles removed) in " << stats.totalSeconds << " s"
		<< (settings.bFrameToModel ? ", registration " + std::to_string(stats.registrationsAccepted) + " accepted / "
										 + std::to_string(stats.registrationsRejected) + " rejected"
								   : std::string());

	outResult.bSucceeded= true;
	return true;
}
