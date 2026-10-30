#include "OcclusionPromptsTests.h"
#include "OcclusionPrompts.h"
#include "unit_test.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace OcclusionPromptsTestsInternal
{
const int k_width= 64;
const int k_height= 48;
const int k_stepColumn= 32; // the first far column of the step map
const float k_nearDepth= 1.f;
const float k_farDepth= 2.f;
const float k_inf= std::numeric_limits<float>::infinity();

OcclusionPromptSettings makeSettings()
{
	OcclusionPromptSettings settings;
	settings.bandPx= 6;
	settings.runGapPx= 5;
	settings.runMinPoints= 10;
	settings.pointSpacingPx= 12;
	settings.maxPointsPerRun= 6;
	return settings;
}

// Near on the left, far on the right
cv::Mat makeStepMap()
{
	cv::Mat depth(k_height, k_width, CV_32FC1, cv::Scalar(k_farDepth));
	depth(cv::Rect(0, 0, k_stepColumn, k_height)).setTo(k_nearDepth);
	return depth;
}

// A disc at the near depth on an uncovered background
cv::Mat makeDiscMap(const cv::Point& centre, int radius)
{
	cv::Mat depth(k_height, k_width, CV_32FC1, cv::Scalar(k_inf));
	for (int y= 0; y < k_height; ++y)
	{
		for (int x= 0; x < k_width; ++x)
		{
			const float dx= (float)(x - centre.x);
			const float dy= (float)(y - centre.y);
			if (dx * dx + dy * dy <= (float)(radius * radius))
			{
				depth.at<float>(y, x)= k_nearDepth;
			}
		}
	}
	return depth;
}

cv::Mat makeDiscMask(const cv::Point& centre, int radius)
{
	cv::Mat mask= cv::Mat::zeros(k_height, k_width, CV_8UC1);
	for (int y= 0; y < k_height; ++y)
	{
		for (int x= 0; x < k_width; ++x)
		{
			const float dx= (float)(x - centre.x);
			const float dy= (float)(y - centre.y);
			if (dx * dx + dy * dy <= (float)(radius * radius))
			{
				mask.at<uint8_t>(y, x)= 255;
			}
		}
	}
	return mask;
}
} // namespace OcclusionPromptsTestsInternal

bool occlusion_prompts_test_step_contour()
{
	UNIT_TEST_BEGIN("a depth step marks its near-side column as the contour")
	using namespace OcclusionPromptsTestsInternal;

	cv::Mat contour;
	OcclusionPrompts::findContours(makeStepMap(), 0.05f, contour);

	int marked= 0;
	int offColumn= 0;
	for (int y= 0; y < k_height; ++y)
	{
		for (int x= 0; x < k_width; ++x)
		{
			if (contour.at<uint8_t>(y, x))
			{
				marked++;
				if (x != k_stepColumn - 1)
					offColumn++;
			}
		}
	}
	if (marked != k_height || offColumn != 0)
	{
		fprintf(stdout, "    %d contour pixels, %d off the near column\n", marked, offColumn);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_flat_map_has_no_runs()
{
	UNIT_TEST_BEGIN("a flat depth map has no runs")
	using namespace OcclusionPromptsTestsInternal;

	cv::Mat depth(k_height, k_width, CV_32FC1, cv::Scalar(k_nearDepth));
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, makeSettings(), runs);
	if (!runs.empty())
	{
		fprintf(stdout, "    %zu runs on a flat map\n", runs.size());
		success= false;
	}

	cv::Mat uncovered(k_height, k_width, CV_32FC1, cv::Scalar(k_inf));
	OcclusionPrompts::findRuns(uncovered, makeSettings(), runs);
	if (!runs.empty())
	{
		fprintf(stdout, "    %zu runs on an uncovered map\n", runs.size());
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_step_run_points()
{
	UNIT_TEST_BEGIN("a depth step gives one run with its points on the near side")
	using namespace OcclusionPromptsTestsInternal;

	const OcclusionPromptSettings settings= makeSettings();
	const cv::Mat depth= makeStepMap();
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);
	if (runs.size() != 1)
	{
		fprintf(stdout, "    %zu runs, expected 1\n", runs.size());
		success= false;
	}
	else
	{
		const OcclusionPromptRun& run= runs[0];
		if ((int)run.positives.size() < 2 || (int)run.positives.size() > settings.maxPointsPerRun)
		{
			fprintf(stdout, "    %zu positives\n", run.positives.size());
			success= false;
		}
		for (const cv::Point2f& p : run.positives)
		{
			const float d= depth.at<float>((int)p.y, (int)p.x);
			const float reach= (float)(k_stepColumn - 1) - p.x;
			if (d != k_nearDepth || reach < 0.f || reach > 2.f * settings.bandPx)
			{
				fprintf(stdout, "    positive (%.0f, %.0f) is not on the near side within two bands\n", p.x, p.y);
				success= false;
			}
		}
		if (run.splitDepth <= k_nearDepth || run.splitDepth >= k_farDepth)
		{
			fprintf(stdout, "    split depth %.3f is not between the sides\n", run.splitDepth);
			success= false;
		}
		const cv::Rect expectedBox(cv::Rect(k_stepColumn - 1 - settings.bandPx, 0, 2 * settings.bandPx + 1, k_height)
								   & cv::Rect(0, 0, k_width, k_height));
		if (run.box != expectedBox)
		{
			fprintf(stdout, "    box (%d, %d, %d, %d)\n", run.box.x, run.box.y, run.box.width, run.box.height);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_disc_run()
{
	UNIT_TEST_BEGIN("a disc on an uncovered background gives one run with its points inside")
	using namespace OcclusionPromptsTestsInternal;

	const cv::Point centre(32, 24);
	const int radius= 14;
	const cv::Mat depth= makeDiscMap(centre, radius);
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, makeSettings(), runs);
	if (runs.size() != 1)
	{
		fprintf(stdout, "    %zu runs, expected 1\n", runs.size());
		success= false;
	}
	else
	{
		const OcclusionPromptRun& run= runs[0];
		for (const cv::Point2f& p : run.positives)
		{
			const float dx= p.x - centre.x;
			const float dy= p.y - centre.y;
			if (dx * dx + dy * dy > (float)(radius * radius))
			{
				fprintf(stdout, "    positive (%.0f, %.0f) is outside the disc\n", p.x, p.y);
				success= false;
			}
		}
		// The near side is the disc within the band of its edge: the outer ring, not the centre
		const int band= makeSettings().bandPx;
		const cv::Mat disc= makeDiscMask(centre, radius);
		const cv::Mat outerRing= disc & ~makeDiscMask(centre, radius - band + 2);
		const cv::Mat core= makeDiscMask(centre, radius - band - 2);
		const int outside= cv::countNonZero(run.nearMask & ~disc);
		const int ringMissing= cv::countNonZero(outerRing & ~run.nearMask);
		const int coreIncluded= cv::countNonZero(run.nearMask & core);
		if (outside != 0 || ringMissing != 0 || coreIncluded != 0)
		{
			fprintf(stdout,
					"    near mask: %d pixels outside the disc, %d of the edge ring missing, %d of the core included\n",
					outside, ringMissing, coreIncluded);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_labels()
{
	UNIT_TEST_BEGIN("labels mark the band where the mask and the mesh disagree")
	using namespace OcclusionPromptsTestsInternal;

	const OcclusionPromptSettings settings= makeSettings();
	const cv::Point centre(32, 24);
	const int radius= 14;
	const int shift= 4;
	const cv::Mat depth= makeDiscMap(centre, radius);
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);
	const cv::Mat mask= makeDiscMask(centre + cv::Point(shift, 0), radius);
	const std::vector<cv::Mat> runMasks(runs.size(), mask);

	cv::Mat labels;
	OcclusionPrompts::computeLabels(depth, runs, runMasks, settings, labels);
	if (labels.type() != CV_8UC1 || labels.size() != depth.size())
	{
		fprintf(stdout, "    labels are not a CV_8UC1 image of the depth's size\n");
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// Just right of the disc's rightmost pixel: the mask covers it, the mesh does not
	const uint8_t rightOfDisc= labels.at<uint8_t>(centre.y, centre.x + radius + 2);
	// Just inside the disc's leftmost pixel: the mesh covers it, the mask does not
	const uint8_t leftInDisc= labels.at<uint8_t>(centre.y, centre.x - radius + 1);
	const uint8_t discCentre= labels.at<uint8_t>(centre.y, centre.x);
	const uint8_t corner= labels.at<uint8_t>(0, 0);
	if (rightOfDisc != OcclusionPrompts::k_labelAdded || leftInDisc != OcclusionPrompts::k_labelRemoved
		|| discCentre != OcclusionPrompts::k_labelKeep || corner != OcclusionPrompts::k_labelKeep)
	{
		fprintf(stdout, "    labels right %d, left %d, centre %d, corner %d\n", rightOfDisc, leftInDisc, discCentre,
				corner);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_long_run_splits()
{
	UNIT_TEST_BEGIN("a run longer than the span splits into parts that cover it")
	using namespace OcclusionPromptsTestsInternal;

	// The step edge runs the whole map's height; a span of a third of it gives at least three parts
	OcclusionPromptSettings settings= makeSettings();
	settings.runMaxSpanPx= k_height / 3;
	const cv::Mat depth= makeStepMap();
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);

	size_t contourPoints= 0;
	int widestSpan= 0;
	for (const OcclusionPromptRun& run : runs)
	{
		contourPoints+= run.contour.size();
		const cv::Rect box= cv::boundingRect(run.contour);
		widestSpan= std::max(widestSpan, std::max(box.width, box.height));
		if (run.positives.empty())
		{
			fprintf(stdout, "    a part has no prompt point\n");
			success= false;
		}
	}
	if (runs.size() < 3 || widestSpan > settings.runMaxSpanPx || contourPoints != (size_t)k_height)
	{
		fprintf(stdout, "    %zu runs, widest span %d, %zu contour points\n", runs.size(), widestSpan, contourPoints);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_labels_only_move_the_silhouette()
{
	UNIT_TEST_BEGIN("a hole inside the object and a blob beside it stay keep")
	using namespace OcclusionPromptsTestsInternal;

	const OcclusionPromptSettings settings= makeSettings();
	const cv::Point centre(32, 24);
	const int radius= 14;
	const cv::Mat depth= makeDiscMap(centre, radius);
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);

	// The mask is the disc with a hole 5 px inside its left edge, inside the band, and a blob 2 px
	// clear of its right edge, also inside the band. Neither touches an edge of the mesh.
	cv::Mat mask= makeDiscMask(centre, radius);
	const cv::Point hole(centre.x - radius + 5, centre.y);
	const cv::Point blob(centre.x + radius + 4, centre.y);
	mask.setTo(0, makeDiscMask(hole, 2));
	mask|= makeDiscMask(blob, 2);
	const std::vector<cv::Mat> runMasks(runs.size(), mask);

	cv::Mat labels;
	OcclusionPrompts::computeLabels(depth, runs, runMasks, settings, labels);
	if (labels.type() != CV_8UC1 || labels.size() != depth.size())
	{
		fprintf(stdout, "    labels are not a CV_8UC1 image of the depth's size\n");
		success= false;
		UNIT_TEST_COMPLETE()
	}

	const uint8_t holeLabel= labels.at<uint8_t>(hole.y, hole.x);
	const uint8_t blobLabel= labels.at<uint8_t>(blob.y, blob.x);
	const int changed= cv::countNonZero(labels != OcclusionPrompts::k_labelKeep);
	if (holeLabel != OcclusionPrompts::k_labelKeep || blobLabel != OcclusionPrompts::k_labelKeep || changed != 0)
	{
		fprintf(stdout, "    hole %d, blob %d, %d pixels changed\n", holeLabel, blobLabel, changed);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_band_pixels_belong_to_their_nearest_contour()
{
	UNIT_TEST_BEGIN("an object standing in another run's band is only removed by its own contour")
	using namespace OcclusionPromptsTestsInternal;

	// Two discs 7 px apart on an uncovered background, each nearer than the other's split depth,
	// with a band wide enough to reach across the gap into the other disc
	OcclusionPromptSettings settings= makeSettings();
	settings.bandPx= 10;
	const cv::Point centreA(18, 24);
	const cv::Point centreB(45, 24);
	const int radius= 10;
	cv::Mat depth= makeDiscMap(centreA, radius);
	depth.setTo(1.3f, makeDiscMask(centreB, radius));
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);
	if (runs.size() != 2)
	{
		fprintf(stdout, "    expected 2 runs, got %zu\n", runs.size());
		success= false;
		UNIT_TEST_COMPLETE()
	}

	// Each run's mask is its own disc, which says background over the other disc
	std::vector<cv::Mat> runMasks;
	for (const OcclusionPromptRun& run : runs)
	{
		const bool bRunA= run.contour.front().x < (centreA.x + centreB.x) / 2;
		runMasks.push_back(makeDiscMask(bRunA ? centreA : centreB, radius));
	}

	cv::Mat labels;
	OcclusionPrompts::computeLabels(depth, runs, runMasks, settings, labels);
	const int changed= cv::countNonZero(labels != OcclusionPrompts::k_labelKeep);
	if (changed != 0)
	{
		fprintf(stdout, "    %d pixels changed, expected none\n", changed);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_mask_only_removes_near_prompts_it_holds()
{
	UNIT_TEST_BEGIN("a mask that leaves out the run's prompt removes nothing along its contour")
	using namespace OcclusionPromptsTestsInternal;

	// A mask of the disc's right half says background over the left half, inside the band, but it
	// leaves the prompts walked in from the left edge out, so it is not the object they sit in and
	// the left edge stays with the mesh
	const OcclusionPromptSettings settings= makeSettings();
	const cv::Point centre(32, 24);
	const int radius= 14;
	const cv::Mat depth= makeDiscMap(centre, radius);
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);
	cv::Mat mask= makeDiscMask(centre, radius);
	mask(cv::Rect(0, 0, centre.x + 3, k_height)).setTo(0);
	int promptsLeftOut= 0;
	for (const OcclusionPromptRun& run : runs)
	{
		for (const cv::Point2f& p : run.positives)
		{
			if (mask.at<uint8_t>((int)std::lround(p.y), (int)std::lround(p.x)) == 0)
				++promptsLeftOut;
		}
	}
	const std::vector<cv::Mat> runMasks(runs.size(), mask);

	cv::Mat labels;
	OcclusionPrompts::computeLabels(depth, runs, runMasks, settings, labels);
	const int removed= cv::countNonZero(labels == OcclusionPrompts::k_labelRemoved);
	if (runs.size() != 1 || promptsLeftOut == 0 || removed != 0)
	{
		fprintf(stdout, "    %zu runs, %d prompts left out, %d pixels removed\n", runs.size(), promptsLeftOut, removed);
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_runs_break_where_the_depth_jumps()
{
	UNIT_TEST_BEGIN("a contour that runs from one object onto another behind it breaks where the depth jumps")
	using namespace OcclusionPromptsTestsInternal;

	// Two blocks side by side on an uncovered background, the right one half again as far: their
	// outlines meet at the top and bottom of the shared edge, within the gap of each other
	const float k_behindDepth= 1.5f;
	const cv::Rect nearBlock(8, 8, 24, 32);
	const cv::Rect behindBlock(32, 8, 24, 32);
	cv::Mat depth(k_height, k_width, CV_32FC1, cv::Scalar(k_inf));
	depth(nearBlock).setTo(k_nearDepth);
	depth(behindBlock).setTo(k_behindDepth);

	OcclusionPromptSettings settings= makeSettings();
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);

	// One run per block, each at its block's depth throughout
	size_t runsAtNear= 0;
	size_t runsAtBehind= 0;
	for (const OcclusionPromptRun& run : runs)
	{
		float minDepth= k_inf;
		float maxDepth= 0.f;
		for (const cv::Point& p : run.contour)
		{
			minDepth= std::min(minDepth, depth.at<float>(p.y, p.x));
			maxDepth= std::max(maxDepth, depth.at<float>(p.y, p.x));
		}
		if (minDepth != maxDepth)
		{
			fprintf(stdout, "    a run spans depths %.2f to %.2f\n", minDepth, maxDepth);
			success= false;
		}
		else if (minDepth == k_nearDepth)
			++runsAtNear;
		else if (minDepth == k_behindDepth)
			++runsAtBehind;
	}
	if (runsAtNear != 1 || runsAtBehind != 1 || runs.size() != 2)
	{
		fprintf(stdout, "    %zu runs, %zu at the near depth, %zu at the depth behind\n", runs.size(), runsAtNear,
				runsAtBehind);
		success= false;
	}

	// A ratio that spans both depths joins them into one run, as before
	settings.runDepthRatio= 2.f;
	OcclusionPrompts::findRuns(depth, settings, runs);
	if (runs.size() != 1)
	{
		fprintf(stdout, "    %zu runs at a ratio of 2\n", runs.size());
		success= false;
	}

	UNIT_TEST_COMPLETE()
}

bool occlusion_prompts_test_mask_notch_narrower_than_the_fill_stays_keep()
{
	UNIT_TEST_BEGIN("a notch into the mask narrower than the fill radius does not remove, a wider one does")
	using namespace OcclusionPromptsTestsInternal;

	// A disc the mask covers, less a notch cut in from its right edge, short of the centre where
	// the prompts sit: with a band of 16 the fill radius is 2, which closes a 3 px notch up to its
	// mouth at the rim, where a notch and the rim's own curve look alike, and leaves a 9 px one
	OcclusionPromptSettings settings= makeSettings();
	settings.bandPx= 16;
	const cv::Point centre(32, 24);
	const int radius= 14;
	const cv::Mat depth= makeDiscMap(centre, radius);
	std::vector<OcclusionPromptRun> runs;
	OcclusionPrompts::findRuns(depth, settings, runs);
	if (runs.size() != 1)
	{
		fprintf(stdout, "    %zu runs\n", runs.size());
		success= false;
	}

	for (const int notchWidth : {3, 9})
	{
		cv::Mat mask= makeDiscMask(centre, radius);
		mask(cv::Rect(centre.x + 6, centre.y - notchWidth / 2, k_width - centre.x - 6, notchWidth)).setTo(0);
		cv::Mat labels;
		OcclusionPrompts::computeLabels(depth, runs, {mask}, settings, labels);
		int deepest= k_width;
		for (int y= 0; y < k_height; ++y)
		{
			for (int x= 0; x < k_width; ++x)
			{
				if (labels.at<uint8_t>(y, x) == OcclusionPrompts::k_labelRemoved)
					deepest= std::min(deepest, x);
			}
		}
		const int fillRadius= settings.bandPx / settings.maskNotchDivisor;
		const bool bFilled= notchWidth <= 2 * fillRadius;
		const int rim= centre.x + radius;
		const bool bOk= bFilled ? deepest >= rim - fillRadius - 1 : deepest <= centre.x + 7;
		if (!bOk)
		{
			fprintf(stdout, "    a %d px notch removed down to x %d of a rim at %d\n", notchWidth, deepest, rim);
			success= false;
		}
	}

	UNIT_TEST_COMPLETE()
}

bool run_occlusion_prompts_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("occlusion_prompts")
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_step_contour);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_flat_map_has_no_runs);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_step_run_points);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_disc_run);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_labels);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_labels_only_move_the_silhouette);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_long_run_splits);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_band_pixels_belong_to_their_nearest_contour);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_mask_only_removes_near_prompts_it_holds);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_runs_break_where_the_depth_jumps);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_mask_notch_narrower_than_the_fill_stays_keep);
	UNIT_TEST_MODULE_END()
}
