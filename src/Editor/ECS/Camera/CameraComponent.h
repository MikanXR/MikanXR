#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "IFrameCoupledPoseProvider.h"
#include "TransformComponent.h"
#include "MikanTypeFwd.h"
#include "MikanCameraTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "SceneFwd.h"
#include "Transform.h"
#include "VRDeviceComponent.h"

#include <memory>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

class VideoFrameDistortionView;
using VideoFrameDistortionViewPtr= std::shared_ptr<VideoFrameDistortionView>;

class CameraDefinition : public TransformComponentDefinition
{
public:
	CameraDefinition();
	CameraDefinition(MikanCameraID cameraId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystemDefinition,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_ownerStageIdPropertyId;
	inline MikanStageID getOwnerStageId() const { return m_stageId; }
	void setOwnerStageId(MikanStageID stageId);

	static const std::string k_trackingMountIdPropertyId;
	inline MikanTrackingMountID getTrackingMountId() const { return m_trackingMountId; }
	void setTrackingMountId(MikanTrackingMountID trackingMountId);

	static const std::string k_videoSourceIdPropertyId;
	inline MikanVideoSourceID getVideoSourceId() const { return m_videoSourceId; }
	void setVideoSourceId(MikanVideoSourceID videoSourceId);

	static const std::string k_lightEnvironmentIdPropertyId;
	inline MikanLightID getLightEnvironmentId() const { return m_lightEnvionmentId; }
	void setLightEnvironmentId(MikanLightID lightEnvironmentId);

	static const std::string k_trackingFrameDelayPropertyId;
	inline int getTrackingFrameDelay() const { return m_trackingFrameDelay; }
	void setTrackingFrameDelay(int trackingFrameDelay);

	/// How a client paces its renders for this camera. Auto follows
	/// getIsPoseDrivenPerFrame(): a per-frame pose wants a render per video
	/// frame, a static pose lets the client run on its own clock.
	static const std::string k_frameSyncModePropertyId;
	inline MikanCameraFrameSyncMode getFrameSyncMode() const { return m_frameSyncMode; }
	void setFrameSyncMode(MikanCameraFrameSyncMode syncMode);

	/// Multiplier on the video source resolution for the color buffer a client
	/// renders for this camera. The composite stays at video resolution, so a
	/// scale above 1 is a supersample: the client rasterizes the silhouette
	/// finer than the composite consumes it and the extra coverage survives the
	/// downsample. Scaling the published pixel size with the focal length and
	/// principal point leaves the client's projection matrix unchanged, so this
	/// cannot move where anything lands in frame.
	static const std::string k_clientColorRenderScalePropertyId;
	inline float getClientColorRenderScale() const { return m_clientColorRenderScale; }
	void setClientColorRenderScale(float scale);

	/// The same multiplier for the depth and shadow buffers. They are a render
	/// pass each on the client and rarely need the color buffer's resolution.
	static const std::string k_clientAuxRenderScalePropertyId;
	inline float getClientAuxRenderScale() const { return m_clientAuxRenderScale; }
	void setClientAuxRenderScale(float scale);

	/// Ceiling on the longer edge of every buffer a client renders for this camera,
	/// applied after the scales and preserving aspect ratio. The scales alone cannot
	/// bound the result, since they multiply a video resolution the camera does not
	/// choose, so this is what keeps a setting from outrunning a client's texture
	/// allocator. One of k_clientMaxBufferDimensions.
	static const std::string k_clientMaxBufferDimensionPropertyId;
	inline MikanClientMaxBufferDimension getClientMaxBufferDimension() const { return m_clientMaxBufferDimension; }
	void setClientMaxBufferDimension(MikanClientMaxBufferDimension maxDimension);
	/// The ceiling in pixels, which is what the size arithmetic wants
	int getClientMaxBufferDimensionPixels() const;

	/// Range a client render scale is held to, and the step its slider moves in
	static const float k_minClientRenderScale;
	static const float k_maxClientRenderScale;
	static const float k_clientRenderScaleStep;

	/// The size a client renders at for a video source of this size: the scale applied,
	/// then the ceiling applied to the longer edge with the aspect ratio kept. Returns
	/// true when the ceiling is what decided the result.
	///
	/// The one place this arithmetic lives. The published size and the size the editor
	/// panel reports both come through here, so the two cannot disagree.
	static bool computeClientRenderSize(int videoWidth, int videoHeight, float scale, int maxBufferDimension,
										int& outWidth, int& outHeight);

	static const std::string k_apertureOrientationOffsetPropertyId;
	static const std::string k_aperturePositionOffsetPropertyId;
	inline MikanQuatd getApertureOffsetOrientation() const { return m_apertureOrientationOffset; }
	inline MikanVector3d getApertureOffsetPosition() const { return m_aperturePositionOffset; }
	void setAperturePoseOffset(const MikanQuatd& q, const MikanVector3d& p);
	void clearAperturePoseOffset();

	static const std::string k_hasValidApertureOffsetPropertyId;
	bool hasValidApertureOffset() const { return m_bHasValidApertureOffset; }

	/// Multiplier applied to MoGe-2's metric depth for captures from this
	/// camera. The model's scale head guesses scale from image appearance, so
	/// an unusual lens can be off by an integer factor while the geometry
	/// stays excellent; the factor is a stable property of the camera/lens, so
	/// it is calibrated once against an ArUco marker of known size and reused.
	static const std::string k_depthMeshScaleCorrectionPropertyId;
	inline float getDepthMeshScaleCorrection() const { return m_depthMeshScaleCorrection; }
	void setDepthMeshScaleCorrection(float scaleCorrection);

	/// Whether this camera's transform is rewritten every frame from an outside
	/// pose source, which makes it derived state rather than authored config.
	/// Runtime only and deliberately unserialized: it describes how the camera is
	/// being driven right now, and CameraComponent::update sets it from what it
	/// actually finds. A tracking mount is known from the definition alone, but a
	/// frame-coupled video source is not, so the component reports that in.
	/// The effective value is a read-only property so a client can resolve the
	/// auto frame sync mode the same way the editor does.
	static const std::string k_poseDrivenPerFramePropertyId;
	void setPoseDrivenPerFrame(bool bDriven);
	inline bool getIsPoseDrivenPerFrame() const
	{
		return m_bPoseDrivenPerFrame || m_trackingMountId != INVALID_MIKAN_ID;
	}

	virtual bool wantsSaveForPropertyChange(const ConfigPropertyChangeSet& changedPropertySet) const override
	{
		// A per-frame pose makes the transform derived, so saving it churns the
		// project file at the video frame rate and buries every other line in the
		// log. The authored data for such a camera is its aperture offset, or the
		// pose offset on the video source, never this. Returning false here also
		// keeps the write out of undo, which is equally right: nobody wants to
		// step back through a camera's tracking history.
		if (getIsPoseDrivenPerFrame()
			&& (changedPropertySet.hasPropertyName(TransformComponentDefinition::k_relativePositionPropertyId)
				|| changedPropertySet.hasPropertyName(TransformComponentDefinition::k_relativeQuaternionPropertyId)
				|| changedPropertySet.hasPropertyName(TransformComponentDefinition::k_relativeScalePropertyId)))
		{
			return false;
		}

		// The pose-driven flag is runtime state that is never written to the
		// project file, so its notification exists only to reach clients
		if (changedPropertySet.getSet().size() == 1
			&& changedPropertySet.hasPropertyName(k_poseDrivenPerFramePropertyId))
		{
			return false;
		}

		return TransformComponentDefinition::wantsSaveForPropertyChange(changedPropertySet);
	}

private:
	MikanStageID m_stageId= INVALID_MIKAN_ID;
	MikanLightID m_lightEnvionmentId= INVALID_MIKAN_ID;
	MikanTrackingMountID m_trackingMountId= INVALID_MIKAN_ID;
	MikanVideoSourceID m_videoSourceId= INVALID_MIKAN_ID;
	int m_trackingFrameDelay= 0;
	MikanCameraFrameSyncMode m_frameSyncMode= MikanCameraFrameSyncMode_Auto;
	float m_clientColorRenderScale= 1.f;
	float m_clientAuxRenderScale= 1.f;
	MikanClientMaxBufferDimension m_clientMaxBufferDimension= MikanClientMaxBufferDimension_4096;
	float m_depthMeshScaleCorrection= 1.f;
	MikanQuatd m_apertureOrientationOffset;
	MikanVector3d m_aperturePositionOffset;
	bool m_bHasValidApertureOffset= false;
	bool m_bPoseDrivenPerFrame= false;
};

class CameraComponent : public TransformComponent
{
public:
	CameraComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void dispose() override;
	virtual void update(float deltaSeconds) override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline static const std::string k_componentClassName= "CameraComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline CameraDefinitionPtr getCameraDefinition() const
	{
		return std::static_pointer_cast<CameraDefinition>(m_definition);
	}
	inline MikanCameraID getCameraId() const { return getCameraDefinition()->getComponentId(); }
	StageComponentConstPtr getOwnerStageComponent() const;
	eTrackingVolumeType getTrackingVolumeType() const;
	VRTrackingVolumeComponentConstPtr getVRTrackingVolumeComponent() const;
	VRTrackingVolumeDefinitionConstPtr getVRTrackingVolumeDefinition() const;
	VRTrackingVolumeDefinitionPtr getVRTrackingVolumeDefinitionMutable();
	bool hasValidTrackingMountComponent() const;
	TrackingMountComponentConstPtr getTrackingMountComponent() const;
	TrackingMountDefinitionConstPtr getTrackingMountDefinition() const;
	TrackingMountDefinitionPtr getTrackingMountDefinitionMutable();
	VRDevicePoseViewPtr makeTrackingMountPoseView(eVRDevicePoseSpace space) const;
	VideoSourceComponentPtr getVideoSourceComponent() const;
	void setVideoSourceById(MikanVideoSourceID videoSourceId);

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	// Helper functions used to fetch camera lens properties
	bool hasValidTrackingMountPoseView() const;
	bool getAperturePixelDimensions(int& outWidth, int& outHeight) const;
	bool areApertureIntrinsicsValid() const;
	bool getApertureIntrinsics(struct MikanVideoSourceIntrinsics& outIntrinsics) const;
	bool hasValidApertureOffsetXform() const;
	bool getApertureOffsetXform(glm::mat4& outAperatureToTrackingMountXform) const;
	bool getStageSpaceAperturePose(glm::mat4& outCameraPose) const;
	bool getStageSpaceAperturePose(glm::dmat4& outCameraPose) const;
	bool getWorldSpaceAperturePose(glm::mat4& outCameraPose) const;
	bool getWorldSpaceAperturePose(glm::dmat4& outCameraPose) const;
	glm::mat4 getStageToWorldTransform() const;
	bool getApertureProjectionMatrix(glm::mat4& outProjectionMatrix, bool bVerticalFlip= false) const;
	bool getApertureViewMatrix(glm::mat4& outViewMatrix) const;
	bool getApertureViewProjectionMatrix(glm::mat4& outVPMatrix, bool bVerticalFlip= false) const;

	// The frame sync mode with Auto resolved against the pose-driven state
	MikanCameraFrameSyncMode getEffectiveFrameSyncMode() const;

	// Helper functions to populate a camera event with the current camera properties.
	// The frame event is the properties event plus the frame index.
	// The event carries the size a client renders at, which is the video resolution
	// times the camera's client render scales, never the video resolution itself.
	// getAperturePixelDimensions stays the video resolution for everything else.
	bool makeCameraPropertiesEvent(int defaultWidth, int defaultHeight,
								   struct MikanCameraNewPropertiesEvent& outPropertiesEvent) const;
	bool makeNewCameraFrameEvent(int64_t frameIndex, int defaultWidth, int defaultHeight,
								 struct MikanCameraNewFrameEvent& newFrameEvent) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_alignCameraFunctionId;
	static const std::string k_captureSceneLightingFunctionId;
	static const std::string k_captureDepthMeshFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames);
	virtual bool invokeFunction(const std::string& functionName) override;

	void alignCamera();
	void captureSceneLighting();
	void captureDepthMesh();

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void onDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);
	void rebuildStageSpacePoseView();
	void updateAperturePoseFromTrackingMount();
	void onActiveDeviceListChanged(eTrackingRuntime runtime);

	// Frame-coupled pose (ticket E4) - only applies latest intrinsics to the video
	// source if they changed meaningfully (>1% relative fx/fy), avoiding an
	// unnecessary recomputeCameraProjectionMatrix() every single tick when (as
	// expected) ARKit's reported intrinsics are essentially constant frame to frame.
	void maybeUpdateFrameCoupledIntrinsics(VideoSourceComponentPtr videoSourceComponent,
										   const struct MikanVideoSourceIntrinsics& newIntrinsics);

	// Turns an event holding the video resolution and its matching intrinsics into one
	// holding the sizes a client should actually render at
	void applyClientRenderScales(struct MikanCameraNewPropertiesEvent& inOutPropertiesEvent) const;

private:
	SelectionComponentWeakPtr m_selectionComponent;
	VRDevicePoseViewPtr m_trackingMountPoseView_StageSpace;
};