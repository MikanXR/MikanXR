#pragma once

#include "MikanRendererFwd.h"
#include "IMkCamera.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

#include <string>

enum eCameraMovementMode : int
{
	orbit,
	fly,
	stationary,
	ortho,
};

enum class eCameraProjectionMode : int
{
	perspective,
	orthographic,
};

// The six axis-aligned look directions by name, as a yaw about +Y and a pitch,
// for the camera verbs and for the view names older projects saved
struct MikanCameraAxisView
{
	const char* name;
	float yawDegrees;
	float pitchDegrees;
};
constexpr int k_cameraAxisViewCount= 6;
extern const MikanCameraAxisView k_cameraAxisViews[k_cameraAxisViewCount];
const MikanCameraAxisView* findCameraAxisView(const std::string& name);

class MikanCamera : public IMkCamera
{
public:
	MikanCamera();
	virtual ~MikanCamera()= default;

	virtual void setName(const std::string& name) override { m_cameraName= name; }
	virtual const std::string& getName() const override { return m_cameraName; }

	virtual const glm::mat4& getProjectionMatrix() const override { return m_projectionMatrix; }
	virtual const glm::mat4& getViewMatrix() const override { return m_viewMatrix; }
	virtual glm::mat4 getViewProjectionMatrix() const override { return m_projectionMatrix * m_viewMatrix; }

	virtual float getAspectRatio() const override { return m_aspectRatio; }
	virtual float getHorizontalFOVDegrees() const override { return m_hFOVDegrees; }
	virtual float getVerticalFOVDegrees() const override { return m_vFOVDegrees; }
	virtual float getZNear() const override { return m_zNear; }
	virtual float getZFar() const override { return m_zFar; }

	virtual glm::vec3 getCameraPositionFromViewMatrix() const override;
	virtual glm::vec3 getCameraForwardFromViewMatrix() const override;
	virtual glm::vec3 getCameraRightFromViewMatrix() const override;
	virtual glm::vec3 getCameraUpFromViewMatrix() const override;
	virtual glm::mat4 getCameraTransformFromViewMatrix() const override;

	eCameraMovementMode getCameraMovementMode() const { return m_movementMode; }
	void setCameraMovementMode(eCameraMovementMode mode);

	// Projection mode (perspective vs orthographic)
	eCameraProjectionMode getProjectionMode() const { return m_projectionMode; }
	bool isOrthographic() const { return m_projectionMode == eCameraProjectionMode::orthographic; }
	void setProjectionMode(eCameraProjectionMode mode);

	// The editor camera's two projections: perspective flies, orthographic looks
	// at a target from its own yaw and pitch. The movement mode follows the
	// projection. Carrying the view direction copies the outgoing projection's
	// yaw and pitch into the incoming one, so a switch keeps the scene in view.
	void setEditorProjection(eCameraProjectionMode mode, bool bCarryViewDirection);

	// Orthographic view: one orientation (yaw about +Y, then pitch), one target,
	// and one extent (the half-height of the view in world meters)
	float getOrthoYawDegrees() const { return m_orthoYawDegrees; }
	float getOrthoPitchDegrees() const { return m_orthoPitchDegrees; }
	void setOrthoOrientation(float yawDegrees, float pitchDegrees);
	float getOrthoExtent() const { return m_orthoExtent; }
	void setOrthoExtent(float extent);
	const glm::vec3& getOrthoTargetPosition() const { return m_orthoTarget; }
	void setOrthoTargetPosition(const glm::vec3& target);
	void adjustOrthoTargetPosition(const glm::vec3& deltaTarget);
	void setOrthoView(float yawDegrees, float pitchDegrees, const glm::vec3& target, float extent);

	// The yaw and pitch of whichever editor projection is active
	float getEditorYawDegrees() const;
	float getEditorPitchDegrees() const;
	void setEditorViewDirection(float yawDegrees, float pitchDegrees);
	// Takes a view matrix back from the view cube: the fly camera as its position
	// plus yaw and pitch, the orthographic camera as yaw and pitch about its target
	void setEditorViewMatrix(const glm::mat4& viewMatrix);

	// Update the projection to match the actual viewport aspect ratio
	void setViewportAspect(float aspectRatio);

	void applyMonoCameraIntrinsics(struct MikanVideoSourceIntrinsics* cameraIntrinsics);

	void setCameraTransform(const glm::mat4& poseXform);
	void setPosition(const glm::vec3& location);
	void lookAt(const glm::vec3& target);

	glm::vec3 getFlyPosition() const { return glm::vec3(m_flyTransform[3]); }
	float getFlyYawDegrees() const { return m_flyYawDegrees; }
	float getFlyPitchDegrees() const { return m_flyPitchDegrees; }
	void setFlyPose(const glm::vec3& position, float yawDegrees, float pitchDegrees);

	void adjustFlyForward(float distance);
	void adjustFlyRight(float distance);
	void adjustFlyUp(float distance);
	void adjustFlyYaw(float deltaDegrees);
	void adjustFlyPitch(float deltaDegrees);

	void setOrbitLocation(float yawDegrees, float pitchDegrees, float radius);
	void setOrbitYaw(float yawDegrees);
	void setOrbitPitch(float pitchDegrees);
	void setOrbitRadius(float radius);
	void setOrbitTargetPosition(const glm::vec3& cameraTarget);
	void adjustOrbitAngles(float deltaYaw, float deltaPitch);
	void adjustOrbitRadius(float deltaRadius);
	void adjustOrbitTargetPosition(const glm::vec3& deltaTarget);

	void computeCameraRayThruPixel(MikanViewportConstPtr viewportPtr, const glm::vec2& pixelLocation,
								   glm::vec3& outRayOrigin, glm::vec3& outRayDirection) const;

protected:
	void applyStationaryParamsToViewMatrix();
	void applyFlyParamsToViewMatrix();
	void applyOrbitParamsToViewMatrix();
	void applyOrthoParamsToViewMatrix();
	void rebuildProjectionMatrix();
	// Rebuild the fly transform rotation from the current yaw and pitch, keeping the position
	void rebuildFlyTransform();

	const float k_default_aspect_ratio= 16.f / 9.f;
	const float k_default_camera_vfov= 35.f;
	const float k_default_camera_z_near= 0.1f;
	const float k_default_camera_z_far= 5000.f;
	const float k_camera_min_zoom= 0.01f;

public:
	// The orthographic view a fresh camera looks from: the isometric diagonal
	static constexpr float k_default_ortho_yaw_degrees= 45.f;
	static constexpr float k_default_ortho_pitch_degrees= -35.264f;
	static constexpr float k_default_ortho_extent= 5.f;

protected:
	std::string m_cameraName;

	float m_aspectRatio;
	float m_hFOVDegrees;
	float m_vFOVDegrees;
	float m_zNear;
	float m_zFar;
	glm::mat4 m_projectionMatrix;
	glm::mat4 m_viewMatrix;

	// Projection parameters
	eCameraProjectionMode m_projectionMode= eCameraProjectionMode::perspective;
	float m_viewportAspectRatio= 16.f / 9.f;

	// Orthographic camera parameters
	float m_orthoYawDegrees= k_default_ortho_yaw_degrees;
	float m_orthoPitchDegrees= k_default_ortho_pitch_degrees;
	glm::vec3 m_orthoTarget= glm::vec3(0.f);
	float m_orthoExtent= k_default_ortho_extent;

	// Stationary camera parameters
	glm::mat4 m_stationaryTransform;

	// Stationary camera parameters
	float m_flyYawDegrees;
	float m_flyPitchDegrees;
	glm::mat4 m_flyTransform;

	// Orbit camera parameters
	float m_orbitYawDegrees;
	float m_orbitPitchDegrees;
	float m_orbitRadius;
	glm::vec3 m_orbitTargetPosition;

	eCameraMovementMode m_movementMode= eCameraMovementMode::orbit;
};
