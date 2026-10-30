#include "OcclusionPromptsTests.h"
#include "OcclusionPrompts.h"
#include "unit_test.h"

#include <opencv2/core.hpp>

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
		const cv::Mat disc= makeDiscMask(centre, radius);
		cv::Mat diff;
		cv::absdiff(run.nearMask, disc, diff);
		if (cv::countNonZero(diff) != 0)
		{
			fprintf(stdout, "    near mask differs from the disc at %d pixels\n", cv::countNonZero(diff));
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

bool run_occlusion_prompts_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("occlusion_prompts")
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_step_contour);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_flat_map_has_no_runs);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_step_run_points);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_disc_run);
	UNIT_TEST_MODULE_CALL_TEST(occlusion_prompts_test_labels);
	UNIT_TEST_MODULE_END()
}
