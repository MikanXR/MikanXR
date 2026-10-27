#include "FrameCoupledAlignmentTests.h"
#include "unit_test.h"

#include "FrameCoupledAlignment.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include <cmath>
#include <stdio.h>

namespace
{
bool matricesClose(const glm::dmat4& a, const glm::dmat4& b, double tolerance)
{
	for (int column= 0; column < 4; ++column)
	{
		for (int row= 0; row < 4; ++row)
		{
			if (std::abs(a[column][row] - b[column][row]) > tolerance)
				return false;
		}
	}
	return true;
}
} // namespace

bool run_frame_coupled_alignment_tests()
{
	UNIT_TEST_MODULE_BEGIN("frame_coupled_alignment")
	UNIT_TEST_MODULE_CALL_TEST(frame_coupled_alignment_test_averaging);
	UNIT_TEST_MODULE_END()
}

bool frame_coupled_alignment_test_averaging()
{
	UNIT_TEST_BEGIN("the shared frame-coupled solve recovers the offset and averages across the hemisphere")

	// A camera at a known stage pose seeing the marker at the origin: the solved
	// offset maps the camera's world pose onto that stage pose
	const glm::dmat4 cameraInStage= glm::rotate(glm::translate(glm::dmat4(1.0), glm::dvec3(0.5, 1.2, -2.0)),
												glm::radians(30.0), glm::dvec3(0.0, 1.0, 0.0));
	const glm::dmat4 markerInCamera= glm::inverse(cameraInStage);
	const glm::mat4 cameraInWorld= glm::mat4(glm::rotate(glm::translate(glm::dmat4(1.0), glm::dvec3(3.0, 0.0, 1.0)),
														 glm::radians(-80.0), glm::dvec3(0.0, 1.0, 0.0)));
	const glm::dmat4 worldToStage= FrameCoupledAlignment::computeWorldToStageSample(markerInCamera, cameraInWorld);
	const bool bRecovers= matricesClose(worldToStage * glm::dmat4(cameraInWorld), cameraInStage, 1e-6);
	if (!bRecovers)
		printf("      the sample does not map the world pose onto the stage pose\n");
	success&= bRecovers;

	// Identical samples average to themselves, and no samples to identity. The
	// sample's rotation came through a float pose, so it is orthonormal only to
	// float precision and the quaternion round trip in the average cleans it up
	// by about that much.
	const bool bIdentical= matricesClose(
		FrameCoupledAlignment::averageWorldToStage({worldToStage, worldToStage, worldToStage}), worldToStage, 1e-5);
	if (!bIdentical)
		printf("      identical samples did not average to themselves\n");
	success&= bIdentical;
	success&= matricesClose(FrameCoupledAlignment::averageWorldToStage({}), glm::dmat4(1.0), 0.0);

	// Two rotations either side of a mean average onto it, with their translations
	const glm::dquat mean= glm::angleAxis(glm::radians(40.0), glm::normalize(glm::dvec3(0.0, 1.0, 0.0)));
	const glm::dquat left= glm::angleAxis(glm::radians(30.0), glm::dvec3(0.0, 1.0, 0.0));
	const glm::dquat right= glm::angleAxis(glm::radians(50.0), glm::dvec3(0.0, 1.0, 0.0));
	glm::dmat4 leftSample= glm::mat4_cast(left);
	leftSample[3]= glm::dvec4(1.0, 0.0, 0.0, 1.0);
	// The same rotation with its quaternion sign flipped, which a naive average
	// would cancel against the first
	glm::dmat4 rightSample= glm::mat4_cast(-right);
	rightSample[3]= glm::dvec4(3.0, 2.0, 0.0, 1.0);
	glm::dmat4 expected= glm::mat4_cast(mean);
	expected[3]= glm::dvec4(2.0, 1.0, 0.0, 1.0);
	const bool bAveraged=
		matricesClose(FrameCoupledAlignment::averageWorldToStage({leftSample, rightSample}), expected, 1e-6);
	if (!bAveraged)
		printf("      two rotations did not average onto their mean\n");
	success&= bAveraged;

	UNIT_TEST_COMPLETE()
}
