#include "OcclusionPrompts.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace OcclusionPromptsInternal
{
const float k_inf= std::numeric_limits<float>::infinity();
const int k_firstWalkStep= 3;
// Where the far side is uncovered, the near side reaches this far beyond the contour's depth
const float k_uncoveredFarRatio= 1.5f;

inline bool isCovered(float d) { return std::isfinite(d); }

// The nearest far-side depth across the contour at p, +inf when it is uncovered, or 0 when p is
// not on a contour
float farDepthAt(const cv::Mat& depth, int x, int y, float depthJump)
{
	const float d= depth.at<float>(y, x);
	if (!isCovered(d))
		return 0.f;

	const int dx[4]= {1, -1, 0, 0};
	const int dy[4]= {0, 0, 1, -1};
	float nearestFar= 0.f;
	bool bFound= false;
	for (int i= 0; i < 4; ++i)
	{
		const int nx= x + dx[i];
		const int ny= y + dy[i];
		if (nx < 0 || ny < 0 || nx >= depth.cols || ny >= depth.rows)
			continue;

		const float dq= depth.at<float>(ny, nx);
		if (!isCovered(dq))
		{
			if (!bFound || nearestFar < k_inf)
			{
				nearestFar= std::max(nearestFar, k_inf);
			}
			bFound= true;
		}
		else if (dq - d > depthJump * dq)
		{
			nearestFar= bFound ? std::min(nearestFar, dq) : dq;
			bFound= true;
		}
	}
	return bFound ? nearestFar : 0.f;
}

// Clears every 8-connected component of region that has no pixel 4-adjacent to a nonzero anchor
// pixel. Both CV_8UC1 of the same size.
void keepComponentsTouching(cv::Mat& region, const cv::Mat& anchor)
{
	if (cv::countNonZero(region) == 0)
		return;

	cv::Mat anchorGrown;
	cv::dilate(anchor, anchorGrown, cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3)));

	cv::Mat componentIds;
	const int componentCount= cv::connectedComponents(region, componentIds, 8, CV_32S);
	std::vector<uint8_t> touches((size_t)componentCount, 0);
	for (int y= 0; y < region.rows; ++y)
	{
		const uint8_t* regionRow= region.ptr<uint8_t>(y);
		const uint8_t* anchorRow= anchorGrown.ptr<uint8_t>(y);
		const int32_t* idRow= componentIds.ptr<int32_t>(y);
		for (int x= 0; x < region.cols; ++x)
		{
			if (regionRow[x] && anchorRow[x])
				touches[(size_t)idRow[x]]= 1;
		}
	}
	for (int y= 0; y < region.rows; ++y)
	{
		uint8_t* regionRow= region.ptr<uint8_t>(y);
		const int32_t* idRow= componentIds.ptr<int32_t>(y);
		for (int x= 0; x < region.cols; ++x)
		{
			if (regionRow[x] && !touches[(size_t)idRow[x]])
				regionRow[x]= 0;
		}
	}
}

cv::Mat distanceToSet(const cv::Mat& set)
{
	cv::Mat notSet;
	cv::compare(set, 0, notSet, cv::CMP_EQ);
	cv::Mat dist;
	cv::distanceTransform(notSet, dist, cv::DIST_L2, 5);
	return dist;
}

// Splits a run's contour points at the median of the box's longer side until no part spans more
// than maxSpan on either axis, appending the parts
void splitBySpan(std::vector<cv::Point> points, int maxSpan, std::vector<std::vector<cv::Point>>& outParts)
{
	const cv::Rect box= cv::boundingRect(points);
	const bool bSplitX= box.width >= box.height;
	const int span= bSplitX ? box.width : box.height;
	if (span <= maxSpan || points.size() < 2)
	{
		outParts.push_back(std::move(points));
		return;
	}

	const size_t middle= points.size() / 2;
	std::nth_element(points.begin(), points.begin() + middle, points.end(),
					 [bSplitX](const cv::Point& a, const cv::Point& b) { return bSplitX ? a.x < b.x : a.y < b.y; });
	std::vector<cv::Point> upper(points.begin() + middle, points.end());
	points.resize(middle);
	splitBySpan(std::move(points), maxSpan, outParts);
	splitBySpan(std::move(upper), maxSpan, outParts);
}

std::vector<int> farthestPoints(const std::vector<cv::Point>& points, int count)
{
	std::vector<int> chosen;
	if (points.empty())
		return chosen;

	chosen.push_back(0);
	std::vector<float> dist(points.size());
	for (size_t i= 0; i < points.size(); ++i)
	{
		dist[i]= (float)cv::norm(points[i] - points[0]);
	}
	while ((int)chosen.size() < count)
	{
		const int next= (int)(std::max_element(dist.begin(), dist.end()) - dist.begin());
		if (dist[next] <= 0.f)
			break;
		chosen.push_back(next);
		for (size_t i= 0; i < points.size(); ++i)
		{
			dist[i]= std::min(dist[i], (float)cv::norm(points[i] - points[next]));
		}
	}
	return chosen;
}
} // namespace OcclusionPromptsInternal

void OcclusionPrompts::findContours(const cv::Mat& depth, float depthJump, cv::Mat& outContour)
{
	outContour= cv::Mat::zeros(depth.size(), CV_8UC1);
	for (int y= 0; y < depth.rows; ++y)
	{
		for (int x= 0; x < depth.cols; ++x)
		{
			if (OcclusionPromptsInternal::farDepthAt(depth, x, y, depthJump) > 0.f)
			{
				outContour.at<uint8_t>(y, x)= 255;
			}
		}
	}
}

void OcclusionPrompts::findRuns(const cv::Mat& depth, const OcclusionPromptSettings& settings,
								std::vector<OcclusionPromptRun>& outRuns)
{
	outRuns.clear();

	cv::Mat contour;
	findContours(depth, settings.depthJump, contour);
	if (cv::countNonZero(contour) == 0)
		return;

	// Contour pixels within the gap of each other share a run
	const int gap= std::max(settings.runGapPx, 1);
	cv::Mat joined;
	cv::dilate(contour, joined, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(gap, gap)));
	cv::Mat labels;
	const int labelCount= cv::connectedComponents(joined, labels, 8, CV_32S);
	std::vector<std::vector<cv::Point>> runPoints(labelCount);
	for (int y= 0; y < depth.rows; ++y)
	{
		for (int x= 0; x < depth.cols; ++x)
		{
			if (contour.at<uint8_t>(y, x))
			{
				runPoints[labels.at<int32_t>(y, x)].push_back(cv::Point(x, y));
			}
		}
	}

	// A merged contour is split into parts of bounded span, each prompted on its own
	std::vector<std::vector<cv::Point>> parts;
	for (int label= 1; label < labelCount; ++label)
	{
		if ((int)runPoints[label].size() < settings.runMinPoints)
			continue;
		OcclusionPromptsInternal::splitBySpan(std::move(runPoints[label]), std::max(settings.runMaxSpanPx, 1), parts);
	}

	const cv::Rect imageRect(0, 0, depth.cols, depth.rows);
	const int band= std::max(settings.bandPx, 1);
	for (const std::vector<cv::Point>& points : parts)
	{
		OcclusionPromptRun run;
		run.contour= points;

		// The depth that splits the run's near side from its far side: the geometric mean of the
		// two sides, or a fixed reach past the near side where the far side is uncovered
		std::vector<float> splits;
		splits.reserve(points.size());
		for (const cv::Point& p : points)
		{
			const float nearDepth= depth.at<float>(p.y, p.x);
			const float farDepth= OcclusionPromptsInternal::farDepthAt(depth, p.x, p.y, settings.depthJump);
			splits.push_back(std::isfinite(farDepth) ? std::sqrt(nearDepth * farDepth)
													 : nearDepth * OcclusionPromptsInternal::k_uncoveredFarRatio);
		}
		std::nth_element(splits.begin(), splits.begin() + splits.size() / 2, splits.end());
		run.splitDepth= splits[splits.size() / 2];

		run.box= cv::boundingRect(points);
		run.box.x-= band;
		run.box.y-= band;
		run.box.width+= 2 * band;
		run.box.height+= 2 * band;
		run.box&= imageRect;

		cv::Mat runContour= cv::Mat::zeros(depth.size(), CV_8UC1);
		for (const cv::Point& p : points)
		{
			runContour.at<uint8_t>(p.y, p.x)= 255;
		}
		const cv::Mat distToContour= OcclusionPromptsInternal::distanceToSet(runContour);

		// The near side the walk may cross: covered, nearer than the split, within two bands
		run.nearMask= cv::Mat::zeros(depth.size(), CV_8UC1);
		cv::Mat walkRegion= cv::Mat::zeros(depth.size(), CV_8UC1);
		for (int y= 0; y < depth.rows; ++y)
		{
			for (int x= 0; x < depth.cols; ++x)
			{
				const float d= depth.at<float>(y, x);
				const bool bNear= OcclusionPromptsInternal::isCovered(d) && d < run.splitDepth;
				if (bNear && run.box.contains(cv::Point(x, y)))
				{
					run.nearMask.at<uint8_t>(y, x)= 255;
				}
				if (bNear && distToContour.at<float>(y, x) <= 2.f * band)
				{
					walkRegion.at<uint8_t>(y, x)= 255;
				}
			}
		}

		// Distance into the region, and the outward normal from the region's signed distance
		cv::Mat distInside;
		cv::distanceTransform(walkRegion, distInside, cv::DIST_L2, 5);
		cv::Mat signedDist= OcclusionPromptsInternal::distanceToSet(walkRegion) - distInside;
		cv::GaussianBlur(signedDist, signedDist, cv::Size(0, 0), 3.0);
		cv::Mat gradX, gradY;
		cv::Sobel(signedDist, gradX, CV_32F, 1, 0);
		cv::Sobel(signedDist, gradY, CV_32F, 0, 1);

		const int count= std::clamp((int)std::lround((double)points.size() / settings.pointSpacingPx), 2,
									std::max(settings.maxPointsPerRun, 2));
		for (int index : OcclusionPromptsInternal::farthestPoints(points, count))
		{
			const cv::Point& p= points[index];
			const float gx= gradX.at<float>(p.y, p.x);
			const float gy= gradY.at<float>(p.y, p.x);
			const float norm= std::max(std::hypot(gx, gy), 1e-6f);
			const float nx= gx / norm;
			const float ny= gy / norm;

			// Walk inward to the spot farthest from any edge of the region
			cv::Point best(-1, -1);
			float bestDist= 0.f;
			for (int s= OcclusionPromptsInternal::k_firstWalkStep; s <= 2 * band; s+= 2)
			{
				const cv::Point q((int)std::lround(p.x - s * nx), (int)std::lround(p.y - s * ny));
				if (!imageRect.contains(q) || !walkRegion.at<uint8_t>(q.y, q.x))
					break;
				const float dist= distInside.at<float>(q.y, q.x);
				if (dist > bestDist)
				{
					bestDist= dist;
					best= q;
				}
			}
			if (best.x >= 0)
			{
				run.positives.push_back(cv::Point2f((float)best.x, (float)best.y));
			}
		}

		if (!run.positives.empty())
		{
			outRuns.push_back(std::move(run));
		}
	}
}

void OcclusionPrompts::computeLabels(const cv::Mat& depth, const std::vector<OcclusionPromptRun>& runs,
									 const std::vector<cv::Mat>& runMasks, const OcclusionPromptSettings& settings,
									 cv::Mat& outLabels)
{
	outLabels= cv::Mat(depth.size(), CV_8UC1, cv::Scalar(k_labelKeep));
	const int band= std::max(settings.bandPx, 1);
	if (runs.empty())
		return;

	// Only the run of a pixel's nearest contour may remove it. A run's band reaches across another
	// object standing beside it, and that run's mask saying background there would carve the
	// other object: its own contour decides what it loses. Adding stays open to every run whose
	// band covers the pixel, since an object's mask reaching past the mesh is its own edge
	// wherever the mesh's nearest contour happens to belong to a neighbour (the cap the mesh
	// never had, right under the desk's edge).
	cv::Mat notContour(depth.size(), CV_8UC1, cv::Scalar(255));
	for (const OcclusionPromptRun& run : runs)
	{
		for (const cv::Point& p : run.contour)
		{
			notContour.at<uint8_t>(p.y, p.x)= 0;
		}
	}
	cv::Mat distToContour;
	cv::Mat nearestContour;
	cv::distanceTransform(notContour, distToContour, nearestContour, cv::DIST_L2, cv::DIST_MASK_5,
						  cv::DIST_LABEL_PIXEL);
	double maxLabel= 0.0;
	cv::minMaxLoc(nearestContour, nullptr, &maxLabel);
	std::vector<int> labelOwner((size_t)maxLabel + 1, -1);
	for (size_t runIndex= 0; runIndex < runs.size(); ++runIndex)
	{
		for (const cv::Point& p : runs[runIndex].contour)
		{
			labelOwner[(size_t)nearestContour.at<int>(p.y, p.x)]= (int)runIndex;
		}
	}

	for (size_t runIndex= 0; runIndex < runs.size() && runIndex < runMasks.size(); ++runIndex)
	{
		const OcclusionPromptRun& run= runs[runIndex];
		const cv::Mat& objectMask= runMasks[runIndex];
		if (objectMask.empty() || objectMask.size() != depth.size() || objectMask.type() != CV_8UC1)
			continue;

		// The mesh's near side as this run sees it, and the two disagreements inside the band
		cv::Mat meshNear= cv::Mat::zeros(depth.size(), CV_8UC1);
		cv::Mat removed= cv::Mat::zeros(depth.size(), CV_8UC1);
		cv::Mat added= cv::Mat::zeros(depth.size(), CV_8UC1);
		for (int y= 0; y < depth.rows; ++y)
		{
			for (int x= 0; x < depth.cols; ++x)
			{
				const float d= depth.at<float>(y, x);
				const bool bMeshOccluder= OcclusionPromptsInternal::isCovered(d) && d < run.splitDepth;
				if (bMeshOccluder)
					meshNear.at<uint8_t>(y, x)= 255;
				if (distToContour.at<float>(y, x) > (float)band)
					continue;

				const bool bMaskOccluder= objectMask.at<uint8_t>(y, x) != 0;
				const bool bOwner= labelOwner[(size_t)nearestContour.at<int>(y, x)] == (int)runIndex;
				if (bMaskOccluder && !bMeshOccluder)
					added.at<uint8_t>(y, x)= 255;
				else if (!bMaskOccluder && bMeshOccluder && bOwner)
					removed.at<uint8_t>(y, x)= 255;
			}
		}

		// A change only moves the silhouette. A removed region that never reaches the far side
		// is a hole the mask left inside the object, and an added region that never reaches the
		// mesh is a blob beside it: neither is an edge the segmenter found, so both stay keep.
		OcclusionPromptsInternal::keepComponentsTouching(removed, ~meshNear);
		OcclusionPromptsInternal::keepComponentsTouching(added, meshNear);
		outLabels.setTo(k_labelRemoved, removed);
		outLabels.setTo(k_labelAdded, added);
	}
}
