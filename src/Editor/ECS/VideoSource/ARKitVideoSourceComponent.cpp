#include "ARKitVideoSourceComponent.h"
#include "ARKitVideoSourceSystem.h"
#include "CameraMath.h"
#include "IARKitVideoDeviceManager.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "Logger.h"
#include "MikanObject.h"
#include "MikanServer.h"
#include "MikanVideoSourceTypes.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkShaderConstants.h"
#include "MkStateStack.h"
#include "ProjectConfigConstants.h"
#include "ThreadUtils.h"
#include "VideoSourceRequestHandler.h"

#include "opencv2/opencv.hpp"

#include <algorithm>
#include <assert.h>

// -- ARKitVideoSourceDefinition -----
const std::string ARKitVideoSourceDefinition::k_basePortPropertyId= "base_port";
const std::string ARKitVideoSourceDefinition::k_poseOffsetPropertyId= "pose_offset";

ARKitVideoSourceDefinition::ARKitVideoSourceDefinition()
	: VideoSourceDefinition()
	, m_basePort(DEFAULT_ARKIT_BASE_PORT)
	, m_poseOffset(1.f)
{
}

ARKitVideoSourceDefinition::ARKitVideoSourceDefinition(MikanVideoSourceID videoSourceId)
	: VideoSourceDefinition(videoSourceId)
	, m_basePort(DEFAULT_ARKIT_BASE_PORT)
	, m_poseOffset(1.f)
{
}

configuru::Config ARKitVideoSourceDefinition::writeToJSON()
{
	configuru::Config pt= VideoSourceDefinition::writeToJSON();

	pt[k_basePortPropertyId]= m_basePort;

	// Written column-major, matching glm's own storage, and only when one has
	// been solved - an absent key reads back as "never aligned" rather than as
	// an identity offset that looks deliberate.
	if (m_bHasPoseOffset)
	{
		std::vector<configuru::Config> offsetValues;
		offsetValues.reserve(16);
		for (int column= 0; column < 4; ++column)
		{
			for (int row= 0; row < 4; ++row)
			{
				offsetValues.push_back(configuru::Config(m_poseOffset[column][row]));
			}
		}
		pt[k_poseOffsetPropertyId]= offsetValues;
	}

	return pt;
}

void ARKitVideoSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	VideoSourceDefinition::readFromJSON(pt);

	m_basePort= pt.get_or<int>(k_basePortPropertyId, m_basePort);

	m_bHasPoseOffset= false;
	m_poseOffset= glm::mat4(1.f);
	if (pt.has_key(k_poseOffsetPropertyId))
	{
		const configuru::Config& offsetValues= pt[k_poseOffsetPropertyId];
		if (offsetValues.is_array() && offsetValues.array_size() == 16)
		{
			for (int index= 0; index < 16; ++index)
			{
				m_poseOffset[index / 4][index % 4]= (float)offsetValues[index].as_float();
			}
			m_bHasPoseOffset= true;
		}
		else
		{
			MIKAN_LOG_WARNING("ARKitVideoSourceDefinition::readFromJSON")
				<< "Ignoring malformed " << k_poseOffsetPropertyId << " (expected 16 floats)";
		}
	}
}

void ARKitVideoSourceDefinition::setPoseOffset(const glm::mat4& poseOffset)
{
	m_poseOffset= poseOffset;
	m_bHasPoseOffset= true;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

void ARKitVideoSourceDefinition::clearPoseOffset()
{
	m_poseOffset= glm::mat4(1.f);
	m_bHasPoseOffset= false;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_poseOffsetPropertyId));
}

bool ARKitVideoSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!VideoSourceDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanARKitVideoSourceValues>();
	if (componentValues)
	{
		m_basePort= componentValues->base_port;
	}

	return true;
}

void ARKitVideoSourceDefinition::setBasePort(int basePort)
{
	if (basePort != m_basePort)
	{
		m_basePort= basePort;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_basePortPropertyId));
	}
}

// -- ARKitVideoSourceComponent -----
ARKitVideoSourceComponent::ARKitVideoSourceComponent(MikanObjectWeakPtr owner)
	: VideoSourceComponent(owner)
	, m_arkitVideoDevice(nullptr)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle ARKitVideoSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanARKitVideoSourceValues::staticGetArchetype();
}

void ARKitVideoSourceComponent::init()
{
	m_bWantsUpdate= true;
	MikanComponent::init();

	// Attempt to open the video source
	openVideoSource();
}

void ARKitVideoSourceComponent::dispose()
{
	// Close the video source if it is open
	closeVideoSource();

	// Call the base class dispose method
	MikanComponent::dispose();
}

void ARKitVideoSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	// Close any open video source that was open
	closeVideoSource();
}

std::string ARKitVideoSourceComponent::getDevicePath() const
{
	if (m_arkitVideoDevice != nullptr)
	{
		return m_arkitVideoDevice->getDevicePath();
	}

	return "";
}

std::string ARKitVideoSourceComponent::getDeviceAPI() const { return "ARKitVideoSource"; }

bool ARKitVideoSourceComponent::openVideoSource()
{
	// If the video source is already open, do nothing
	if (m_arkitVideoDevice != nullptr)
		return true;

	ARKitVideoSourceDefinitionPtr definition= getARKitVideoSourceDefinition();

	auto arkitVideoSourceSystem= std::static_pointer_cast<ARKitVideoSourceSystem>(getOwnerObject()->getOwnerSystem());
	IARKitVideoDeviceManagerPtr arkitVideoDeviceManager= arkitVideoSourceSystem->getARKitVideoDeviceManager();
	if (!arkitVideoDeviceManager)
	{
		// If the manager is still initializing, mark this component as pending so the
		// system will retry openVideoSource() once the manager is ready
		auto arkitVideoManagerState= arkitVideoSourceSystem->getARKitVideoManagerState();
		if (arkitVideoManagerState == ARKitVideoDeviceManagerLoader::eARKitVideoManagerState::uninitialized
			|| arkitVideoManagerState == ARKitVideoDeviceManagerLoader::eARKitVideoManagerState::initializing)
		{
			m_bPendingOpen= true;
			return true;
		}
		else
		{
			return false;
		}
	}
	m_bPendingOpen= false;

	ARKitVideoConnectionSettings connectionSettings;
	connectionSettings.basePort= static_cast<uint16_t>(definition->getBasePort());

	m_arkitVideoDevice= arkitVideoDeviceManager->createVideoDevice(connectionSettings);
	if (m_arkitVideoDevice == nullptr)
		return false;

	// Listen for events from the ARKit video device before calling open()
	// so that notifyDeviceOpened() is received when the async open completes
	m_arkitVideoDevice->addListener(this);

	// Kick off the async open — post-open setup happens in notifyDeviceOpened()
	if (m_arkitVideoDevice->open() == eVideoOpeningStatus::failed)
	{
		m_arkitVideoDevice->removeListener(this);
		m_arkitVideoDevice= nullptr;
		return false;
	}

	return true;
}

void ARKitVideoSourceComponent::closeVideoSource()
{
	m_bPendingOpen= false;

	if (m_arkitVideoDevice != nullptr)
	{
		// Remove the listener for the ARKit video device
		m_arkitVideoDevice->removeListener(this);

		// Stop the video stream if it is running
		forceStopVideoStream();

		// Close the ARKit video device
		m_arkitVideoDevice->close();

		// Tell the ARKit video device manager to destroy the device
		auto arkitVideoSourceSystem=
			std::static_pointer_cast<ARKitVideoSourceSystem>(getOwnerObject()->getOwnerSystem());
		IARKitVideoDeviceManagerPtr arkitVideoDeviceManager= arkitVideoSourceSystem->getARKitVideoDeviceManager();
		if (arkitVideoDeviceManager)
		{
			arkitVideoDeviceManager->destroyVideoDevice(m_arkitVideoDevice);
		}

		// Clear the ARKit video device pointer
		m_arkitVideoDevice= nullptr;
	}

	m_lastVideoWidth= 0;
	m_lastVideoHeight= 0;

	// Let any connected clients know that the video source closed
	MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceClosedEvent();
	if (OnClosed)
	{
		OnClosed(getSelfPtr<VideoSourceComponent>());
	}
}

eVideoStreamingStatus ARKitVideoSourceComponent::startVideoStreamInternal()
{
	if (m_arkitVideoDevice != nullptr)
	{
		eVideoStreamingStatus status= m_arkitVideoDevice->startVideoStream();
		if (OnStarted && (status == eVideoStreamingStatus::started || status == eVideoStreamingStatus::pendingStart))
		{
			OnStarted(getSelfPtr<VideoSourceComponent>());
		}

		return status;
	}

	return eVideoStreamingStatus::failed;
}

eVideoStreamingStatus ARKitVideoSourceComponent::getVideoStreamingStatus() const
{
	if (m_arkitVideoDevice != nullptr)
	{
		return m_arkitVideoDevice->getVideoStreamingStatus();
	}

	return eVideoStreamingStatus::failed;
}

void ARKitVideoSourceComponent::stopVideoStreamInternal()
{
	if (m_arkitVideoDevice != nullptr)
	{
		m_arkitVideoDevice->stopVideoStream();

		if (OnStopped)
		{
			OnStopped(getSelfPtr<VideoSourceComponent>());
		}
	}
}

bool ARKitVideoSourceComponent::getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const
{
	if (m_lastVideoWidth > 0 && m_lastVideoHeight > 0)
	{
		outPixelWidth= m_lastVideoWidth;
		outPixelHeight= m_lastVideoHeight;
		return true;
	}

	return false;
}

// -- Zero-copy GPU texture access (ticket E3; NV12->RGBA shader pass as of "Phase 6") ----
IMkTexturePtr ARKitVideoSourceComponent::getDirectColorTexture() const
{
	if (m_nv12ConversionFrameBuffer == nullptr)
		return IMkTexturePtr();

	// The conversion pass itself runs in processDirectVideoFrame() (called once
	// per tick by VideoFrameDistortionView::readAndProcessVideoFrame(), before
	// this is read) - this just returns whatever it last wrote.
	return m_nv12ConversionFrameBuffer->getColorTexture();
}

void ARKitVideoSourceComponent::processDirectVideoFrame()
{
	if (m_arkitVideoDevice == nullptr)
		return;

	uint32_t lumaGlId= m_arkitVideoDevice->getLumaTextureGlId();
	uint32_t chromaGlId= m_arkitVideoDevice->getChromaTextureGlId();
	if (lumaGlId == 0 || chromaGlId == 0)
		return; // no decoded frame yet this session

	// Skip the conversion when no new frame has arrived. Callers run this every
	// render tick, but frames only arrive at the source rate - a 30fps stream into
	// a ~97Hz render loop means roughly three of every four passes reconvert
	// pixels that have not changed, at full resolution. The output framebuffer
	// keeps the last converted frame, so a caller reading the texture later in the
	// same tick still sees valid content, which is what the unconditional call at
	// the call site was protecting.
	const int64_t currentFrameIndex= m_lastBundleFrameSeq.load();
	if (currentFrameIndex >= 0 && currentFrameIndex == m_lastConvertedFrameIndex)
		return;

	m_lastConvertedFrameIndex= currentFrameIndex;

	if (m_lumaTextureWrapper == nullptr)
		m_lumaTextureWrapper= CreateMkExternalTexture();
	m_lumaTextureWrapper->setExternalPlatformTexture(&lumaGlId);

	if (m_chromaTextureWrapper == nullptr)
		m_chromaTextureWrapper= CreateMkExternalTexture();
	m_chromaTextureWrapper->setExternalPlatformTexture(&chromaGlId);

	const int width= m_lumaTextureWrapper->getTextureWidth();
	const int height= m_lumaTextureWrapper->getTextureHeight();
	if (width <= 0 || height <= 0)
		return;

	IMkGraphicsContext* graphicsContext= getGraphicsContext();
	if (graphicsContext == nullptr)
		return;

	// Lazily create the conversion pipeline's fixed resources on first use -
	// mirrors GLVideoFrameProcessor's FBO + lazy-resource-creation pattern.
	// m_nv12ConversionFrameBuffer never has an external texture attached, so it
	// creates and owns its own RGBA color texture internally (see
	// IMkFrameBuffer::attachColorTexture's own doc comment on this default).
	if (m_nv12ConversionFrameBuffer == nullptr)
	{
		m_nv12ConversionFrameBuffer= createMkFrameBuffer("ARKit NV12 Conversion Frame Buffer");
		m_nv12ConversionFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
		m_nv12ConversionFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA);

		m_nv12ConversionMaterial=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_CONVERT_NV12_TO_RGBA);
		m_nv12ConversionMaterialInstance= createMkMaterialInstance(m_nv12ConversionMaterial);
		// Not v-flipped: this pass writes a video texture, it does not display one.
		// The NV12 planes arrive with image row 0 at v=0 (a straight cuMemcpy2D of
		// the decoded frame), which is the same convention copyOpenCVMatIntoGLTexture
		// produces for CPU sources, and the output has to keep it so that consumers
		// cannot tell which decode tier produced the frame. Flipping here would
		// invert relative to every other source, and the display quad's own flip
		// would then land the image upside down.
		m_nv12ConversionQuad= createFullscreenQuadMesh(graphicsContext, m_nv12ConversionMaterial, false);
	}

	if (width != m_nv12ConversionWidth || height != m_nv12ConversionHeight)
	{
		m_nv12ConversionFrameBuffer->setSize(width, height);
		m_nv12ConversionWidth= width;
		m_nv12ConversionHeight= height;
	}

	if (!m_nv12ConversionFrameBuffer->isValid())
	{
		if (!m_nv12ConversionFrameBuffer->createResources())
		{
			MIKAN_LOG_ERROR("ARKitVideoSourceComponent::processDirectVideoFrame")
				<< "Failed to create NV12 conversion framebuffer";
			return;
		}
	}

	// BT.601 (SD) vs BT.709 (HD) coefficient selection, resolution-based - see
	// getPTConvertNV12ToRGBAQuad()'s own comment for why this isn't driven from
	// real caps colorimetry (ARKit's decode pipeline doesn't currently surface
	// per-frame colorimetry metadata at this layer, unlike
	// MikanGStreamerVideoDevice::extractColorimetryInfo for Network sources).
	const float isHD= (width >= 1280) ? 1.0f : 0.0f;

	MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
											 "ARKitVideoSourceComponent NV12 Conversion Scope",
											 m_nv12ConversionFrameBuffer);
	if (frameBufferBinding)
	{
		if (auto materialBinding= m_nv12ConversionMaterial->bindMaterial())
		{
			m_nv12ConversionMaterialInstance->setTextureBySemantic(eUniformSemantic::lumaTexture, m_lumaTextureWrapper);
			m_nv12ConversionMaterialInstance->setTextureBySemantic(eUniformSemantic::chromaTexture,
																   m_chromaTextureWrapper);
			m_nv12ConversionMaterialInstance->setFloatBySemantic(eUniformSemantic::floatConstant0, isHD);

			if (auto materialInstanceBinding= m_nv12ConversionMaterialInstance->bindMaterialInstance(materialBinding))
			{
				m_nv12ConversionQuad->drawElements();
			}
		}
	}
}

// -- IARKitVideoDeviceListener ----
void ARKitVideoSourceComponent::notifyDeviceOpened(const IARKitVideoDevice* device)
{
	if (device != m_arkitVideoDevice.get())
		return;

	// Let any connected clients know that the video source opened
	MikanServer::getInstance()->getVideoSourceRequestHandler()->publishVideoSourceOpenedEvent();
	if (OnOpened)
	{
		OnOpened(getSelfPtr<VideoSourceComponent>());
	}
}

void ARKitVideoSourceComponent::notifyDeviceClosed(const IARKitVideoDevice* device)
{
	if (device == m_arkitVideoDevice.get())
	{
		// The video source is now invalidated, so we can clear the pointer
		// but we still want to clean up the video source state
		m_arkitVideoDevice= nullptr;
		closeVideoSource();
	}
}

void ARKitVideoSourceComponent::notifyFrameBundleReceived(const ARKitVideoFrameBundle& bundle)
{
	assert(m_arkitVideoDevice != nullptr);

	// Tracked unconditionally (independent of which fields this particular bundle
	// carries) so getDirectFrameIndex() reflects the latest frameSeq seen at all,
	// not just ones with a pose - see that method's declaration comment.
	m_lastBundleFrameSeq.store(static_cast<int64_t>(bundle.frameSeq));

	// As of ticket C4, the video pipeline hardware-decodes straight to CUDA device
	// memory (see MikanARKitVideoDevice::notifyFrameBundleReceived) - bundle.videoData
	// is always null today, so there's nothing to hand to writeVideoFrame() yet.
	// This guard is the natural, forward-compatible consumer of the documented
	// ARKitVideoFrameBundle contract ("videoData ... valid only during the
	// callback") rather than dead code: once a later ticket gives the ARKit pipeline
	// a CPU-readable (or CUDA-GL composited) preview path, this starts writing real
	// frames with no further change needed here.
	if (bundle.hasVideo && bundle.videoData != nullptr)
	{
		ARKitVideoSourceDefinitionPtr definition= getARKitVideoSourceDefinition();
		const bool is_frame_flipped= definition->getIsFrameMirrored();
		cv::Size bufferDimensions(bundle.videoWidth, bundle.videoHeight);

		m_lastVideoWidth= bundle.videoWidth;
		m_lastVideoHeight= bundle.videoHeight;

		writeVideoFrame(bundle.videoData, bufferDimensions, is_frame_flipped);
	}

	// Frame-coupled pose (ticket E4).
	if (bundle.hasPose)
	{
		ARKitVideoSourceDefinitionPtr definition= getARKitVideoSourceDefinition();

		// ARKitPoseFrameBuffer::transform is row-major camera-to-world
		// (IARKitVideoDevice.h) - glm::mat4's constructor/subscript operators are
		// column-major, so this must be transposed, not just memcpy'd. This exact
		// row/column-major mixup was flagged as the single most likely bug spot
		// back when the wire format was first designed (Track A5) - get it wrong
		// here and the camera pose will look like a scrambled/sheared transform,
		// not an obviously-wrong one.
		const float* t= bundle.pose.transform;
		const glm::mat4 cameraToWorld(t[0], t[4], t[8], t[12], t[1], t[5], t[9], t[13], t[2], t[6], t[10], t[14], t[3],
									  t[7], t[11], t[15]);

		// A frameSeq that runs backwards means the phone restarted its ARKit
		// session, and ARKit chooses a fresh world origin every session. Any
		// stored offset describes the previous world, so keeping it would place
		// the camera confidently in the wrong place rather than obviously
		// failing. Drop it and make the operator re-align.
		if (bundle.frameSeq < m_lastPoseFrameSeq && definition->hasPoseOffset())
		{
			MIKAN_LOG_WARNING("ARKitVideoSourceComponent::notifyFrameBundleReceived")
				<< "frameSeq went backwards (" << m_lastPoseFrameSeq << " -> " << bundle.frameSeq
				<< "), so the phone restarted its ARKit session. Clearing the marker alignment, which "
				<< "described the previous session's world origin - re-align to restore it.";
			definition->clearPoseOffset();
		}
		m_lastPoseFrameSeq= bundle.frameSeq;

		// Single point where an ARKit pose leaves its own world space. The offset
		// is solved once by marker alignment (AppStage_AlignCameraByOriginMarker)
		// and is identity until then, so an unaligned source streams raw ARKit
		// world poses exactly as before.
		const glm::mat4 cameraToStage=
			definition->hasPoseOffset() ? definition->getPoseOffset() * cameraToWorld : cameraToWorld;

		MikanVideoSourceIntrinsics intrinsics;
		MikanMonoIntrinsics monoIntrinsics;
		createDefautMonoIntrinsics(static_cast<int>(bundle.pose.imageWidth), static_cast<int>(bundle.pose.imageHeight),
								   monoIntrinsics);
		// Overwrite the synthetic fx/fy/cx/cy with ARKit's real, per-frame values -
		// znear/zfar/pixel_width/pixel_height from the synthetic defaults are kept
		// as-is (ARKit doesn't report clip planes, and pixel dimensions already
		// match since they were used to build the synthetic intrinsics above).
		// ARKit reports no distortion coefficients, so distorted == undistorted.
		//
		// MikanMatrix3d is column-major: x/y/z name the columns and the digit is
		// the row (see its declaration comment), so OpenCV's [0][2]/[1][2]
		// principal point lands in z0/z1, not x2/y2. Putting it in x2/y2 leaves
		// cx/cy reading as zero through MikanMatrix3d_to_cv_mat33d, which collapses
		// the undistortion map onto the image corner.
		monoIntrinsics.undistorted_camera_matrix.x0= bundle.pose.fx;
		monoIntrinsics.undistorted_camera_matrix.y1= bundle.pose.fy;
		monoIntrinsics.undistorted_camera_matrix.z0= bundle.pose.cx;
		monoIntrinsics.undistorted_camera_matrix.z1= bundle.pose.cy;
		monoIntrinsics.distorted_camera_matrix= monoIntrinsics.undistorted_camera_matrix;
		intrinsics.makeMonoIntrinsics()= monoIntrinsics;

		std::lock_guard<std::mutex> lock(m_latestPoseMutex);
		m_latestPose.transform= cameraToStage;
		m_latestPose.sourceWorldTransform= cameraToWorld;
		m_latestPose.intrinsics= intrinsics;
		m_latestPose.frameSeq= bundle.frameSeq;
		m_latestPose.bValid= true;
	}
}

int64_t ARKitVideoSourceComponent::getDirectFrameIndex() const { return m_lastBundleFrameSeq.load(); }

void ARKitVideoSourceComponent::setPoseOffset(const glm::mat4& worldToStageXform)
{
	getARKitVideoSourceDefinition()->setPoseOffset(worldToStageXform);
}

glm::mat4 ARKitVideoSourceComponent::getPoseOffset() const { return getARKitVideoSourceDefinition()->getPoseOffset(); }

bool ARKitVideoSourceComponent::hasPoseOffset() const { return getARKitVideoSourceDefinition()->hasPoseOffset(); }

bool ARKitVideoSourceComponent::getLatestFrameCoupledPose(glm::mat4& outTransform,
														  MikanVideoSourceIntrinsics& outIntrinsics,
														  uint32_t& outFrameSeq) const
{
	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	if (!m_latestPose.bValid)
		return false;

	outTransform= m_latestPose.transform;
	outIntrinsics= m_latestPose.intrinsics;
	outFrameSeq= m_latestPose.frameSeq;
	return true;
}

bool ARKitVideoSourceComponent::getLatestSourceWorldPose(glm::mat4& outTransform, uint32_t& outFrameSeq) const
{
	std::lock_guard<std::mutex> lock(m_latestPoseMutex);
	if (!m_latestPose.bValid)
		return false;

	outTransform= m_latestPose.sourceWorldTransform;
	outFrameSeq= m_latestPose.frameSeq;
	return true;
}

// -- IPropertyInterface ----
void ARKitVideoSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	VideoSourceComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(ARKitVideoSourceDefinition::k_basePortPropertyId, MikanVariantType::INT)
			->setDefaultValue(DEFAULT_ARKIT_BASE_PORT));
}

bool ARKitVideoSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	ARKitVideoSourceDefinitionPtr definition= getARKitVideoSourceDefinition();

	if (propertyName == ARKitVideoSourceDefinition::k_basePortPropertyId)
	{
		outValue= definition->getBasePort();
		return true;
	}

	return VideoSourceComponent::getPropertyValue(propertyName, outValue);
}

bool ARKitVideoSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	ARKitVideoSourceDefinitionPtr definition= getARKitVideoSourceDefinition();

	if (propertyName == ARKitVideoSourceDefinition::k_basePortPropertyId)
	{
		definition->setBasePort(inValue.getIntValue());
		return true;
	}

	return VideoSourceComponent::setPropertyValue(propertyName, inValue);
}

void ARKitVideoSourceComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
														const ConfigPropertyChangeSet& changedPropertySet)
{
	// basePort is the only property left that affects the open connection -
	// changing it must force a reconnect.
	if (changedPropertySet.hasPropertyName(ARKitVideoSourceDefinition::k_basePortPropertyId))
	{
		closeVideoSource();
		openVideoSource();
	}
	else
	{
		VideoSourceComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);
	}
}
