#include "App.h"
#include "CameraComponent.h"
#include "CameraMath.h"
#include "CameraObjectSystem.h"
#include "CameraRequestHandler.h"
#include "ClientSourceManager.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "PathUtils.h"
#include "IMkState.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "IMkGraphicsContext.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MathTypeConversion.h"
#include "MikanCompositorTypes.h"
#include "MikanObject.h"
#include "MikanServer.h"
#include "MkMaterialInstance.h"
#include "MkScopedState.h"
#include "MkStateStack.h"
#include "CompositorGraphAssetReference.h"
#include "ProjectConfig.h"
#include "ProjectConfigConstants.h"
#include "SceneObjectSystem.h"
#include "SharedTextureWriter.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"
#include "TransformComponent.h"
#include "StringUtils.h"
#include "VideoFrameDistortionView.h"
#include "VideoSourceComponent.h"
#include "Windows/CompositorNodeEditorWindow.h"
#include "IMkWindowContext.h"

#include "AssetReferencePropertyMetaData.h"
#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <assert.h>
#include <easy/profiler.h>

// -- CompositorConfig -----
const std::string CompositorDefinition::k_compositorGraphPathPropertyId= "compositor_graph_path";
const std::string CompositorDefinition::k_cameraIdPropertyId= "camera_id";
const std::string CompositorDefinition::k_ownerScenePropertyId= "owner_scene_id";
const std::string CompositorDefinition::k_spoutEnableOutputNamePropertyId= "spout_enable_output";
const std::string CompositorDefinition::k_spoutOutputNamePropertyId= "spout_output_name";

CompositorDefinition::CompositorDefinition()
	: MikanComponentDefinition()
	, m_nodeGraphAssetRef(std::make_shared<AssetReferenceConfig>())
	, m_bIsSpoutOutputStreaming(false)
	, m_spoutOutputName(DEFAULT_SPOUT_OUTPUT_NAME)
{
}

CompositorDefinition::CompositorDefinition(MikanCompositorID compositorId)
	: MikanComponentDefinition(compositorId, "")
	, m_ownerSceneId(INVALID_MIKAN_ID)
	, m_nodeGraphAssetRef(std::make_shared<AssetReferenceConfig>())
	, m_bIsSpoutOutputStreaming(false)
	, m_spoutOutputName(DEFAULT_SPOUT_OUTPUT_NAME)
{
}

configuru::Config CompositorDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_cameraIdPropertyId]= m_cameraId;
	pt[k_ownerScenePropertyId]= m_ownerSceneId;
	pt[k_spoutEnableOutputNamePropertyId]= m_bIsSpoutOutputStreaming;
	pt[k_spoutOutputNamePropertyId]= m_spoutOutputName;

	if (m_nodeGraphAssetRef)
	{
		pt[k_compositorGraphPathPropertyId]= m_nodeGraphAssetRef->writeToJSON();
	}

	return pt;
}

void CompositorDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_cameraId= pt.get_or<int>(k_cameraIdPropertyId, INVALID_MIKAN_ID);
	m_ownerSceneId= pt.get_or<int>(k_ownerScenePropertyId, INVALID_MIKAN_ID);
	m_bIsSpoutOutputStreaming= pt.get_or<bool>(k_spoutEnableOutputNamePropertyId, m_bIsSpoutOutputStreaming);
	m_spoutOutputName= pt.get_or<std::string>(k_spoutOutputNamePropertyId, m_spoutOutputName);
	if (m_spoutOutputName.empty())
		m_spoutOutputName= DEFAULT_SPOUT_OUTPUT_NAME;

	m_nodeGraphAssetRef= CompositorGraphAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_compositorGraphPathPropertyId))
	{
		m_nodeGraphAssetRef->readFromJSON(pt[k_compositorGraphPathPropertyId]);
	}
}

bool CompositorDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanCompositorComponentValues>();
	if (componentValues)
	{
		m_ownerSceneId= componentValues->owner_scene_id;
		m_cameraId= componentValues->camera_id;

		const std::string graphPathString= componentValues->compositor_graph_path.getUtf8Value();
		if (!graphPathString.empty())
		{
			setCompositorGraphPath(std::filesystem::path(graphPathString));
		}

		m_bIsSpoutOutputStreaming= componentValues->spout_enable_output;
		m_spoutOutputName= componentValues->spout_output_name.getUtf8Value();
	}

	if (componentValues->owner_scene_id == INVALID_MIKAN_ID)
	{
		auto sceneObjectSystem= ownerObjectSystem->getObjectSystemOfType<SceneObjectSystem>();

		m_ownerSceneId= sceneObjectSystem->getFirstComponentId();
	}

	return true;
}

void CompositorDefinition::setCameraId(MikanCameraID cameraId)
{
	if (m_cameraId != cameraId)
	{
		m_cameraId= cameraId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_cameraIdPropertyId));
	}
}

void CompositorDefinition::setOwnerSceneId(MikanSceneID stageId)
{
	if (m_ownerSceneId != stageId)
	{
		m_ownerSceneId= stageId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_ownerScenePropertyId));
	}
}

bool CompositorDefinition::hasCompositorGraphPath() const { return !m_nodeGraphAssetRef->assetPath.empty(); }

std::filesystem::path CompositorDefinition::getCompositorGraphPath() const { return m_nodeGraphAssetRef->assetPath; }

void CompositorDefinition::setCompositorGraphPath(const std::filesystem::path& graphPath)
{
	const std::string stored= graphPath.empty() ? std::string() : PathUtils::makeStoredProjectPath(graphPath);

	if (stored != m_nodeGraphAssetRef->assetPath)
	{
		m_nodeGraphAssetRef->assetPath= stored;
		notifyPropertyChanged(
			ConfigPropertyChangeSet().addPropertyName(CompositorDefinition::k_compositorGraphPathPropertyId));
	}
}

void CompositorDefinition::setIsSpoutOutputStreaming(bool bIsStreaming)
{
	if (m_bIsSpoutOutputStreaming != bIsStreaming)
	{
		m_bIsSpoutOutputStreaming= bIsStreaming;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spoutEnableOutputNamePropertyId));
	}
}

void CompositorDefinition::setSpoutOutputName(const std::string& spoutOutputName)
{
	if (m_spoutOutputName != spoutOutputName)
	{
		m_spoutOutputName= spoutOutputName;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_spoutOutputNamePropertyId));
	}
}

// -- CompositorComponent -----
CompositorComponent::CompositorComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
	m_bWantsUpdate= true;
}

// -- IEntityAccessor ----
rfk::Struct const* CompositorComponent::getClientAPIValuesStructType() const
{
	return &MikanCompositorComponentValues::staticGetArchetype();
}

void CompositorComponent::init()
{
	MikanComponent::init();

	m_viewportQuadMesh= createFullscreenQuadMesh(getGraphicsContext(), false);
	m_nodeGraphAssetRef= std::static_pointer_cast<CompositorGraphAssetReference>(
		CompositorGraphAssetReferenceFactory().allocateAssetReference());

	// Listen for changes to the compositor definition
	getCompositorDefinition()->OnPropertyChanged+= MakeDelegate(this, &CompositorComponent::onDefinitionChanged);
}

void CompositorComponent::postInit()
{
	MikanComponent::postInit();

	// Initialize the compositor graph if we have one assigned
	handleCompositorNodeGraphChanged(getCompositorDefinition()->getCompositorGraphPath());
}

void CompositorComponent::dispose()
{
	// Unsubscribes the distortion view from its video source before the view is
	// freed, since the source's receive thread writes into it by raw pointer
	stop();

	getCompositorDefinition()->OnPropertyChanged-= MakeDelegate(this, &CompositorComponent::onDefinitionChanged);

	m_nodeGraph= nullptr;
	m_nodeGraphAssetRef= nullptr;
	m_viewportQuadMesh= nullptr;

	MikanComponent::dispose();
}

void CompositorComponent::tryCompositeOldestFrame(float deltaSeconds)
{
	EASY_FUNCTION();

	// Wait until the source video frame queue is full to maximize the time
	// clients have to generate their render targets for each frame before we composite it.
	if (m_frameEventQueue.size() < m_videoDistortionView->getMaxFrameQueueSize())
		return;

	// Once the queue is full, we can composite at the source video frame rate,
	// so we track the time since the last frame was composited to enforce that
	m_timeSinceLastFrameComposited+= deltaSeconds;

	// Wait until a new video frame is available to keep the compositor display rate
	// in sync with the source video frame rate
	if (!m_videoDistortionView->hasNewVideoFrame())
		return;

	// The oldest pending frame is at the front of the queue
	MikanCameraNewFrameEvent& oldestPendingFrame= m_frameEventQueue.front();

	MIKAN_LOG_TRACE("CompositorComponent::tryCompositeOldestFrame") << "Compositing frame " << oldestPendingFrame.frame;

	// Try to get the editor node graph first...
	CompositorNodeGraphPtr nodeGraph= m_editorNodeGraph.lock();
	if (!nodeGraph)
	{
		// ...then fallback to asset reference (default case)
		nodeGraph= m_nodeGraph;
	}

	// If we have a valid compositor node graph, use that to composite the frame.
	// The editor pause skips evaluation but keeps the frame queue draining,
	// freezing the output at the last composite.
	if (nodeGraph && !m_bEditorEvaluationPaused)
	{
		evaluateCompositorNodeGraph(nodeGraph);
	}

	// Remember the index of the last frame we composited
	m_lastCompositedFrameIndex= oldestPendingFrame.frame;

	// Reset the time since the last frame was composited
	m_timeSinceLastFrameComposited= 0.f;

	// Tell any listeners that a new frame was composited
	if (OnNewFrameComposited)
	{
		OnNewFrameComposited();
	}

	// Pop the frame event from the queue now that it's composited
	m_frameEventQueue.pop();
}

void CompositorComponent::tryEnqueueNewFrame(CameraComponentPtr cameraComponent)
{
	EASY_FUNCTION();

	// Read the next video frame (if any) and apply distortion correction.
	// Returns the index of the latest video frame processed.
	int64_t nextVideoFrameIndex= m_videoDistortionView->readAndProcessVideoFrame();

	// Fetch new video frames if the video frame queue isn't full
	if (m_lastReadVideoFrameIndex != nextVideoFrameIndex)
	{
		// Remember the index of the last video frame we read
		m_lastReadVideoFrameIndex= nextVideoFrameIndex;

		// tryCompositeOldestFrame should have prevented this,
		// but just in case, if the queue is full, drop frames until we have room for the new one
		int droppedFrameCount= 0;
		while (m_frameEventQueue.size() >= m_videoDistortionView->getMaxFrameQueueSize())
		{
			// Drop the oldest frame in the queue to make room for the new one
			m_frameEventQueue.pop();
			droppedFrameCount++;
		}
		if (droppedFrameCount > 0)
		{
			MIKAN_LOG_ERROR("CompositorComponent::tryEnqueueNewFrame")
				<< "Frame queue overflow. Dropped " << droppedFrameCount << " frames";
		}

		// Try and make a new frame event with the current camera properties.
		// If we fail (e.g. due to invalid intrinsics), skip this frame and try again with the next one
		if (MikanCameraNewFrameEvent newFrameEvent; cameraComponent->makeNewCameraFrameEvent(
				m_lastReadVideoFrameIndex, 0, 0, // no fallback render target size in this case
				newFrameEvent))
		{
			MIKAN_LOG_TRACE("CompositorComponent::tryEnqueueNewFrame") << "Enqueue frame " << newFrameEvent.frame;
			m_frameEventQueue.push(newFrameEvent);

			// Tell clients what to render, in the form the camera's sync mode wants
			publishClientFrameNotification(cameraComponent, newFrameEvent);
		}
		else
		{
			MIKAN_LOG_TRACE("CompositorComponent::tryEnqueueNewFrame")
				<< "Invalid intrinsics for frame " << m_lastReadVideoFrameIndex << ". Skipping frame.";
		}
	}
}

void CompositorComponent::update(float deltaSeconds)
{
	EASY_FUNCTION();

	if (!getIsRunning())
		return;

	if (getOwnerObjectSystem()->getAllCompositorsPaused())
		return;

	CameraComponentPtr cameraComponent= getCameraComponent();
	if (!cameraComponent)
		return;

	// Update the video source streaming state in case settings changed
	updateVideoSourceStreaming();

	// Update the output streaming state in case it changed while we were stopped
	updateOutputStreaming();

	// Wait until we have a valid distortion view with a known frame size
	if (m_videoDistortionView)
	{
		// Composite the next frame if we got all the renders back from the clients
		tryCompositeOldestFrame(deltaSeconds);

		// Try to add new frames from the video source if we have room in the queue
		tryEnqueueNewFrame(cameraComponent);
	}
}

IMkTexturePtr CompositorComponent::getVideoSourceTexture(eVideoTextureSource textureSource) const
{
	const int64 pendingFrameIndex= getPendingCompositedFrameIndex();

	switch (textureSource)
	{
	case eVideoTextureSource::video_texture:
		return (m_videoDistortionView != nullptr) ? m_videoDistortionView->getVideoTexture(pendingFrameIndex)
												  : IMkTexturePtr();
	case eVideoTextureSource::distortion_texture:
		return (m_videoDistortionView != nullptr) ? m_videoDistortionView->getDistortionTexture() : IMkTexturePtr();
	}

	return IMkTexturePtr();
}

IMkTexturePtr CompositorComponent::getVideoPreviewTexture(eVideoTextureSource textureSource) const
{
	// For now, just return the same texture as the video source
	return getVideoSourceTexture(textureSource);
}

void CompositorComponent::setEditorCompositorNodeGraph(CompositorNodeGraphPtr editorNodeGraph)
{
	m_editorNodeGraph= editorNodeGraph;

	// Closing the editor always resumes evaluation
	if (editorNodeGraph == nullptr)
	{
		m_bEditorEvaluationPaused= false;
	}
}

IMkTextureConstPtr CompositorComponent::getCompositedFrameTexture() const
{
	// Prefer the editor graph, mirroring the evaluation fallback in
	// tryCompositeOldestFrame: whichever graph composites is the one shown
	CompositorNodeGraphPtr editorNodeGraph= m_editorNodeGraph.lock();
	if (editorNodeGraph)
	{
		return editorNodeGraph->getCompositedFrameTexture();
	}

	return m_nodeGraph ? m_nodeGraph->getCompositedFrameTexture() : IMkTextureConstPtr();
}

IMkTexturePtr CompositorComponent::getCompositedFrameTextureMutable()
{
	return std::const_pointer_cast<IMkTexture>(getCompositedFrameTexture());
}

int64_t CompositorComponent::getPendingCompositedFrameIndex() const
{
	return !m_frameEventQueue.empty() ? m_frameEventQueue.front().frame : -1;
}

bool CompositorComponent::hasValidCompositorGraph() const { return m_nodeGraph != nullptr; }

void CompositorComponent::addNewCompositorGraph()
{
	removeCompositorGraph();
	editCompositorGraph();
}

void CompositorComponent::editCompositorGraph()
{
	App* app= App::getInstance();

	// One editor window serves every compositor: an open one rebinds and comes forward
	CompositorNodeEditorWindow* window= app->getWindowOfType<CompositorNodeEditorWindow>();
	if (window == nullptr)
	{
		app->createAppWindow<CompositorNodeEditorWindow>()->bindCompositorComponent(getSelfPtr<CompositorComponent>());
	}
	else
	{
		window->openCompositorComponent(getSelfPtr<CompositorComponent>());
		window->getMkWindowContext()->raiseWindow();
	}
}

void CompositorComponent::setEditorHeld(bool bHeld)
{
	if (m_bEditorHeld != bHeld)
	{
		m_bEditorHeld= bHeld;

		// A window closing during app shutdown may outlive the project's systems
		if (CompositorObjectSystemPtr ownerSystem= getOwnerObjectSystem())
		{
			ownerSystem->refreshRunningCompositors();
		}
	}
}

void CompositorComponent::removeCompositorGraph() { setCompositorGraphAssetPath(std::filesystem::path()); }

void CompositorComponent::evaluateCompositorNodeGraph(CompositorNodeGraphPtr nodeGraph)
{
	NodeEvaluator evaluator= {};
	evaluator.setCurrentGraphicsContext(getGraphicsContext()).setDeltaSeconds(m_timeSinceLastFrameComposited);

	if (nodeGraph->compositeFrame(evaluator))
	{
		// Publish the composited frame to Spout if streaming is enabled
		if (getIsOutputStreaming())
		{
			IMkTexturePtr frameTexture= getCompositedFrameTextureMutable();

			if (frameTexture != nullptr && m_renderTargetWriteAccessor->getIsInitialized())
			{
				// Get the raw pointer to the underlying graphics API specific texture resource
				void* platformTexturePtr= frameTexture->getPlatformTexture();

				m_renderTargetWriteAccessor->writeColorFrameTexture(platformTexturePtr);
			}
		}

		m_lastNodeEvalErrors.clear();
	}
	else
	{
		m_lastNodeEvalErrors= evaluator.getErrors();
	}
}

void CompositorComponent::renderToViewportQuad() const
{
	IMkTextureConstPtr compositedFrameTexture= getCompositedFrameTexture();
	if (compositedFrameTexture)
	{
		MkMaterialInstancePtr materialInstance= m_viewportQuadMesh->getMaterialInstance();
		MkMaterialConstPtr material= materialInstance->getMaterial();

		if (auto materialBinding= material->bindMaterial())
		{
			// Bind the color texture
			materialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, compositedFrameTexture);

			// Draw the color texture
			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				MkStateStack& stateStack= getGraphicsContext()->getMkStateStack();
				MkScopedState scopedState= stateStack.createScopedState("CompositorComponentRender");
				scopedState.getStackState()->disableFlag(eMkStateFlagType::depthTest);

				m_viewportQuadMesh->drawElements();
			}
		}
	}
}

void CompositorComponent::updateVideoSourceStreaming()
{
	if (m_bIsRunning)
	{
		VideoSourceComponentPtr currentDistortionSource= m_videoDistortionSource.lock();
		VideoSourceComponentPtr desiredDistortionSource= getVideoSourceComponent();

		if (desiredDistortionSource != currentDistortionSource)
		{
			startVideoSourceStreaming(desiredDistortionSource);
		}
	}
	else
	{
		stopVideoSourceStreaming();
	}
}

void CompositorComponent::stopVideoSourceStreaming()
{
	if (m_videoDistortionView)
	{
		CameraComponentPtr cameraComponent= getCameraComponent();
		if (cameraComponent)
		{
			VideoSourceComponentPtr videoSource= cameraComponent->getVideoSourceComponent();

			if (videoSource)
			{
				videoSource->stopVideoStream(m_videoDistortionView.get());
			}
		}

		m_videoDistortionSource.reset();
		m_videoDistortionView= nullptr;
	}
}

void CompositorComponent::startVideoSourceStreaming(VideoSourceComponentPtr videoSource)
{
	assert(getIsRunning());

	// Clean up any pre-existing buffers
	// Stop any existing streaming on existing video distortion source
	stopVideoSourceStreaming();

	// Remember which video distortion source we are creating the view for
	m_videoDistortionSource= videoSource;

	// Create a distortion view to read the incoming video frames into a texture
	// (the view resizes itself when a frame of a new size arrives)
	m_videoDistortionView=
		std::make_shared<VideoFrameDistortionView>(videoSource, eVideoFrameProcessorMode::COMPOSITOR,
												   videoSource->getVideoSourceDefinition()->getVideoFrameQueueSize());

	// Always use the undistorted video frame for compositing
	m_videoDistortionView->setVideoDisplayMode(eVideoDisplayMode::mode_undistored);

	// Register the view as a stream consumer — VideoSourceComponent::update() drives the retry loop
	videoSource->startVideoStream(m_videoDistortionView.get());
}

void CompositorComponent::updateOutputStreaming()
{
	CompositorDefinitionConstPtr definition= getCompositorDefinition();
	const bool bWantsOutput= m_bOutputStreamingAllowed && definition->getIsSpoutOutputStreaming()
							 && !definition->getSpoutOutputName().empty();
	const bool bIsStreaming= getIsOutputStreaming();

	// Create the Spout sender if we don't have one already but want to stream
	if (bWantsOutput && !bIsStreaming)
	{
		startOutputStreaming();
	}
	// Stop streaming if we are currently streaming but don't want to anymore
	else if (!bWantsOutput && bIsStreaming)
	{
		stopOutputStreaming();
	}
}

bool CompositorComponent::startOutputStreaming()
{
	// If we are already streaming, do nothing
	if (getIsOutputStreaming())
		return true;

	// Make sure we have a valid texture to stream
	const std::string& spoutOutputName= getCompositorDefinition()->getSpoutOutputName();
	if (spoutOutputName.empty())
		return false;

	IMkTextureConstPtr compositorTexture= getCompositedFrameTexture();
	if (compositorTexture == nullptr)
		return false;

	// Compositing buffer should always be RGBA 32BPP
	// Spout can only support RGBA32 and BGRA32
	assert(compositorTexture->getBufferFormat() == MK_RGBA);

	SharedTextureDescriptor sharedTextureDescriptor;
	sharedTextureDescriptor.color_buffer_type= SharedColorBufferType::RGBA32;
	sharedTextureDescriptor.depth_buffer_type= SharedDepthBufferType::NODEPTH;
	sharedTextureDescriptor.width= compositorTexture->getTextureWidth();
	sharedTextureDescriptor.height= compositorTexture->getTextureHeight();
	sharedTextureDescriptor.graphicsAPI= SharedClientGraphicsApi::OpenGL;

	// Setup render target write accessor
	m_renderTargetWriteAccessor= createSharedTextureWriteAccessor(spoutOutputName, INVALID_MIKAN_ID);
	m_renderTargetWriteAccessor->initialize(&sharedTextureDescriptor, true, nullptr);

	return true;
}

bool CompositorComponent::getIsOutputStreaming() const
{
	return m_renderTargetWriteAccessor && m_renderTargetWriteAccessor->getIsInitialized();
}

void CompositorComponent::stopOutputStreaming()
{
	if (m_renderTargetWriteAccessor)
	{
		m_renderTargetWriteAccessor->dispose();
		m_renderTargetWriteAccessor= nullptr;
	}
}

bool CompositorComponent::start()
{
	if (!getIsRunning())
	{
		m_bIsRunning= true;
		m_timeSinceLastFrameComposited= 0.f;

		updateVideoSourceStreaming();

		// A free-running client needs the camera state before its first frame
		// event would have carried it, and needs to know a compositor is consuming
		m_lastPublishedCameraProperties.reset();
		publishCompositorLifecycleEvent(true);
		if (CameraComponentPtr cameraComponent= getCameraComponent())
		{
			m_lastFrameSyncMode= cameraComponent->getEffectiveFrameSyncMode();
			if (m_lastFrameSyncMode == MikanCameraFrameSyncMode_FreeRunning)
				publishCameraPropertiesIfChanged(cameraComponent, true);
		}
	}

	return true;
}

void CompositorComponent::stop()
{
	const bool bWasRunning= m_bIsRunning;

	m_bIsRunning= false;

	stopVideoSourceStreaming();
	stopOutputStreaming();

	m_lastPublishedCameraProperties.reset();
	if (bWasRunning)
		publishCompositorLifecycleEvent(false);
}

// -- Client notification ----
static bool cameraPropertiesEqual(const MikanCameraNewPropertiesEvent& a, const MikanCameraNewPropertiesEvent& b)
{
	auto equal3f= [](const MikanVector3f& u, const MikanVector3f& v) { return u.x == v.x && u.y == v.y && u.z == v.z; };
	auto equal2i= [](const MikanVector2i& u, const MikanVector2i& v) { return u.x == v.x && u.y == v.y; };
	auto equal2d= [](const MikanVector2d& u, const MikanVector2d& v) { return u.x == v.x && u.y == v.y; };

	return a.camera_id == b.camera_id && equal3f(a.camera_forward, b.camera_forward)
		   && equal3f(a.camera_up, b.camera_up) && equal3f(a.camera_position, b.camera_position)
		   && equal2i(a.pixel_size, b.pixel_size) && equal2d(a.focal_length, b.focal_length)
		   && equal2d(a.principal_point, b.principal_point) && equal2d(a.z_bounds, b.z_bounds);
}

void CompositorComponent::publishClientFrameNotification(CameraComponentPtr cameraComponent,
														 const MikanCameraNewFrameEvent& newFrameEvent)
{
	const MikanCameraFrameSyncMode syncMode= cameraComponent->getEffectiveFrameSyncMode();
	const bool bModeChanged= syncMode != m_lastFrameSyncMode;
	m_lastFrameSyncMode= syncMode;

	if (syncMode == MikanCameraFrameSyncMode_VideoFrame)
	{
		MikanServer* mikanServer= getOwnerEditorWindow()->getMikanServer();
		if (mikanServer)
		{
			// Tell all clients that we have a new frame to render
			mikanServer->getCameraRequestHandler()->publishCameraNewFrameEvent(newFrameEvent);
		}
	}
	else
	{
		// A client rendering on its own clock only needs to hear about changes.
		// Entering free running republishes so the client is not left on a state it
		// last saw in a frame event.
		publishCameraPropertiesIfChanged(cameraComponent, bModeChanged);
	}
}

void CompositorComponent::publishCameraPropertiesIfChanged(CameraComponentPtr cameraComponent, bool bForce)
{
	MikanServer* mikanServer= getOwnerEditorWindow()->getMikanServer();
	if (!mikanServer)
		return;

	MikanCameraNewPropertiesEvent propertiesEvent;
	if (!cameraComponent->makeCameraPropertiesEvent(0, 0, propertiesEvent))
		return;

	if (!bForce && m_lastPublishedCameraProperties.has_value()
		&& cameraPropertiesEqual(*m_lastPublishedCameraProperties, propertiesEvent))
	{
		return;
	}

	mikanServer->getCameraRequestHandler()->publishCameraNewPropertiesEvent(propertiesEvent);
	m_lastPublishedCameraProperties= propertiesEvent;
}

void CompositorComponent::publishCompositorLifecycleEvent(bool bStarted)
{
	IEditorWindow* editorWindow= getOwnerEditorWindow();
	MikanServer* mikanServer= editorWindow ? editorWindow->getMikanServer() : nullptr;
	if (!mikanServer)
		return;

	CameraRequestHandler* cameraRequestHandler= mikanServer->getCameraRequestHandler();
	const MikanCameraID cameraId= getCompositorDefinition()->getCameraId();
	if (bStarted)
		cameraRequestHandler->publishCompositorStartedEvent(getCompositorId(), cameraId);
	else
		cameraRequestHandler->publishCompositorStoppedEvent(getCompositorId(), cameraId);
}

CompositorObjectSystemPtr CompositorComponent::getOwnerObjectSystem() const
{
	return std::static_pointer_cast<CompositorObjectSystem>(getOwnerObject()->getOwnerSystem());
}

MikanStageID CompositorComponent::getOwnerStageId() const { return getCompositorDefinition()->getOwnerSceneId(); }

StageComponentPtr CompositorComponent::getOwnerStageComponent() const
{
	return getObjectSystemOfType<StageObjectSystem>()->getStageById(getOwnerStageId());
}

CameraComponentPtr CompositorComponent::getCameraComponent() const
{
	MikanCameraID cameraId= getCompositorDefinition()->getCameraId();

	return getObjectSystemOfType<CameraObjectSystem>()->getCameraById(cameraId);
}

VideoSourceComponentPtr CompositorComponent::getVideoSourceComponent() const
{
	CameraComponentPtr cameraComponent= getCameraComponent();
	if (cameraComponent)
	{
		return cameraComponent->getVideoSourceComponent();
	}

	return VideoSourceComponentPtr();
}

void CompositorComponent::setCameraComponent(CameraComponentPtr newCameraComponent)
{
	CameraComponentPtr oldCameraComponent= getCameraComponent();
	MikanCameraID oldCameraId= oldCameraComponent ? oldCameraComponent->getCameraId() : INVALID_MIKAN_ID;
	MikanCameraID newCameraId= newCameraComponent ? newCameraComponent->getCameraId() : INVALID_MIKAN_ID;

	if (newCameraId != oldCameraId)
	{
		// Update the camera ID in the compositor definition
		getCompositorDefinition()->setCameraId(newCameraId);

		// Update video streaming state for the new camera
		updateVideoSourceStreaming();
	}
}

std::filesystem::path CompositorComponent::getCompositorGraphAssetPath() const
{
	return m_nodeGraphAssetRef->getInternalAssetPath();
}

void CompositorComponent::setCompositorGraphAssetPath(const std::filesystem::path& assetRefPath)
{
	// A file of another graph kind is refused outright rather than stored and
	// failed on every load. A path with no file behind it yet is kept.
	const std::string graphClassName= NodeGraphFactory::peekGraphClassName(assetRefPath);
	if (!graphClassName.empty() && graphClassName != CompositorNodeGraph::k_graphClassName)
	{
		MIKAN_LOG_ERROR("CompositorComponent::setCompositorGraphAssetPath")
			<< assetRefPath << " is a " << graphClassName << ", not a compositor graph";
		return;
	}

	handleCompositorNodeGraphChanged(assetRefPath);
	getCompositorDefinition()->setCompositorGraphPath(assetRefPath);
}

void CompositorComponent::handleCompositorNodeGraphChanged(const std::filesystem::path& newAssetRefPath)
{
	m_nodeGraphAssetRef->setAssetPath(newAssetRefPath);

	if (!newAssetRefPath.empty())
	{
		m_nodeGraph= std::dynamic_pointer_cast<CompositorNodeGraph>(NodeGraphFactory::loadNodeGraph(
			getOwnerEditorWindow(), newAssetRefPath, CompositorNodeGraph::k_graphClassName));
		if (m_nodeGraph)
		{
			MIKAN_LOG_INFO("CompositorComponent::handleCompositorNodeGraphChanged")
				<< "Loaded compositor graph: " << newAssetRefPath;

			m_nodeGraph->bindToCompositorComponent(getSelfPtr<CompositorComponent>());
		}
		else
		{
			MIKAN_LOG_ERROR("CompositorComponent::handleCompositorNodeGraphChanged")
				<< "Failed to load compositor graph: " << newAssetRefPath;
		}
	}
	else
	{
		m_nodeGraph= nullptr;
	}

	// Clear any previous graph eval errors
	m_lastNodeEvalErrors.clear();
}

void CompositorComponent::onDefinitionChanged(CommonConfigPtr configPtr,
											  const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(CompositorDefinition::k_spoutEnableOutputNamePropertyId))
	{
		stopOutputStreaming();
		updateOutputStreaming();
	}

	if (changedPropertySet.hasPropertyName(CompositorDefinition::k_spoutOutputNamePropertyId))
	{
		updateOutputStreaming();
	}
}

// -- IPropertyInterface ----
void CompositorComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(CompositorDefinition::k_cameraIdPropertyId, MikanVariantType::INT)
			->setDefaultValue(-1)
			->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(CompositorDefinition::k_ownerScenePropertyId, MikanVariantType::INT)
			->setDefaultValue(-1)
			->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(CompositorDefinition::k_compositorGraphPathPropertyId,
																  MikanVariantType::STRING)
								 ->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
									 AssetReferenceFactory::createFactory<CompositorGraphAssetReferenceFactory>())));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		CompositorDefinition::k_spoutEnableOutputNamePropertyId, MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(CompositorDefinition::k_spoutOutputNamePropertyId,
																  MikanVariantType::STRING));
}

bool CompositorComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == CompositorDefinition::k_cameraIdPropertyId)
	{
		outValue= getCompositorDefinition()->getCameraId();
		return true;
	}
	else if (propertyName == CompositorDefinition::k_ownerScenePropertyId)
	{
		outValue= getCompositorDefinition()->getOwnerSceneId();
		return true;
	}
	else if (propertyName == CompositorDefinition::k_compositorGraphPathPropertyId)
	{
		outValue= getCompositorDefinition()->getCompositorGraphPath();
		return true;
	}
	else if (propertyName == CompositorDefinition::k_spoutEnableOutputNamePropertyId)
	{
		outValue= getCompositorDefinition()->getIsSpoutOutputStreaming();
		return true;
	}
	else if (propertyName == CompositorDefinition::k_spoutOutputNamePropertyId)
	{
		outValue= getCompositorDefinition()->getSpoutOutputName().c_str();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool CompositorComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == CompositorDefinition::k_cameraIdPropertyId)
	{
		const MikanCameraID cameraId= inValue.getIntValue();
		getCompositorDefinition()->setCameraId(cameraId);
		return true;
	}
	else if (propertyName == CompositorDefinition::k_ownerScenePropertyId)
	{
		const MikanSceneID sceneId= inValue.getIntValue();
		getCompositorDefinition()->setOwnerSceneId(sceneId);
		return true;
	}
	else if (propertyName == CompositorDefinition::k_compositorGraphPathPropertyId)
	{
		const std::string fileString= inValue.getUtf8Value();
		const std::filesystem::path filePath(fileString);

		// Route through the asset-path setter so the live node graph
		// rebuilds along with the definition
		setCompositorGraphAssetPath(filePath);
		return true;
	}
	else if (propertyName == CompositorDefinition::k_spoutEnableOutputNamePropertyId)
	{
		const bool bIsStreaming= inValue.getBoolValue();
		getCompositorDefinition()->setIsSpoutOutputStreaming(bIsStreaming);
		return true;
	}
	else if (propertyName == CompositorDefinition::k_spoutOutputNamePropertyId)
	{
		const std::string spoutOutputName= inValue.getUtf8Value();
		getCompositorDefinition()->setSpoutOutputName(spoutOutputName);
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string CompositorComponent::k_addNewCompositorGraphFunctionId= "add_new_compositor_graph";
const std::string CompositorComponent::k_editCompositorGraphFunctionId= "edit_compositor_graph";
const std::string CompositorComponent::k_removeCompositorGraphFunctionId= "remove_compositor_graph";

void CompositorComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_editCompositorGraphFunctionId, "Add Compositor Graph")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_editCompositorGraphFunctionId, "Edit Compositor Graph")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_removeCompositorGraphFunctionId, "Remove Compositor Graph")
			->setUIHidden());
}

bool CompositorComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == CompositorComponent::k_addNewCompositorGraphFunctionId)
	{
		addNewCompositorGraph();
		return true;
	}
	else if (functionName == CompositorComponent::k_editCompositorGraphFunctionId)
	{
		editCompositorGraph();
		return true;
	}
	else if (functionName == CompositorComponent::k_removeCompositorGraphFunctionId)
	{
		removeCompositorGraph();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}

// -- Lua Binding ----
void CompositorComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<CompositorComponent, MikanComponent>(CompositorComponent::k_componentClassName.c_str())
		.addProperty("compositorId", [](CompositorComponent* c) -> int { return c->getCompositorId(); })
		.addProperty("cameraId",
					 [](CompositorComponent* c) -> int { return c->getCompositorDefinition()->getCameraId(); })
		.addProperty("ownerSceneId",
					 [](CompositorComponent* c) -> int { return c->getCompositorDefinition()->getOwnerSceneId(); })
		.addProperty("ownerStageId", [](CompositorComponent* c) -> int { return c->getOwnerStageId(); })
		.addProperty(
			"isSpoutOutputStreaming",
			[](CompositorComponent* c) -> bool { return c->getCompositorDefinition()->getIsSpoutOutputStreaming(); },
			[](CompositorComponent* c, bool v) { c->getCompositorDefinition()->setIsSpoutOutputStreaming(v); })
		.addFunction("editCompositorGraph", [](CompositorComponent* c) { c->editCompositorGraph(); })
		.addFunction("getOwnerStage",
					 [](CompositorComponent* c) -> StageComponent* { return c->getOwnerStageComponent().get(); })
		.endClass();
}