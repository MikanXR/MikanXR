#include "CameraMath.h"
#include "Logger.h"
#include "VideoFrameDistortionView.h"
#include "IEditorWindow.h"
#include "MikanCoreTypes.h"
#include "MikanObject.h"
#include "MikanVideoSourceTypes.h"
#include "MonoLensCalibration/AppStage_MonoLensCalibration.h"
#include "VideoSourceSettings/AppStage_VideoSourceSettings.h"
#include "VideoSourceComponent.h"

#include <easy/profiler.h>

// -- VideoSourceDefinition -----
const std::string VideoSourceDefinition::k_videoSourceIntrinsicsPropertyId= "intrinsics_ptr";
const std::string VideoSourceDefinition::k_intrinsicsTypePropertyId= "intrinsics_type";
const std::string VideoSourceDefinition::k_isFrameMirroredPropertyId= "is_frame_mirrored";
const std::string VideoSourceDefinition::k_isBufferMirroredPropertyId= "is_buffer_mirrored";
const std::string VideoSourceDefinition::k_videoFrameQueueSizePropertyId= "video_frame_queue_size";

VideoSourceDefinition::VideoSourceDefinition()
	: MikanComponentDefinition()
	, m_intrinsics()
{
}

VideoSourceDefinition::VideoSourceDefinition(MikanVideoSourceID videoSourceId)
	: MikanComponentDefinition(videoSourceId, "")
	, m_intrinsics()
{
}

configuru::Config VideoSourceDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[VideoSourceDefinition::k_isFrameMirroredPropertyId]= m_bIsFrameMirrored;
	pt[VideoSourceDefinition::k_isBufferMirroredPropertyId]= m_bIsBufferMirrored;
	pt[VideoSourceDefinition::k_videoFrameQueueSizePropertyId]= m_videoFrameQueueSize;

	switch (m_intrinsics.intrinsics_type)
	{
	case MikanIntrinsicsType::MONO_CAMERA_INTRINSICS:
		pt["intrinsics_type"]= std::string("mono");
		CommonConfig::writeMonoTrackerIntrinsics(pt, m_intrinsics.getMonoIntrinsics());
		break;
	case MikanIntrinsicsType::STEREO_CAMERA_INTRINSICS:
		pt["intrinsics_type"]= std::string("stereo");
		CommonConfig::writeStereoTrackerIntrinsics(pt, m_intrinsics.getStereoIntrinsics());
		break;
	}

	return pt;
}

void VideoSourceDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_bIsFrameMirrored= pt.get_or<bool>("is_frame_mirrored", false);
	m_bIsBufferMirrored= pt.get_or<bool>("is_buffer_mirrored", false);
	m_videoFrameQueueSize= pt.get_or<int>("video_frame_queue_size", 10);

	std::string intrinsics_type= pt.get_or<std::string>("intrinsics_type", "");
	if (intrinsics_type == "mono")
	{
		MikanMonoIntrinsics monoIntrinsics= {};
		CommonConfig::readMonoTrackerIntrinsics(pt, monoIntrinsics);

		m_intrinsics.makeMonoIntrinsics()= monoIntrinsics;
	}
	else if (intrinsics_type == "stereo")
	{
		MikanStereoIntrinsics stereoIntrinsics= {};
		CommonConfig::readStereoTrackerIntrinsics(pt, stereoIntrinsics);
		m_intrinsics.intrinsics_type= MikanIntrinsicsType::STEREO_CAMERA_INTRINSICS;

		m_intrinsics.makeStereoIntrinsics()= stereoIntrinsics;
	}
	else
	{
		m_intrinsics= MikanVideoSourceIntrinsics();
	}
}

bool VideoSourceDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanVideoSourceValues>();
	if (componentValues)
	{
		m_intrinsics.intrinsics_ptr= componentValues->intrinsics_ptr;
		m_intrinsics.intrinsics_type= componentValues->intrinsics_type;
		m_bIsFrameMirrored= componentValues->is_frame_mirrored;
		m_bIsBufferMirrored= componentValues->is_buffer_mirrored;
		m_videoFrameQueueSize= componentValues->video_frame_queue_size;
	}

	return true;
}

void VideoSourceDefinition::setIsFrameMirrored(bool isFrameMirrored)
{
	m_bIsFrameMirrored= isFrameMirrored;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_isFrameMirroredPropertyId));
}

void VideoSourceDefinition::setIsBufferMirrored(bool isBufferMirrored)
{
	m_bIsBufferMirrored= isBufferMirrored;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_isBufferMirroredPropertyId));
}

void VideoSourceDefinition::setVideoFrameQueueSize(int videoFrameQueueSize)
{
	if (m_videoFrameQueueSize != videoFrameQueueSize)
	{
		m_videoFrameQueueSize= videoFrameQueueSize;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_videoFrameQueueSizePropertyId));
	}
}

void VideoSourceDefinition::setCameraIntrinsics(const MikanVideoSourceIntrinsics& cameraIntrinsics)
{
	m_intrinsics= cameraIntrinsics;
	notifyPropertyChanged(ConfigPropertyChangeSet()
							  .addPropertyName(k_videoSourceIntrinsicsPropertyId)
							  .addPropertyName(k_intrinsicsTypePropertyId));
}

// -- VideoSourceComponent -----
VideoSourceComponent::VideoSourceComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
	, m_projectionMatrix(glm::mat4(1.f))
{
	m_bWantsUpdate= true;
}

void VideoSourceComponent::startVideoStream(VideoFrameDistortionView* view)
{
	std::lock_guard<std::mutex> lock(m_activeViewMutex);

	m_activeViews.insert(view);
	m_bHasAnyActiveViews.store(true);
}

void VideoSourceComponent::stopVideoStream(VideoFrameDistortionView* view)
{
	{
		std::lock_guard<std::mutex> lock(m_activeViewMutex);

		m_activeViews.erase(view);
		m_bHasAnyActiveViews= !m_activeViews.empty();
	}

	// The device stops with the last subscriber, never while one remains
	if (!m_bHasAnyActiveViews)
	{
		const eVideoStreamingStatus status= getVideoStreamingStatus();
		if (status == eVideoStreamingStatus::started || status == eVideoStreamingStatus::pendingStart)
			stopVideoStreamInternal();
	}
}

void VideoSourceComponent::forceStopVideoStream()
{
	{
		std::lock_guard<std::mutex> lock(m_activeViewMutex);

		m_activeViews.clear();
		m_bHasAnyActiveViews= false;
	}

	stopVideoStreamInternal();
}

void VideoSourceComponent::update(float deltaSeconds)
{
	if (m_bHasAnyActiveViews)
	{
		const eVideoStreamingStatus status= getVideoStreamingStatus();
		if (status != eVideoStreamingStatus::started && status != eVideoStreamingStatus::pendingStart)
			startVideoStreamInternal();
	}
}

// -- IEntityAccessor ----
rfk::Struct const* VideoSourceComponent::getClientAPIValuesStructType() const
{
	return &MikanVideoSourceValues::staticGetArchetype();
}

void VideoSourceComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	MikanComponent::setDefinition(definition);

	auto videoSourceDefinitionPtr= std::static_pointer_cast<VideoSourceDefinition>(definition);
}

MikanVideoSourceID VideoSourceComponent::getVideoSourceId() const
{
	return getVideoSourceDefinition()->getVideoSourceId();
}

bool VideoSourceComponent::getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const { return false; }

bool VideoSourceComponent::getVideoModeName(std::string& outVideoModeName) const { return false; }

bool VideoSourceComponent::getFrameRate(float& outFrameRate) const { return false; }

bool VideoSourceComponent::getVideoColorimetry(VideoColorimetry& outColorimetry) const { return false; }

bool VideoSourceComponent::areCameraIntrinsicsValid() const
{
	return getVideoSourceDefinition()->getCameraIntrinsicsType() != MikanIntrinsicsType::INVALID_CAMERA_INTRINSICS;
}

bool VideoSourceComponent::getCameraIntrinsics(MikanVideoSourceIntrinsics& out_camera_intrinsics) const
{
	if (areCameraIntrinsicsValid())
	{
		out_camera_intrinsics= getVideoSourceDefinition()->getCameraIntrinsics();

		return true;
	}

	return false;
}

bool VideoSourceComponent::setCameraIntrinsics(const MikanVideoSourceIntrinsics& camera_intrinsics)
{
	getVideoSourceDefinition()->setCameraIntrinsics(camera_intrinsics);
	recomputeCameraProjectionMatrix();

	return true;
}

glm::mat4 VideoSourceComponent::getProjectionMatrix() const { return m_projectionMatrix; }

bool VideoSourceComponent::isVideoSettingSupported(const eVideoSettingType property_type) const { return false; }

bool VideoSourceComponent::setVideoSetting(const eVideoSettingType property_type, float desired_value) { return false; }

bool VideoSourceComponent::getVideoSetting(const eVideoSettingType property_type, float& outFractionValue) const
{
	return false;
}

void VideoSourceComponent::recomputeCameraProjectionMatrix()
{
	MikanVideoSourceIntrinsics intrinsics;
	if (getCameraIntrinsics(intrinsics))
	{
		switch (intrinsics.intrinsics_type)
		{
		case MikanIntrinsicsType::MONO_CAMERA_INTRINSICS:
		{
			const MikanMonoIntrinsics& monoIntrinsics= intrinsics.getMonoIntrinsics();

			computeOpenGLProjMatFromCameraIntrinsics(monoIntrinsics, m_projectionMatrix);
		}
		break;
		case MikanIntrinsicsType::STEREO_CAMERA_INTRINSICS:
		{
			const MikanStereoIntrinsics& stereoIntrinsics= intrinsics.getStereoIntrinsics();

			computeOpenGLProjMatFromCameraIntrinsics(stereoIntrinsics, eStereoIntrinsicsSide::left, m_projectionMatrix);
		}
		break;
		}
	}
}

// The set stays locked while the views are written, rather than copying the
// pointers out first. A view is a raw pointer owned by a stage or a compositor,
// and the receive thread has no other way to know it was freed: holding the lock
// makes stopVideoStream wait out a write in progress, which is at most one frame
// copy per view.
void VideoSourceComponent::writeVideoFrame(const unsigned char* videoBuffer, const cv::Size& bufferDimensions,
										   const bool bIsFlipped)
{
	std::lock_guard<std::mutex> lock(m_activeViewMutex);

	for (VideoFrameDistortionView* activeView : m_activeViews)
	{
		activeView->writeVideoFrame(videoBuffer, bufferDimensions, bIsFlipped);
	}
}

void VideoSourceComponent::writeStereoVideoFrameSection(const unsigned char* videoBuffer,
														const cv::Size& bufferDimensions, const bool bIsFlipped,
														const VideoFrameSection section, const cv::Rect& bufferBounds)
{
	std::lock_guard<std::mutex> lock(m_activeViewMutex);

	for (VideoFrameDistortionView* activeView : m_activeViews)
	{
		if (activeView->getVideoFrameSection() == section)
		{
			activeView->writeStereoVideoFrameSection(videoBuffer, bufferDimensions, bIsFlipped, bufferBounds);
		}
	}
}

// -- IPropertyInterface ----
void VideoSourceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(VideoSourceDefinition::k_isFrameMirroredPropertyId,
																  MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(VideoSourceDefinition::k_isBufferMirroredPropertyId,
																  MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VideoSourceDefinition::k_videoFrameQueueSizePropertyId, MikanVariantType::INT)
								 ->setDefaultValue(DEFAULT_VIDEO_FRAME_QUEUE_SIZE));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VideoSourceDefinition::k_intrinsicsTypePropertyId, MikanVariantType::INT)
			->setReadOnly()
			->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VideoSourceDefinition::k_videoSourceIntrinsicsPropertyId,
											 MikanVariantType::POLYMORPHIC_OBJECT)
			->setReadOnly()
			->setUIHidden());
}

bool VideoSourceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == VideoSourceDefinition::k_isFrameMirroredPropertyId)
	{
		outValue= getVideoSourceDefinition()->getIsFrameMirrored();
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_isBufferMirroredPropertyId)
	{
		outValue= getVideoSourceDefinition()->getIsBufferMirrored();
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_videoFrameQueueSizePropertyId)
	{
		outValue= getVideoSourceDefinition()->getVideoFrameQueueSize();
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_intrinsicsTypePropertyId)
	{
		outValue= (int)getVideoSourceDefinition()->getCameraIntrinsicsType();
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_videoSourceIntrinsicsPropertyId)
	{
		outValue= getVideoSourceDefinition()->getCameraIntrinsics().intrinsics_ptr;
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool VideoSourceComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == VideoSourceDefinition::k_isFrameMirroredPropertyId)
	{
		getVideoSourceDefinition()->setIsFrameMirrored(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_isBufferMirroredPropertyId)
	{
		getVideoSourceDefinition()->setIsBufferMirrored(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == VideoSourceDefinition::k_videoFrameQueueSizePropertyId)
	{
		getVideoSourceDefinition()->setVideoFrameQueueSize(inValue.getIntValue());
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string VideoSourceComponent::k_showVideoSourceSettingsFunctionId= "show_video_source_settings";
const std::string VideoSourceComponent::k_calibrateIntrinsicsFunctionId= "calibrate_intrinsics";
const std::string VideoSourceComponent::k_testIntrinsicsFunctionId= "test_intrinsics";

void VideoSourceComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_showVideoSourceSettingsFunctionId, "Show Video Source Settings"));
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_calibrateIntrinsicsFunctionId, "Calibrate Intrinsics"));
	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_testIntrinsicsFunctionId, "Test Intrinsics"));
}

bool VideoSourceComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_showVideoSourceSettingsFunctionId)
	{
		showVideoSourceSettings();
		return true;
	}
	else if (functionName == k_calibrateIntrinsicsFunctionId)
	{
		calibrateIntrinsics();
		return true;
	}
	else if (functionName == k_testIntrinsicsFunctionId)
	{
		testIntrinsics();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}

void VideoSourceComponent::showVideoSourceSettings()
{
	getOwnerEditorWindow()->pushAppStageOfType<AppStage_VideoSourceSettings>()->setVideoSourceComponent(
		getSelfPtr<VideoSourceComponent>());
}

void VideoSourceComponent::calibrateIntrinsics()
{
	auto* monoLensCalibration= getOwnerEditorWindow()->pushAppStageOfType<AppStage_MonoLensCalibration>();
	monoLensCalibration->setVideoSourceComponent(getSelfPtr<VideoSourceComponent>());
}

void VideoSourceComponent::testIntrinsics()
{
	auto* monoLensCalibration= getOwnerEditorWindow()->pushAppStageOfType<AppStage_MonoLensCalibration>();
	monoLensCalibration->setBypassCalibrationFlag(true);
	monoLensCalibration->setVideoSourceComponent(getSelfPtr<VideoSourceComponent>());
}