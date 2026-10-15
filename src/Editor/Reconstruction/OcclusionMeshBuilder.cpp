#include "OcclusionMeshBuilder.h"

#include "CameraPathClip.h"
#include "DepthTrackReader.h"
#include "FrameToModelRegistration.h"
#include "Logger.h"
#include "MeshCleanup.h"
#include "MeshTexturing.h"
#include "MovieDecoder.h"
#include "PoseTrack.h"
#include "SmallMotion.h"
#include "TsdfVolume.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_inverse.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

namespace OcclusionMeshBuilderInternal
{
using Clock= std::chrono::steady_clock;

// A long take is thinned to this many texture view candidates
constexpr size_t k_maxTextureCandidates= 256;
// Frames read past a seek before a keyframe's colour frame is given up on,
// enough for a seek that lands on the keyframe before it at any sane GOP
constexpr int k_maxDecodeReadForward= 300;

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
								 OcclusionMeshBuildResult& outResult, MovieDecoder* colourDecoder)
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
	std::vector<glm::vec3> pathPositions;    // where each fused frame's camera stood
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
		outResult.fusedKeyframes.push_back(keyframes[keyframeIndex]);
		pathPositions.push_back(glm::vec3(cameraToWorld[3]));
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

	// The trim is a mask over the extraction only: the grid keeps everything
	// it fused, so a far wall still anchored the registration above
	std::vector<uint8_t> keepMask;
	if (settings.cameraPathRadius > 0.f)
	{
		stats.voxelsClipped= CameraPathClip::buildKeepMask(volume, pathPositions, settings.cameraPathRadius, keepMask);
	}

	if (!volume.extractMesh(outResult.mesh, keepMask.empty() ? nullptr : &keepMask)
		|| outResult.mesh.getTriangleCount() == 0)
	{
		outResult.failureReason= keepMask.empty() ? "Nothing was fused" : "Nothing was fused within the trim radius";
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

	// Texturing runs on the cleaned mesh, after the island pass has settled
	// which triangles there are to colour. A take whose video cannot be read
	// still yields the mesh.
	if (settings.bTexture && colourDecoder != nullptr)
	{
		if (isCancelled(observer))
		{
			outResult.bCancelled= true;
			return false;
		}

		std::vector<TextureView> views;
		stats.textureViews= collectTextureViews(track, reader, *colourDecoder, outResult, settings.texturing,
												settings.textureViews, settings.textureTileWidth, observer, views);
		if (isCancelled(observer))
		{
			outResult.bCancelled= true;
			return false;
		}

		MeshTexturingStats texturingStats;
		if (stats.textureViews > 0
			&& MeshTexturing::textureMesh(outResult.mesh, views, settings.texturing, outResult.mesh, outResult.atlas,
										  &texturingStats))
		{
			stats.textureViewsUsed= texturingStats.viewsUsed;
			stats.trianglesUntextured= texturingStats.trianglesUntextured;
			stats.atlasWidth= texturingStats.atlasWidth;
			stats.atlasHeight= texturingStats.atlasHeight;
		}
		else
		{
			MIKAN_LOG_WARNING("OcclusionMeshBuilder::build") << "No colour frame of the take could be used, the mesh "
																"is untextured";
			outResult.mesh.texCoords.clear();
			outResult.atlas.release();
			stats.textureViews= 0;
		}
	}

	stats.totalSeconds= secondsSince(startTime);
	reportProgress(observer, eOcclusionMeshBuildPhase::complete, 0, 0);

	MIKAN_LOG_INFO("OcclusionMeshBuilder::build")
		<< "Fused " << stats.framesIntegrated << " frames into " << stats.trianglesClean << " triangles ("
		<< stats.trianglesRemovedAsIslands << " island triangles removed, " << stats.voxelsClipped
		<< " voxels trimmed) in " << stats.totalSeconds << " s"
		<< (settings.bFrameToModel ? ", registration " + std::to_string(stats.registrationsAccepted) + " accepted / "
										 + std::to_string(stats.registrationsRejected) + " rejected"
								   : std::string())
		<< (stats.textureViews > 0 ? ", textured from " + std::to_string(stats.textureViewsUsed) + " of "
										 + std::to_string(stats.textureViews) + " views"
								   : std::string());

	outResult.bSucceeded= true;
	return true;
}

int OcclusionMeshBuilder::collectTextureViews(const PoseTrack& track, DepthTrackReader& reader, MovieDecoder& decoder,
											  const OcclusionMeshBuildResult& result,
											  const MeshTexturingSettings& texturing, int maxViews, int tileWidth,
											  IOcclusionMeshBuildObserver* observer, std::vector<TextureView>& outViews)
{
	using namespace OcclusionMeshBuilderInternal;

	outViews.clear();
	if (!decoder.isOpen() || result.fusedKeyframes.empty() || result.mesh.getTriangleCount() == 0 || maxViews <= 0
		|| tileWidth <= 0)
	{
		return 0;
	}
	if (track.getImageWidth() <= 0 || track.getImageHeight() <= 0)
		return 0;

	const size_t triangleCount= result.mesh.getTriangleCount();

	// Every fused keyframe is a candidate, scored by the triangles it sees
	// from its pose and depth alone, so the costly colour decode is paid
	// only for the views chosen. A long take is thinned to a few hundred
	// candidates.
	const size_t fusedCount= result.fusedKeyframes.size();
	const size_t candidateStride=
		std::max<size_t>(1, (fusedCount + k_maxTextureCandidates - 1) / k_maxTextureCandidates);
	struct Candidate
	{
		size_t trackIndex= 0;
		TextureView view; // pose, depth intrinsics, and depth only
		std::vector<uint8_t> seen;
		size_t seenCount= 0;
	};
	std::vector<Candidate> candidates;
	candidates.reserve(fusedCount / candidateStride + 1);

	cv::Mat depth, confidence;
	for (size_t fusedIndex= 0; fusedIndex < fusedCount; fusedIndex+= candidateStride)
	{
		if (isCancelled(observer))
			return 0;
		reportProgress(observer, eOcclusionMeshBuildPhase::texturing, (int)(fusedIndex / candidateStride),
					   (int)(fusedCount / candidateStride + 1 + (size_t)maxViews));

		const size_t trackIndex= result.fusedKeyframes[fusedIndex];
		const PoseTrackFrame& frame= track.getFrame(trackIndex);
		if (!reader.readRecord(frame.fileIndex, depth, confidence))
			continue;

		Candidate candidate;
		candidate.trackIndex= trackIndex;
		const glm::mat4& recordedCameraToWorld=
			result.bHasRefinedPoses && frame.fileIndex < result.refinedCameraToWorld.size()
				? result.refinedCameraToWorld[frame.fileIndex]
				: frame.cameraToWorld;
		candidate.view.cameraToWorld= toOpenCvCameraToWorld(recordedCameraToWorld);
		candidate.view.depthIntrinsics= frameDepthIntrinsics(track, frame);
		candidate.view.depth= depth.clone();
		MeshTexturing::markSeenTriangles(result.mesh, candidate.view, texturing, candidate.seen);
		candidate.seenCount= (size_t)std::count(candidate.seen.begin(), candidate.seen.end(), (uint8_t)1);
		if (candidate.seenCount > 0)
			candidates.push_back(std::move(candidate));
	}
	if (candidates.empty())
		return 0;

	// Greedy coverage: each pick is the candidate that sees the most
	// triangles nothing picked yet sees, until nothing is gained
	std::vector<uint8_t> covered(triangleCount, 0);
	std::vector<size_t> chosen;
	std::vector<uint8_t> taken(candidates.size(), 0);
	while ((int)chosen.size() < maxViews)
	{
		size_t bestCandidate= candidates.size();
		size_t bestGain= 0;
		for (size_t candidateIndex= 0; candidateIndex < candidates.size(); ++candidateIndex)
		{
			if (taken[candidateIndex] || candidates[candidateIndex].seenCount <= bestGain)
				continue;

			const std::vector<uint8_t>& seen= candidates[candidateIndex].seen;
			size_t gain= 0;
			for (size_t triangle= 0; triangle < triangleCount; ++triangle)
			{
				if (seen[triangle] && !covered[triangle])
					++gain;
			}
			if (gain > bestGain)
			{
				bestGain= gain;
				bestCandidate= candidateIndex;
			}
		}
		if (bestCandidate == candidates.size())
			break;

		taken[bestCandidate]= 1;
		chosen.push_back(bestCandidate);
		const std::vector<uint8_t>& seen= candidates[bestCandidate].seen;
		for (size_t triangle= 0; triangle < triangleCount; ++triangle)
			covered[triangle]|= seen[triangle];
	}

	// Decoded in time order so the seeks only ever move forward. A decoded
	// frame belongs to a keyframe when its presentation time is within half
	// a frame period of the keyframe's; seek lands on the first frame at or
	// after the time it is given, or on the keyframe before it with some
	// backends, so the read runs forward until the keyframe's own arrives.
	std::sort(chosen.begin(), chosen.end(),
			  [&](size_t a, size_t b) { return candidates[a].trackIndex < candidates[b].trackIndex; });
	const int64_t framePeriodUs= track.getMedianFramePeriodUs();
	const int64_t toleranceUs= framePeriodUs > 0 ? framePeriodUs / 2 : 1000;
	const int tileHeight=
		std::max(1, (int)std::lround((double)tileWidth * track.getImageHeight() / track.getImageWidth()));

	cv::Mat frameBgr;
	for (size_t chosenIndex= 0; chosenIndex < chosen.size(); ++chosenIndex)
	{
		if (isCancelled(observer))
			return (int)outViews.size();
		reportProgress(observer, eOcclusionMeshBuildPhase::texturing,
					   (int)(fusedCount / candidateStride + 1 + chosenIndex),
					   (int)(fusedCount / candidateStride + 1 + (size_t)maxViews));

		Candidate& candidate= candidates[chosen[chosenIndex]];
		const PoseTrackFrame& frame= track.getFrame(candidate.trackIndex);
		// A seek that lands past the keyframe (the backend rounds to the
		// frame after, at times) is retried from a second earlier
		int64_t ptsUs= 0;
		bool bLanded= false;
		for (int seekAttempt= 0; seekAttempt < 2 && !bLanded; ++seekAttempt)
		{
			const double seekSeconds= (double)frame.timeUs * 1e-6 - (seekAttempt == 0 ? 0.0 : 1.0);
			if (!decoder.seek(std::max(seekSeconds, 0.0)))
				break;
			for (int readAttempt= 0; readAttempt < k_maxDecodeReadForward; ++readAttempt)
			{
				if (!decoder.readNext(frameBgr, ptsUs))
					break;
				if (std::llabs(ptsUs - frame.timeUs) <= toleranceUs)
				{
					bLanded= true;
					break;
				}
				if (ptsUs > frame.timeUs + toleranceUs)
					break;
			}
		}
		if (!bLanded || frameBgr.empty())
		{
			MIKAN_LOG_WARNING("OcclusionMeshBuilder::collectTextureViews")
				<< "No colour frame at " << frame.timeUs << " us (file index " << frame.fileIndex
				<< "), the read stopped at " << ptsUs << " us";
			continue;
		}

		TextureView view= std::move(candidate.view);
		PinholeIntrinsics colour;
		colour.fx= frame.fx;
		colour.fy= frame.fy;
		colour.cx= frame.cx;
		colour.cy= frame.cy;
		view.tileIntrinsics=
			scaleIntrinsics(colour, track.getImageWidth(), track.getImageHeight(), tileWidth, tileHeight);
		cv::resize(frameBgr, view.tile, cv::Size(tileWidth, tileHeight), 0.0, 0.0, cv::INTER_AREA);
		outViews.push_back(std::move(view));
	}

	return (int)outViews.size();
}
