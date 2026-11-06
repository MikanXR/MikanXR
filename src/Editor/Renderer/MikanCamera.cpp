#include "App.h"
#include "CameraMath.h"
#include "MikanCamera.h"
#include "MikanViewport.h"
#include "InputManager.h"
#include "MathUtility.h"
#include "MathGLM.h"
#include "MikanVideoSourceTypes.h"
#include "MikanCameraTypes.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>

const MikanCameraAxisView k_cameraAxisViews[k_cameraAxisViewCount]= {
	{"top", 0.f, -90.f},  {"bottom", 0.f, 90.f}, {"front", 0.f, 0.f},
	{"back", 180.f, 0.f}, {"left", 270.f, 0.f},  {"right", 90.f, 0.f},
};

const MikanCameraAxisView* findCameraAxisView(const std::string& name)
{
	for (const MikanCameraAxisView& axisView : k_cameraAxisViews)
	{
		if (name == axisView.name)
			return &axisView;
	}

	return nullptr;
}

namespace
{
// The orientation of a yaw about +Y followed by a pitch about the turned right
// axis. The camera looks along its -Z axis, so a negative pitch looks down.
glm::mat3 buildYawPitchOrientation(float yawDegrees, float pitchDegrees)
{
	const glm::mat3 pitchRot= glm::rotate(glm::mat4(1.f), degrees_to_radians(pitchDegrees), glm::vec3(1.f, 0.f, 0.f));
	const glm::mat3 yawRot= glm::rotate(glm::mat4(1.f), degrees_to_radians(yawDegrees), glm::vec3(0.f, 1.f, 0.f));

	return glm::mat3(glm_composite_xform(pitchRot, yawRot));
}

// The yaw and pitch that look along a direction. Straight up or down every yaw
// looks the same way, so the fallback yaw stands.
void directionToYawPitch(const glm::vec3& direction, float fallbackYawDegrees, float& outYawDegrees,
						 float& outPitchDegrees)
{
	const glm::vec3 forward= glm::normalize(direction);
	const float horizontal= sqrtf(forward.x * forward.x + forward.z * forward.z);

	outPitchDegrees= radians_to_degrees(asinf(clampf(forward.y, -1.f, 1.f)));
	outYawDegrees= horizontal > 1e-4f ? radians_to_degrees(atan2f(-forward.x, -forward.z)) : fallbackYawDegrees;
}
} // namespace

MikanCamera::MikanCamera()
{
	m_viewMatrix= glm::mat4(1.f);

	m_hFOVDegrees= k_default_camera_vfov * k_default_aspect_ratio;
	m_vFOVDegrees= k_default_camera_vfov;
	m_zNear= k_default_camera_z_near;
	m_zFar= k_default_camera_z_far;

	m_projectionMatrix= glm::perspective(degrees_to_radians(m_vFOVDegrees), k_default_aspect_ratio, m_zNear, m_zFar);

	// Stationary camera parameters
	m_stationaryTransform= glm::mat4(1.f);

	// Fly camera parameters
	m_flyTransform= glm::mat4(1.f);

	// Orbit camera parameters
	m_orbitYawDegrees= 0.0f;
	m_orbitPitchDegrees= 0.0f;
	m_orbitRadius= k_camera_min_zoom;
	m_orbitTargetPosition= glm::vec3(0.0f);

	// Default to fly movement
	m_flyYawDegrees= 0.0f;
	m_flyPitchDegrees= 0.0f;
	m_movementMode= eCameraMovementMode::fly;
	applyFlyParamsToViewMatrix();
}

void MikanCamera::setOrbitLocation(float yawDegrees, float pitchDegrees, float radius)
{
	m_orbitYawDegrees= wrap_degrees(yawDegrees);
	m_orbitPitchDegrees= clampf(pitchDegrees, 0.0f, 90.0f);
	m_orbitRadius= fmaxf(radius, k_camera_min_zoom);
	applyOrbitParamsToViewMatrix();
}

void MikanCamera::setCameraMovementMode(eCameraMovementMode newMode)
{
	if (newMode != m_movementMode)
	{
		switch (newMode)
		{
		case orbit:
			applyOrbitParamsToViewMatrix();
			break;
		case fly:
			applyFlyParamsToViewMatrix();
			break;
		case stationary:
			applyStationaryParamsToViewMatrix();
			break;
		case ortho:
			applyOrthoParamsToViewMatrix();
			break;
		}
		m_movementMode= newMode;
	}
}

void MikanCamera::setProjectionMode(eCameraProjectionMode mode)
{
	if (m_projectionMode != mode)
	{
		m_projectionMode= mode;
		rebuildProjectionMatrix();
	}
}

void MikanCamera::setEditorProjection(eCameraProjectionMode mode, bool bCarryViewDirection)
{
	if (mode == m_projectionMode)
		return;

	if (mode == eCameraProjectionMode::orthographic)
	{
		if (bCarryViewDirection)
		{
			m_orthoYawDegrees= m_flyYawDegrees;
			m_orthoPitchDegrees= m_flyPitchDegrees;
		}

		m_projectionMode= eCameraProjectionMode::orthographic;
		m_movementMode= eCameraMovementMode::ortho;
		applyOrthoParamsToViewMatrix();
	}
	else
	{
		if (bCarryViewDirection)
		{
			m_flyYawDegrees= m_orthoYawDegrees;
			m_flyPitchDegrees= clampf(m_orthoPitchDegrees, -90.f, 90.f);
			rebuildFlyTransform();
		}

		m_projectionMode= eCameraProjectionMode::perspective;
		m_movementMode= eCameraMovementMode::fly;
		applyFlyParamsToViewMatrix();
	}

	rebuildProjectionMatrix();
}

void MikanCamera::setOrthoOrientation(float yawDegrees, float pitchDegrees)
{
	m_orthoYawDegrees= wrap_degrees(yawDegrees);
	m_orthoPitchDegrees= clampf(pitchDegrees, -90.f, 90.f);

	if (m_movementMode == eCameraMovementMode::ortho)
	{
		applyOrthoParamsToViewMatrix();
	}
}

void MikanCamera::setOrthoExtent(float extent)
{
	m_orthoExtent= fmaxf(extent, k_camera_min_zoom);

	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		rebuildProjectionMatrix();
	}
}

void MikanCamera::setOrthoTargetPosition(const glm::vec3& target)
{
	m_orthoTarget= target;

	if (m_movementMode == eCameraMovementMode::ortho)
	{
		applyOrthoParamsToViewMatrix();
	}
}

void MikanCamera::adjustOrthoTargetPosition(const glm::vec3& deltaTarget)
{
	if (m_movementMode == eCameraMovementMode::ortho)
	{
		m_orthoTarget+= deltaTarget;
		applyOrthoParamsToViewMatrix();
	}
}

void MikanCamera::setOrthoView(float yawDegrees, float pitchDegrees, const glm::vec3& target, float extent)
{
	// Seeding the orthographic view of a perspective camera must not touch the
	// matrices its fly pose just built, so the live matrices rebuild only while
	// the camera is looking through the orthographic projection
	m_orthoYawDegrees= wrap_degrees(yawDegrees);
	m_orthoPitchDegrees= clampf(pitchDegrees, -90.f, 90.f);
	m_orthoTarget= target;
	m_orthoExtent= fmaxf(extent, k_camera_min_zoom);

	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		applyOrthoParamsToViewMatrix();
		rebuildProjectionMatrix();
	}
}

float MikanCamera::getEditorYawDegrees() const
{
	return m_projectionMode == eCameraProjectionMode::orthographic ? m_orthoYawDegrees : m_flyYawDegrees;
}

float MikanCamera::getEditorPitchDegrees() const
{
	return m_projectionMode == eCameraProjectionMode::orthographic ? m_orthoPitchDegrees : m_flyPitchDegrees;
}

void MikanCamera::setEditorViewDirection(float yawDegrees, float pitchDegrees)
{
	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		setOrthoOrientation(yawDegrees, pitchDegrees);
	}
	else
	{
		setFlyPose(getFlyPosition(), yawDegrees, pitchDegrees);
	}
}

void MikanCamera::setEditorViewMatrix(const glm::mat4& viewMatrix)
{
	// The camera transform is the inverse of the view, and the camera looks along its -Z axis
	const glm::mat4 cameraTransform= glm::inverse(viewMatrix);
	const glm::vec3 forward= -glm::vec3(cameraTransform[2]);

	float yawDegrees, pitchDegrees;
	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		directionToYawPitch(forward, m_orthoYawDegrees, yawDegrees, pitchDegrees);
		setOrthoOrientation(yawDegrees, pitchDegrees);
	}
	else
	{
		directionToYawPitch(forward, m_flyYawDegrees, yawDegrees, pitchDegrees);
		setFlyPose(glm::vec3(cameraTransform[3]), yawDegrees, pitchDegrees);
	}
}

void MikanCamera::setViewportAspect(float aspectRatio)
{
	if (aspectRatio > 0.f)
	{
		m_viewportAspectRatio= aspectRatio;

		// Compositor cameras drive their projection from real lens intrinsics
		// (applyMonoCameraIntrinsics), so only rebuild for the editor camera modes.
		if (m_movementMode != eCameraMovementMode::stationary)
		{
			rebuildProjectionMatrix();
		}
	}
}

void MikanCamera::setCameraTransform(const glm::mat4& xform)
{
	switch (m_movementMode)
	{
	case orbit:
		// Ignored in this movement mode
		break;
	case fly:
		m_flyTransform= xform;
		applyFlyParamsToViewMatrix();
		break;
	case stationary:
		m_stationaryTransform= xform;
		applyStationaryParamsToViewMatrix();
		break;
	}
}

void MikanCamera::setPosition(const glm::vec3& location)
{
	switch (m_movementMode)
	{
	case orbit:
		// Ignored in this movement mode
		break;
	case fly:
		m_flyTransform[3]= glm::vec4(location, 1.f);
		applyFlyParamsToViewMatrix();
		break;
	case stationary:
		m_stationaryTransform[3]= glm::vec4(location, 1.f);
		applyStationaryParamsToViewMatrix();
		break;
	}
}

void MikanCamera::lookAt(const glm::vec3& target)
{
	switch (m_movementMode)
	{
	case orbit:
		// Ignored in this movement mode
		break;
	case fly:
	{
		m_viewMatrix= glm::lookAt(glm::vec3(m_flyTransform[3]), target, glm::vec3(0, 1, 0)); // +Y is up.
		m_flyTransform= getCameraTransformFromViewMatrix();
	}
	break;
	case stationary:
	{
		m_viewMatrix= glm::lookAt(glm::vec3(m_stationaryTransform[3]), target, glm::vec3(0, 1, 0)); // +Y is up.
		m_flyTransform= getCameraTransformFromViewMatrix();
	}
	break;
	}
}

void MikanCamera::adjustFlyForward(float distance)
{
	if (m_movementMode == eCameraMovementMode::fly)
	{
		const glm::vec3 position= glm_mat4_get_position(m_flyTransform);
		const glm::vec3 delta= -glm_mat4_get_z_axis(m_flyTransform) * distance;
		glm_mat4_set_position(m_flyTransform, position + delta);

		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::adjustFlyRight(float distance)
{
	if (m_movementMode == eCameraMovementMode::fly)
	{
		const glm::vec3 position= glm_mat4_get_position(m_flyTransform);
		const glm::vec3 delta= glm_mat4_get_x_axis(m_flyTransform) * distance;
		glm_mat4_set_position(m_flyTransform, position + delta);

		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::adjustFlyUp(float distance)
{
	if (m_movementMode == eCameraMovementMode::fly)
	{
		const glm::vec3 position= glm_mat4_get_position(m_flyTransform);
		const glm::vec3 delta= glm_mat4_get_y_axis(m_flyTransform) * distance;
		glm_mat4_set_position(m_flyTransform, position + delta);

		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::adjustFlyYaw(float deltaDegrees)
{
	if (m_movementMode == eCameraMovementMode::fly)
	{
		m_flyYawDegrees= wrap_degrees(m_flyYawDegrees + deltaDegrees);
		rebuildFlyTransform();

		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::adjustFlyPitch(float deltaDegrees)
{
	if (m_movementMode == eCameraMovementMode::fly)
	{
		m_flyPitchDegrees= clampf(m_flyPitchDegrees - deltaDegrees, -90.f, 90.f);
		rebuildFlyTransform();

		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::setFlyPose(const glm::vec3& position, float yawDegrees, float pitchDegrees)
{
	m_flyYawDegrees= wrap_degrees(yawDegrees);
	m_flyPitchDegrees= clampf(pitchDegrees, -90.f, 90.f);

	glm_mat4_set_position(m_flyTransform, position);
	rebuildFlyTransform();

	if (m_movementMode == eCameraMovementMode::fly)
	{
		applyFlyParamsToViewMatrix();
	}
}

void MikanCamera::rebuildFlyTransform()
{
	const glm::vec3 position= glm_mat4_get_position(m_flyTransform);

	glm_mat4_set_rotation(m_flyTransform, buildYawPitchOrientation(m_flyYawDegrees, m_flyPitchDegrees));
	glm_mat4_set_position(m_flyTransform, position);
}

void MikanCamera::setOrbitYaw(float yawDegrees)
{
	m_orbitYawDegrees= yawDegrees;
	applyOrbitParamsToViewMatrix();
}

void MikanCamera::setOrbitPitch(float pitchDegrees)
{
	m_orbitPitchDegrees= pitchDegrees;
	applyOrbitParamsToViewMatrix();
}

void MikanCamera::setOrbitRadius(float radius)
{
	m_orbitRadius= radius;
	applyOrbitParamsToViewMatrix();
}

void MikanCamera::setOrbitTargetPosition(const glm::vec3& cameraTarget)
{
	m_orbitTargetPosition= cameraTarget;
	applyOrbitParamsToViewMatrix();
}

void MikanCamera::adjustOrbitAngles(float deltaYaw, float deltaPitch)
{
	if (m_movementMode == eCameraMovementMode::orbit)
	{
		setOrbitLocation(m_orbitYawDegrees + deltaYaw, m_orbitPitchDegrees + deltaPitch, m_orbitRadius);
	}
}

void MikanCamera::adjustOrbitRadius(float deltaRadius)
{
	if (m_movementMode == eCameraMovementMode::orbit)
	{
		setOrbitLocation(m_orbitYawDegrees, m_orbitPitchDegrees, m_orbitRadius + deltaRadius);
	}
}

void MikanCamera::adjustOrbitTargetPosition(const glm::vec3& deltaTarget)
{
	if (m_movementMode == eCameraMovementMode::orbit)
	{
		m_orbitTargetPosition+= deltaTarget;
		applyOrbitParamsToViewMatrix();
	}
}

void MikanCamera::applyMonoCameraIntrinsics(MikanVideoSourceIntrinsics* cameraIntrinsics)
{
	assert(cameraIntrinsics->intrinsics_type == MikanIntrinsicsType::MONO_CAMERA_INTRINSICS);

	const MikanMonoIntrinsics& monoIntrinsics= cameraIntrinsics->getMonoIntrinsics();
	monoIntrinsics.undistorted_camera_matrix;

	int unusedViewport[4];
	computeOpenGLProjMatFromCameraIntrinsics(cameraIntrinsics->getMonoIntrinsics(), m_projectionMatrix, unusedViewport);

	m_aspectRatio= (float)monoIntrinsics.aspect_ratio;
	m_vFOVDegrees= (float)monoIntrinsics.vfov;
	m_hFOVDegrees= (float)monoIntrinsics.hfov;
	m_zNear= (float)monoIntrinsics.znear;
	m_zFar= (float)monoIntrinsics.zfar;
}

void MikanCamera::applyStationaryParamsToViewMatrix()
{
	m_viewMatrix= computeGLMCameraViewMatrix(m_stationaryTransform);
}

void MikanCamera::applyFlyParamsToViewMatrix() { m_viewMatrix= computeGLMCameraViewMatrix(m_flyTransform); }

void MikanCamera::applyOrbitParamsToViewMatrix()
{
	const float yawRadians= degrees_to_radians(m_orbitYawDegrees);
	const float pitchRadians= degrees_to_radians(m_orbitPitchDegrees);
	const float xzRadiusAtPitch= m_orbitRadius * cosf(pitchRadians);
	const glm::vec3 cameraPosition(m_orbitTargetPosition.x + xzRadiusAtPitch * sinf(yawRadians),
								   m_orbitTargetPosition.y + m_orbitRadius * sinf(pitchRadians),
								   m_orbitTargetPosition.z + xzRadiusAtPitch * cosf(yawRadians));

	if (fabsf(m_orbitPitchDegrees) < 85.0f)
	{
		m_viewMatrix= glm::lookAt(cameraPosition,
								  m_orbitTargetPosition, // Look at tracking origin
								  glm::vec3(0, 1, 0));   // +Y is up.
	}
	else
	{
		m_viewMatrix= glm::lookAt(cameraPosition,
								  m_orbitTargetPosition, // Look at tracking origin
								  glm::vec3(sinf(yawRadians), 0.0f, -cosf(yawRadians)));
	}
}

void MikanCamera::applyOrthoParamsToViewMatrix()
{
	// The same yaw and pitch orientation the fly camera uses, so the two
	// projections agree on a direction, with the eye pulled far back along the
	// view axis so scene geometry at any reasonable stage depth stays between
	// the near and far clip planes
	const glm::mat3 orientation= buildYawPitchOrientation(m_orthoYawDegrees, m_orthoPitchDegrees);
	const glm::vec3 forward= -orientation[2];
	const float eyeDistance= m_zFar * 0.5f;

	glm::mat4 cameraTransform(orientation);
	glm_mat4_set_position(cameraTransform, m_orthoTarget - forward * eyeDistance);

	m_viewMatrix= computeGLMCameraViewMatrix(cameraTransform);
}

void MikanCamera::rebuildProjectionMatrix()
{
	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		const float halfHeight= fmaxf(m_orthoExtent, k_camera_min_zoom);
		const float halfWidth= halfHeight * m_viewportAspectRatio;
		m_projectionMatrix= glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, m_zNear, m_zFar);
	}
	else
	{
		m_projectionMatrix= glm::perspective(degrees_to_radians(m_vFOVDegrees), m_viewportAspectRatio, m_zNear, m_zFar);
	}

	m_aspectRatio= m_viewportAspectRatio;
}

glm::vec3 MikanCamera::getCameraPositionFromViewMatrix() const
{
	// Assumes no scaling
	const glm::mat3 rotMat(m_viewMatrix);
	const glm::vec3 d(m_viewMatrix[3]);
	const glm::vec3 position= -d * rotMat;

	return position;
}

glm::vec3 MikanCamera::getCameraRightFromViewMatrix() const
{
	return glm::vec3(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);
}

glm::vec3 MikanCamera::getCameraUpFromViewMatrix() const
{
	return glm::vec3(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]);
}

glm::vec3 MikanCamera::getCameraForwardFromViewMatrix() const
{
	// Camera forward is along negative Z-axis
	return glm::vec3(m_viewMatrix[0][2], m_viewMatrix[1][2], m_viewMatrix[2][2]) * -1.f;
}

glm::mat4 MikanCamera::getCameraTransformFromViewMatrix() const
{
	return glm::mat4(glm::vec4(getCameraRightFromViewMatrix(), 0.f), glm::vec4(getCameraUpFromViewMatrix(), 0.f),
					 glm::vec4(getCameraForwardFromViewMatrix() * -1.f, 0.f), // Camera forward is along negative Z-axis
					 glm::vec4(getCameraPositionFromViewMatrix(), 1.f));
}

void MikanCamera::computeCameraRayThruPixel(MikanViewportConstPtr viewport, const glm::vec2& viewportPixelPos,
											glm::vec3& outRayOrigin, glm::vec3& outRayDirection) const
{
	glm::i32vec2 viewportSize= viewport->getViewportSize();
	const float viewportWidth= (float)viewportSize.x;
	const float viewportHeight= (float)viewportSize.y;

	// https://antongerdelan.net/opengl/raycasting.html
	// Convert the pixel location into normalized device coordinates
	const float ndcX= ((2.f * viewportPixelPos.x) / viewportWidth) - 1.f;
	const float ndcY= 1.f - ((2.f * viewportPixelPos.y) / viewportHeight);

	if (m_projectionMode == eCameraProjectionMode::orthographic)
	{
		// In an orthographic projection every ray shares the camera-forward direction;
		// it's the origin that varies across the near plane. Unproject the near-plane point.
		const glm::mat4 invViewProj= glm::inverse(m_projectionMatrix * m_viewMatrix);
		glm::vec4 nearPointWorld= invViewProj * glm::vec4(ndcX, ndcY, -1.f, 1.f);
		nearPointWorld/= nearPointWorld.w;

		outRayOrigin= glm::vec3(nearPointWorld);
		outRayDirection= getCameraForwardFromViewMatrix();
	}
	else
	{
		// Convert the nds ray into a 4d-clip space ray
		const glm::vec4 ray_clip(ndcX, ndcY, -1.f, 1.0);

		// Convert the clip space ray back into an eye space ray
		glm::vec4 ray_eye= glm::inverse(m_projectionMatrix) * ray_clip;
		ray_eye= glm::vec4(ray_eye.x, ray_eye.y, -1.f, 0.f);

		// Convert the eye space ray to world space
		outRayDirection= glm::inverse(m_viewMatrix) * ray_eye;
		outRayOrigin= getCameraPositionFromViewMatrix();
	}
}