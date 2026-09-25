#pragma once

#include "IARKitVideoDevice.h"
#include "IFrameCoupledPoseProvider.h"
#include "MikanVideoSourceTypes.h"
#include "MkRendererFwd.h"
#include "VideoSourceComponent.h"

#include <atomic>
#include <mutex>

class ARKitVideoSourceDefinition : public VideoSourceDefinition
{
public:
	ARKitVideoSourceDefinition();
	ARKitVideoSourceDefinition(MikanVideoSourceID videoSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	// Stored as int (not uint16_t) so getPropertyValue()'s static-type-inferred
	// MikanVariantType matches the reflected MikanARKitVideoSourceValues::base_port
	// field (an int) exactly - see ServerEntitySerializer's per-type rules. Narrowed
	// to uint16_t only at the ARKitVideoConnectionSettings boundary in
	// openVideoSource().
	static const std::string k_basePortPropertyId;
	inline int getBasePort() const { return m_basePort; }
	void setBasePort(int basePort);

	// ARKit-world-to-stage offset solved by marker alignment, persisted so an
	// alignment survives closing the project.
	//
	// Deliberately not a reflected property. A property descriptor would need
	// matching legs in the client-API values struct and getPropertyValue to
	// satisfy the schema guard, which means a wire change and regenerated
	// bindings for something no client application has any use for.
	static const std::string k_poseOffsetPropertyId;
	inline const glm::mat4& getPoseOffset() const { return m_poseOffset; }
	inline bool hasPoseOffset() const { return m_bHasPoseOffset; }
	void setPoseOffset(const glm::mat4& poseOffset);
	void clearPoseOffset();

private:
	int m_basePort;
	glm::mat4 m_poseOffset;
	bool m_bHasPoseOffset= false;
};

class ARKitVideoSourceComponent : public VideoSourceComponent,
								  public IARKitVideoDeviceListener,
								  public IFrameCoupledPoseProvider
{
public:
	ARKitVideoSourceComponent(MikanObjectWeakPtr owner);

	virtual void init() override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "ARKitVideoSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline ARKitVideoSourceDefinitionPtr getARKitVideoSourceDefinition() const
	{
		return std::static_pointer_cast<ARKitVideoSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	// -- Video Source Interface ----
	virtual std::string getDevicePath() const override;
	virtual std::string getDeviceAPI() const override;
	virtual bool openVideoSource() override;
	virtual void closeVideoSource() override;
	virtual eVideoStreamingStatus getVideoStreamingStatus() const override;
	virtual bool getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const override;

	// -- Zero-copy GPU texture access (ticket E3; NV12->RGBA shader pass as of "Phase 6") ----
	virtual IMkTexturePtr getDirectColorTexture() const override;
	virtual void processDirectVideoFrame() override;

	// Latest bundle's frameSeq (video or pose - whichever arrived most
	// recently), or -1 if none has arrived yet (ticket E4 - see
	// VideoSourceComponent::getDirectFrameIndex()'s own comment for why this
	// exists: it's what makes VideoFrameDistortionView's change-detection work
	// again for this GPU-direct source).
	virtual int64_t getDirectFrameIndex() const override;

	// -- IFrameCoupledPoseProvider ----
	virtual void setPoseOffset(const glm::mat4& worldToStageXform) override;
	virtual glm::mat4 getPoseOffset() const override;
	virtual bool hasPoseOffset() const override;

	virtual bool getLatestFrameCoupledPose(glm::mat4& outTransform, MikanVideoSourceIntrinsics& outIntrinsics,
										   uint32_t& outFrameSeq) const override;
	virtual bool getLatestSourceWorldPose(glm::mat4& outTransform, uint32_t& outFrameSeq) const override;

	// -- IARKitVideoDeviceListener ----
	virtual void notifyDeviceOpened(const class IARKitVideoDevice* device) override;
	virtual void notifyDeviceClosed(const class IARKitVideoDevice* device) override;
	virtual void notifyFrameBundleReceived(const ARKitVideoFrameBundle& bundle) override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	bool isPendingOpen() const { return m_bPendingOpen; }

protected:
	virtual eVideoStreamingStatus startVideoStreamInternal() override;
	virtual void stopVideoStreamInternal() override;

	void onDefinitionMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

private:
	IARKitVideoDevicePtr m_arkitVideoDevice= nullptr;
	bool m_bPendingOpen= false;

	// Cached from the most recently received frame bundle - IARKitVideoDevice has no
	// getStreamProperties()-style query the way INetworkVideoDevice does (see
	// NetworkVideoSourceComponent), since ARKit's video dimensions are only known
	// once a bundle actually arrives.
	int m_lastVideoWidth= 0;
	int m_lastVideoHeight= 0;

	// NV12->RGBA GLSL conversion pipeline ("Phase 6"). The plugin only exposes raw
	// luma/chroma GL texture ids (IARKitVideoDevice::getLumaTextureGlId/
	// getChromaTextureGlId - see that header's comment on why this crosses the
	// MikanCoreApp/MikanRenderer layering boundary as raw ids rather than
	// IMkTexturePtrs, and on why the plugin can't do the shader pass itself: no GL
	// context/shader-cache access at that layer). This component wraps each id in
	// an IMkExternalTexture (setExternalPlatformTexture() is cheap to call again
	// every frame - it early-outs unless the underlying GL id actually changed,
	// see GlExternalTexture::setExternalPlatformTexture) and owns the actual
	// two-sampler fullscreen shader pass that converts them into a displayable
	// RGBA output texture. processDirectVideoFrame() runs the pass once per tick;
	// getDirectColorTexture() just returns the already-converted output.
	IMkExternalTexturePtr m_lumaTextureWrapper;
	IMkExternalTexturePtr m_chromaTextureWrapper;
	IMkFrameBufferPtr m_nv12ConversionFrameBuffer;
	MkMaterialConstPtr m_nv12ConversionMaterial;
	MkMaterialInstancePtr m_nv12ConversionMaterialInstance;
	IMkTriangulatedMeshPtr m_nv12ConversionQuad;
	int m_nv12ConversionWidth= 0;
	int m_nv12ConversionHeight= 0;

	// Latest frame-coupled pose (ticket E4). Historically notifyFrameBundleReceived()
	// could fire from a background pose-receiver worker thread while
	// getLatestFrameCoupledPose()/getDirectFrameIndex() were read from the main/GL
	// thread inside CameraComponent::update(), which is what originally motivated
	// this mutex. Since pose moved into the video RTP stream's own header
	// extension, notifyFrameBundleReceived() now fires from
	// MikanARKitVideoDevice::update() - the same main/GL thread as the reader -
	// making writer and reader same-thread in practice. The mutex is kept anyway:
	// IARKitVideoDeviceListener's contract still doesn't guarantee a specific
	// calling thread, so removing it would be relying on an implementation detail
	// rather than the documented interface.
	mutable std::mutex m_latestPoseMutex;
	struct LatestPose
	{
		// Camera-to-stage: what CameraComponent consumes, offset already applied.
		glm::mat4 transform= glm::mat4(1.0f);
		// The same pose in ARKit's own world space, kept because solving for the
		// offset has to start from a pose the offset has not touched.
		glm::mat4 sourceWorldTransform= glm::mat4(1.0f);
		MikanVideoSourceIntrinsics intrinsics;
		uint32_t frameSeq= 0;
		bool bValid= false;
	};
	LatestPose m_latestPose;

	// Latest frameSeq seen across ANY bundle (video/depth/pose), not just ones with
	// pose - getDirectFrameIndex() needs this to keep VideoFrameDistortionView's
	// change-detection working even for bundles that arrive without a pose (e.g.
	// depth streaming disabled, or an occasional dropped pose packet). A plain
	// atomic rather than folding into m_latestPoseMutex since it's a single scalar
	// updated unconditionally on every bundle, independent of pose validity.
	std::atomic<int64_t> m_lastBundleFrameSeq{-1};

	// Previous bundle's frameSeq, used to notice the phone restarting its ARKit
	// session. Touched only where the pose is consumed, under the same
	// same-thread reasoning as m_latestPose above.
	uint32_t m_lastPoseFrameSeq= 0;

	// Frame index of the last NV12->RGBA conversion, so processDirectVideoFrame()
	// can skip re-converting an unchanged frame. Only touched on the GL thread.
	int64_t m_lastConvertedFrameIndex= -1;
};
