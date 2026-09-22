#pragma once

#include "AssetFwd.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "CompositorConstants.h"
#include "Graphs/NodeError.h"
#include "MikanCameraEvents.h"
#include "MikanCameraTypes.h"
#include "MikanComponent.h"
#include "MikanCoreTypes.h"
#include "MikanRendererFwd.h"
#include "MikanTypeFwd.h"
#include "MkRendererFwd.h"
#include "NodeFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "SceneFwd.h"
#include "SharedTextureFwd.h"
#include "Transform.h"
#include "VideoDisplayConstants.h"

#include <memory>
#include <optional>
#include <queue>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

class VideoFrameDistortionView;
using VideoFrameDistortionViewPtr= std::shared_ptr<VideoFrameDistortionView>;

class CompositorDefinition : public MikanComponentDefinition
{
public:
	CompositorDefinition();
	CompositorDefinition(MikanCompositorID compositorId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	MikanCompositorID getCompositorId() const { return getComponentId(); }

	static const std::string k_cameraIdPropertyId;
	inline MikanCameraID getCameraId() const { return m_cameraId; }
	void setCameraId(MikanCameraID cameraId);

	static const std::string k_ownerScenePropertyId;
	inline MikanSceneID getOwnerSceneId() const { return m_ownerSceneId; }
	void setOwnerSceneId(MikanSceneID sceneId);

	static const std::string k_compositorGraphPathPropertyId;
	bool hasCompositorGraphPath() const;
	std::filesystem::path getCompositorGraphPath() const;
	void setCompositorGraphPath(const std::filesystem::path& graphPath);

	static const std::string k_spoutEnableOutputNamePropertyId;
	inline bool getIsSpoutOutputStreaming() const { return m_bIsSpoutOutputStreaming; }
	void setIsSpoutOutputStreaming(bool bIsStreaming);

	static const std::string k_spoutOutputNamePropertyId;
	inline const std::string& getSpoutOutputName() const { return m_spoutOutputName; }
	void setSpoutOutputName(const std::string& spoutOutputName);

private:
	MikanSceneID m_ownerSceneId= INVALID_MIKAN_ID;
	MikanCameraID m_cameraId= INVALID_MIKAN_ID;
	AssetReferenceConfigPtr m_nodeGraphAssetRef;
	bool m_bIsSpoutOutputStreaming= false;
	std::string m_spoutOutputName;
};

class CompositorComponent : public MikanComponent
{
public:
	CompositorComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void postInit() override;
	virtual void dispose() override;
	virtual void update(float deltaSeconds) override;
	void renderToViewportQuad() const;

	bool start();
	bool getIsRunning() const { return m_bIsRunning; }
	void stop();

	// A graph editor window holds its compositor running while it is open, so
	// the graph gets frames and evaluation whichever compositor the scene displays
	bool getIsEditorHeld() const { return m_bEditorHeld; }
	void setEditorHeld(bool bHeld);
	// Whether the compositor may publish its output; the owning system grants it
	// only to the scene's display compositor
	void setOutputStreamingAllowed(bool bAllowed) { m_bOutputStreamingAllowed= bAllowed; }

	inline static const std::string k_componentClassName= "CompositorComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	CompositorObjectSystemPtr getOwnerObjectSystem() const;
	MikanStageID getOwnerStageId() const;
	StageComponentPtr getOwnerStageComponent() const;
	VideoSourceComponentPtr getVideoSourceComponent() const;

	CameraComponentPtr getCameraComponent() const;
	void setCameraComponent(CameraComponentPtr cameraComponent);

	std::filesystem::path getCompositorGraphAssetPath() const;
	void setCompositorGraphAssetPath(const std::filesystem::path& assetRefPath);

	inline CompositorDefinitionPtr getCompositorDefinition() const
	{
		return std::static_pointer_cast<CompositorDefinition>(m_definition);
	}
	inline MikanCompositorID getCompositorId() const { return getCompositorDefinition()->getCompositorId(); }

	IMkTexturePtr getVideoSourceTexture(eVideoTextureSource textureSource) const;
	IMkTexturePtr getVideoPreviewTexture(eVideoTextureSource textureSource) const;
	void setEditorCompositorNodeGraph(CompositorNodeGraphPtr editorNodeGraph);
	// Editor pause: skip graph evaluation while frames keep draining, freezing
	// the composite. Runtime-only, cleared when the editor graph unbinds.
	void setEditorEvaluationPaused(bool bPaused) { m_bEditorEvaluationPaused= bPaused; }
	bool getEditorEvaluationPaused() const { return m_bEditorEvaluationPaused; }
	const std::vector<NodeEvaluationError>& getLastNodeEvalErrors() const { return m_lastNodeEvalErrors; }
	IMkTextureConstPtr getCompositedFrameTexture() const;
	IMkTexturePtr getCompositedFrameTextureMutable();
	int64_t getPendingCompositedFrameIndex() const;
	inline int64_t getLastCompositedFrameIndex() const { return m_lastCompositedFrameIndex; }

	bool hasValidCompositorGraph() const;
	void addNewCompositorGraph();
	void editCompositorGraph();
	void removeCompositorGraph();

	MulticastDelegate<void()> OnNewFrameComposited;

	// -- IEntityAccessor ----
	virtual rfk::Struct const* getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_addNewCompositorGraphFunctionId;
	static const std::string k_editCompositorGraphFunctionId;
	static const std::string k_removeCompositorGraphFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void onDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

	// Video Source
	void updateVideoSourceStreaming();

	// Spout Output
	void updateOutputStreaming();
	bool startOutputStreaming();
	bool getIsOutputStreaming() const;
	void stopOutputStreaming();

	void handleCompositorNodeGraphChanged(const std::filesystem::path& newAssetRefPath);
	void stopVideoSourceStreaming();
	void startVideoSourceStreaming(VideoSourceComponentPtr videoSource);
	void tryCompositeOldestFrame(float deltaSeconds);
	void tryEnqueueNewFrame(CameraComponentPtr cameraComponent);
	void evaluateCompositorNodeGraph(CompositorNodeGraphPtr nodeGraph);

	// Client notification. A video-frame synced camera gets a frame event per video
	// frame; a free-running camera gets a properties event only when the camera
	// state changes, or when forced (compositor start, mode change). A client that
	// joins between changes pulls the state with GetCameraProperties instead.
	void publishClientFrameNotification(CameraComponentPtr cameraComponent,
										const MikanCameraNewFrameEvent& newFrameEvent);
	void publishCameraPropertiesIfChanged(CameraComponentPtr cameraComponent, bool bForce);
	void publishCompositorLifecycleEvent(bool bStarted);

private:
	// Compositor Rendering
	IMkTriangulatedMeshPtr m_viewportQuadMesh;

	// Pending queue of camera poses awaiting client render
	std::queue<MikanCameraNewFrameEvent> m_frameEventQueue;

	// Compositor Node Graph
	CompositorGraphAssetReferencePtr m_nodeGraphAssetRef;
	CompositorNodeGraphPtr m_nodeGraph;
	CompositorNodeGraphWeakPtr m_editorNodeGraph;
	bool m_bEditorEvaluationPaused= false;

	// Errors that occurred during the last graph evaluation
	std::vector<NodeEvaluationError> m_lastNodeEvalErrors;

	// Undistorted Video Frame Buffer
	VideoSourceComponentWeakPtr m_videoDistortionSource;
	VideoFrameDistortionViewPtr m_videoDistortionView;

	// Output Spout Sender
	ISharedTextureWriteAccessorPtr m_renderTargetWriteAccessor;

	bool m_bIsRunning= false;
	bool m_bEditorHeld= false;
	bool m_bOutputStreamingAllowed= true;

	// Free-running client notification state
	std::optional<MikanCameraNewPropertiesEvent> m_lastPublishedCameraProperties;
	MikanCameraFrameSyncMode m_lastFrameSyncMode= MikanCameraFrameSyncMode_Auto;
	int64_t m_lastReadVideoFrameIndex= 0;
	int64_t m_lastCompositedFrameIndex= 0;
	float m_timeSinceLastFrameComposited= 0.f;
};
