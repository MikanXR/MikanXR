#include "CameraFramePoseTests.h"
#include "unit_test.h"

#include "Camera/CameraComponent.h"

#include "glm/ext/matrix_double4x4.hpp"
#include "glm/ext/matrix_transform.hpp"

#include <cmath>
#include <memory>

namespace CameraFramePoseTestsInternal
{
bool matricesMatch(const glm::mat4& a, const glm::mat4& b)
{
	for (int column= 0; column < 4; ++column)
	{
		for (int row= 0; row < 4; ++row)
		{
			if (std::fabs(a[column][row] - b[column][row]) > 1e-6f)
				return false;
		}
	}
	return true;
}

// A camera with no owner and no tracking mount, whose stage space aperture pose
// is its relative transform, the identity
std::shared_ptr<CameraComponent> makeCamera()
{
	auto camera= std::make_shared<CameraComponent>(MikanObjectWeakPtr());
	camera->setDefinition(std::make_shared<CameraDefinition>());
	return camera;
}
} // namespace CameraFramePoseTestsInternal

bool run_camera_frame_pose_tests()
{
	UNIT_TEST_MODULE_BEGIN("camera_frame_pose")
	UNIT_TEST_MODULE_CALL_TEST(camera_frame_pose_test_pin_reports_the_queued_pose);
	UNIT_TEST_MODULE_CALL_TEST(camera_frame_pose_test_unpin_restores_the_live_pose);
	UNIT_TEST_MODULE_END()
}

bool camera_frame_pose_test_pin_reports_the_queued_pose()
{
	using namespace CameraFramePoseTestsInternal;

	UNIT_TEST_BEGIN("a pinned composited frame pose is the stage space aperture pose")
	auto camera= makeCamera();
	glm::mat4 livePose;
	success&= camera->getStageSpaceAperturePose(livePose);

	const glm::mat4 queuedPose=
		glm::rotate(glm::translate(glm::mat4(1.f), glm::vec3(0.3f, 1.2f, -0.5f)), 0.4f, glm::vec3(0.f, 1.f, 0.f));
	success&= !matricesMatch(livePose, queuedPose);
	camera->pinCompositedFramePose(queuedPose);

	glm::mat4 pinnedPose;
	success&= camera->getStageSpaceAperturePose(pinnedPose);
	success&= matricesMatch(pinnedPose, queuedPose);

	// The double precision overload goes through the same pose
	glm::dmat4 pinnedPoseDouble;
	success&= camera->getStageSpaceAperturePose(pinnedPoseDouble);
	success&= matricesMatch(glm::mat4(pinnedPoseDouble), queuedPose);
	UNIT_TEST_COMPLETE()
}

bool camera_frame_pose_test_unpin_restores_the_live_pose()
{
	using namespace CameraFramePoseTestsInternal;

	UNIT_TEST_BEGIN("unpinning returns the camera to its live pose")
	auto camera= makeCamera();
	glm::mat4 livePose;
	success&= camera->getStageSpaceAperturePose(livePose);

	camera->pinCompositedFramePose(glm::translate(glm::mat4(1.f), glm::vec3(1.f, 2.f, 3.f)));
	camera->unpinCompositedFramePose();

	glm::mat4 afterPose;
	success&= camera->getStageSpaceAperturePose(afterPose);
	success&= matricesMatch(afterPose, livePose);
	UNIT_TEST_COMPLETE()
}
