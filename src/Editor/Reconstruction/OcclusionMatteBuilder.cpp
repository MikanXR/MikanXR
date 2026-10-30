#include "OcclusionMatteBuilder.h"
#include "DepthRasterizer.h"
#include "Logger.h"
#include "MovieDecoder.h"
#include "OcclusionMeshBuilder.h"
#include "PoseTrack.h"
#include "Sam2Segmenter.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace OcclusionMatteBuilderInternal
{
using Clock= std::chrono::steady_clock;

// The least a small run's crop extends past its box, in matte pixels
const int k_minCropMarginPx= 128;

float secondsSince(const Clock::time_point& start)
{
	return std::chrono::duration<float>(Clock::now() - start).count();
}

bool isCancelled(IOcclusionMatteBuildObserver* observer) { return observer && observer->isCancelRequested(); }

// How many of the points lie inside the mask
size_t countInside(const cv::Mat& mask, const std::vector<cv::Point2f>& points)
{
	size_t count= 0;
	for (const cv::Point2f& p : points)
	{
		const int x= (int)std::lround(p.x);
		const int y= (int)std::lround(p.y);
		if (x >= 0 && y >= 0 && x < mask.cols && y < mask.rows && mask.at<uint8_t>(y, x) != 0)
			++count;
	}
	return count;
}

float maskIoU(const cv::Mat& a, const cv::Mat& b)
{
	cv::Mat both, either;
	cv::bitwise_and(a, b, both);
	cv::bitwise_or(a, b, either);
	const int unionCount= cv::countNonZero(either);
	return unionCount > 0 ? (float)cv::countNonZero(both) / (float)unionCount : 0.f;
}
} // namespace OcclusionMatteBuilderInternal

OcclusionPromptSettings OcclusionMatteBuilder::promptSettingsAtScale(const OcclusionMatteSettings& settings)
{
	const int scale= std::max(settings.matteScale, 1);
	OcclusionPromptSettings prompts= settings.prompts;
	prompts.bandPx= std::max(settings.bandPx / scale, 1);
	prompts.runGapPx= std::max(settings.prompts.runGapPx / scale, 1);
	prompts.runMinPoints= std::max(settings.prompts.runMinPoints / scale, 2);
	prompts.pointSpacingPx= std::max(settings.prompts.pointSpacingPx / scale, 1);
	prompts.runMaxSpanPx= std::max(settings.prompts.runMaxSpanPx / scale, 1);
	return prompts;
}

bool OcclusionMatteBuilder::build(const PoseTrack& track, MovieDecoder& decoder, const std::vector<MatteMesh>& meshes,
								  const OcclusionMatteSettings& settings, Sam2Segmenter& segmenter,
								  const std::filesystem::path& jsonPath, IOcclusionMatteBuildObserver* observer,
								  OcclusionMatteBuildResult& outResult)
{
	outResult= OcclusionMatteBuildResult();
	outResult.jsonPath= jsonPath;
	const OcclusionMatteBuilderInternal::Clock::time_point start= OcclusionMatteBuilderInternal::Clock::now();

	if (!decoder.isOpen() || decoder.isStill())
	{
		outResult.failureReason= "the take's movie is not open";
		return false;
	}
	if (!segmenter.getIsInitialized())
	{
		outResult.failureReason= "the segmenter is not started";
		return false;
	}
	if (track.getFrameCount() == 0 || meshes.empty())
	{
		outResult.failureReason= track.getFrameCount() == 0 ? "the take has no frames" : "no mesh to render";
		return false;
	}

	const int scale= std::max(settings.matteScale, 1);
	const int matteWidth= std::max(track.getImageWidth() / scale, 1);
	const int matteHeight= std::max(track.getImageHeight() / scale, 1);
	const OcclusionPromptSettings prompts= promptSettingsAtScale(settings);

	MatteTrackHeader header;
	header.sessionId= track.getSessionId();
	header.imageWidth= track.getImageWidth();
	header.imageHeight= track.getImageHeight();
	header.matteWidth= matteWidth;
	header.matteHeight= matteHeight;
	header.bandPx= settings.bandPx;
	header.frameStep= std::max(settings.frameStep, 1);
	header.recordCount= track.getFrameCount();
	header.model= settings.modelName;
	for (const MatteMesh& mesh : meshes)
	{
		header.stencils.push_back({mesh.name, mesh.modelPath});
	}

	MatteTrackWriter writer;
	if (!writer.open(jsonPath, header))
	{
		outResult.failureReason= "could not write " + jsonPath.string();
		return false;
	}

	std::vector<DepthRasterizerMesh> rasterMeshes;
	for (const MatteMesh& mesh : meshes)
	{
		DepthRasterizerMesh rasterMesh;
		rasterMesh.mesh= mesh.mesh;
		rasterMesh.meshToWorld= mesh.meshToTakeWorld;
		rasterMeshes.push_back(rasterMesh);
	}

	// Frames decode in order; each is matched to the track by its presentation
	// time, as playback matches them, and written under that frame's file index
	const double fps= decoder.getFps() > 0.0 ? decoder.getFps() : 30.0;
	const int64_t toleranceUs= (int64_t)(0.5e6 / fps);
	const int total= (int)track.getFrameCount();
	OcclusionMatteBuildStats& stats= outResult.stats;
	stats.matteWidth= matteWidth;
	stats.matteHeight= matteHeight;

	cv::Mat frameBgr;
	cv::Mat depth;
	cv::Mat heldLabels;
	std::vector<cv::Mat> runMasks;
	std::vector<cv::Mat> candidates;
	std::vector<float> scores;
	int64_t ptsUs= 0;
	int decodedSinceSegment= header.frameStep;
	if (observer)
		observer->onProgress(eOcclusionMatteBuildPhase::decoding, 0, total);

	while (decoder.readNext(frameBgr, ptsUs))
	{
		if (OcclusionMatteBuilderInternal::isCancelled(observer))
		{
			writer.discard();
			outResult.bCancelled= true;
			outResult.failureReason= "cancelled";
			return false;
		}
		stats.framesDecoded++;

		size_t trackIndex= 0;
		if (!track.findNearestFrame(ptsUs, toleranceUs, trackIndex))
		{
			stats.framesUnmatched++;
			continue;
		}
		const PoseTrackFrame& frame= track.getFrame(trackIndex);

		// The frames outside the requested range keep their initial keep record
		if (settings.lastFrame >= 0 && frame.fileIndex > settings.lastFrame)
			break;
		if (frame.fileIndex < settings.firstFrame)
		{
			if (observer)
				observer->onProgress(eOcclusionMatteBuildPhase::decoding, (int)trackIndex + 1, total);
			continue;
		}

		// The frames between segmented ones hold the last labels
		if (decodedSinceSegment < header.frameStep)
		{
			decodedSinceSegment++;
			if (!heldLabels.empty())
			{
				writer.writeRecord(frame.fileIndex, heldLabels);
				stats.framesHeld++;
			}
			if (observer)
				observer->onProgress(eOcclusionMatteBuildPhase::decoding, (int)trackIndex + 1, total);
			continue;
		}
		decodedSinceSegment= 1;

		// The meshes seen from the recorded pose, at the matte's size
		PinholeIntrinsics intrinsics;
		intrinsics.fx= frame.fx;
		intrinsics.fy= frame.fy;
		intrinsics.cx= frame.cx;
		intrinsics.cy= frame.cy;
		intrinsics= scaleIntrinsics(intrinsics, track.getImageWidth(), track.getImageHeight(), matteWidth, matteHeight);
		const glm::mat4 cameraToWorld= OcclusionMeshBuilder::toOpenCvCameraToWorld(frame.cameraToWorld);
		DepthRasterizer::render(rasterMeshes, cameraToWorld, intrinsics, matteWidth, matteHeight, depth);

		OcclusionMatteFrame processed;
		processed.fileIndex= frame.fileIndex;
		processed.timeUs= frame.timeUs;
		OcclusionPrompts::findRuns(depth, prompts, processed.runs);

		const OcclusionMatteBuilderInternal::Clock::time_point segmentStart=
			OcclusionMatteBuilderInternal::Clock::now();
		runMasks.assign(processed.runs.size(), cv::Mat());
		processed.runIoU.assign(processed.runs.size(), -1.f);
		processed.runCoverage.assign(processed.runs.size(), -1.f);
		processed.runExcess.assign(processed.runs.size(), -1.f);
		if (!processed.runs.empty())
		{
			// A run whose box is small is prompted on a crop around it encoded on its own, since the
			// encoder's 1024 input resolves a thin part of a small object only at that magnification.
			// The runs that span the frame share one encode of the whole frame first.
			const cv::Rect matteRect(0, 0, depth.cols, depth.rows);
			const cv::Rect frameRect(0, 0, frameBgr.cols, frameBgr.rows);
			const double frameScaleX= (double)frameBgr.cols / depth.cols;
			const double frameScaleY= (double)frameBgr.rows / depth.rows;
			auto cropFor= [&](const OcclusionPromptRun& run) -> cv::Rect
			{
				if (!settings.bCropSmallRuns || run.box.width > depth.cols / 2 || run.box.height > depth.rows / 2)
					return matteRect;
				const int marginX= std::max(run.box.width / 2, OcclusionMatteBuilderInternal::k_minCropMarginPx);
				const int marginY= std::max(run.box.height / 2, OcclusionMatteBuilderInternal::k_minCropMarginPx);
				return cv::Rect(run.box.x - marginX, run.box.y - marginY, run.box.width + 2 * marginX,
								run.box.height + 2 * marginY)
					   & matteRect;
			};

			bool bEncodeFailed= false;
			cv::Rect encodedCrop(-1, -1, 0, 0);
			for (int pass= 0; pass < 2 && !bEncodeFailed; ++pass)
			{
				for (size_t runIndex= 0; runIndex < processed.runs.size() && !bEncodeFailed; ++runIndex)
				{
					const OcclusionPromptRun& run= processed.runs[runIndex];
					const cv::Rect crop= cropFor(run);
					const bool bWholeFrame= crop == matteRect;
					if (bWholeFrame != (pass == 0))
						continue;

					if (crop != encodedCrop)
					{
						const cv::Rect frameCrop=
							cv::Rect((int)std::floor(crop.x * frameScaleX), (int)std::floor(crop.y * frameScaleY),
									 (int)std::ceil(crop.width * frameScaleX),
									 (int)std::ceil(crop.height * frameScaleY))
							& frameRect;
						if (!segmenter.setFrame(frameBgr(frameCrop)))
						{
							bEncodeFailed= true;
							break;
						}
						encodedCrop= crop;
					}

					Sam2Segmenter::Prompt prompt;
					prompt.positives.reserve(run.positives.size());
					for (const cv::Point2f& p : run.positives)
					{
						prompt.positives.push_back(cv::Point2f(p.x - crop.x, p.y - crop.y));
					}
					if (!segmenter.predict(prompt, crop.size(), crop.size(), candidates, scores))
						continue;

					// The candidate that agrees most with the mesh's near side around the run, among
					// those holding at least half of the run's own points: a mask that leaves out the
					// points it was asked for is some other object
					int best= -1;
					float bestIoU= -1.f;
					for (size_t c= 0; c < candidates.size(); ++c)
					{
						cv::Mat full= cv::Mat::zeros(depth.size(), CV_8UC1);
						candidates[c].copyTo(full(crop));
						candidates[c]= full;
						if (OcclusionMatteBuilderInternal::countInside(candidates[c], run.positives) * 2
							< run.positives.size())
							continue;
						const float iou= OcclusionMatteBuilderInternal::maskIoU(candidates[c], run.nearMask);
						if (iou > bestIoU)
						{
							bestIoU= iou;
							best= (int)c;
						}
					}
					if (best >= 0)
					{
						processed.runIoU[runIndex]= bestIoU;
						const double nearArea= cv::countNonZero(run.nearMask);
						const double maskArea= cv::countNonZero(candidates[best]);
						const double overlap= cv::countNonZero(candidates[best] & run.nearMask);
						const float coverage= nearArea > 0.0 ? (float)(overlap / nearArea) : 0.f;
						processed.runCoverage[runIndex]= coverage;
						processed.runExcess[runIndex]= maskArea > 0.0 ? (float)((maskArea - overlap) / maskArea) : 0.f;

						// A mask that misses most of the object the contour belongs to is not an edge
						// of it, and the band stays with the mesh rather than carved by the mask
						if (coverage >= settings.minRunCoverage)
						{
							runMasks[runIndex]= candidates[best];
						}
						else
						{
							++stats.runsDropped;
						}
					}
					else if (!candidates.empty())
					{
						++stats.runsDropped;
					}
				}
			}
			if (bEncodeFailed)
			{
				writer.discard();
				if (OcclusionMatteBuilderInternal::isCancelled(observer))
				{
					outResult.bCancelled= true;
					outResult.failureReason= "cancelled";
				}
				else
				{
					outResult.failureReason= "the segmenter failed to encode a frame";
				}
				return false;
			}
		}
		stats.segmentSeconds+= OcclusionMatteBuilderInternal::secondsSince(segmentStart);
		stats.runsTotal+= (int)processed.runs.size();

		OcclusionPrompts::computeLabels(depth, processed.runs, runMasks, prompts, processed.labels);
		if (!writer.writeRecord(frame.fileIndex, processed.labels))
		{
			writer.discard();
			outResult.failureReason= "could not write record " + std::to_string(frame.fileIndex);
			return false;
		}
		heldLabels= processed.labels;
		stats.framesSegmented++;

		if (observer)
		{
			processed.bgr= frameBgr;
			processed.depth= depth;
			processed.runMasks= runMasks;
			observer->onFrameProcessed(processed);
			observer->onProgress(eOcclusionMatteBuildPhase::decoding, (int)trackIndex + 1, total);
		}
	}

	if (stats.framesSegmented == 0)
	{
		writer.discard();
		outResult.failureReason= "no frame of the movie matched the pose track";
		return false;
	}
	if (!writer.finish())
	{
		outResult.failureReason= "could not finish " + jsonPath.string();
		return false;
	}

	stats.totalSeconds= OcclusionMatteBuilderInternal::secondsSince(start);
	if (observer)
		observer->onProgress(eOcclusionMatteBuildPhase::complete, total, total);
	MIKAN_LOG_INFO("OcclusionMatteBuilder::build")
		<< "Wrote " << jsonPath.string() << ": " << stats.framesSegmented << " frames segmented, " << stats.framesHeld
		<< " held, " << stats.runsTotal << " runs, in " << stats.totalSeconds << " s";
	outResult.bSucceeded= true;
	return true;
}
