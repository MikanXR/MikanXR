#include "MikanWMFVideoDevice.h"
#include "MikanWMFVideoDeviceManager.h"
#include "Logger.h"
#include "MemoryUtils.h"
#include "WMFUtility.h"
#include "WMFVideoFrameProcessor.h"

#include <mfreadwrite.h>
#include <sstream>
#include <iomanip>

using namespace WMFUtility;

namespace
{
// The symbolic link an enumerated activate names, in the same form
// WMFDeviceList stores it, or empty when the activate has none
std::string readSymbolicLink(IMFActivate* activate)
{
	wchar_t* wszSymbolicLink= nullptr;
	if (FAILED(activate->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &wszSymbolicLink,
											NULL)))
	{
		return std::string();
	}

	char szSymbolicLink[512];
	wcstombs(szSymbolicLink, wszSymbolicLink, (unsigned)_countof(szSymbolicLink));
	CoTaskMemFree(wszSymbolicLink);

	return std::string(szSymbolicLink);
}
} // namespace

// -- MikanUsbVideoDevice -----
MikanWMFVideoDevice::MikanWMFVideoDevice(MikanWMFVideoDeviceManager* ownerDeviceManager,
										 const WMFDeviceInfo& deviceInfo)
	: m_ownerDeviceManager(ownerDeviceManager)
	, m_deviceInfo(deviceInfo)
{
}

MikanWMFVideoDevice::~MikanWMFVideoDevice() { close(); }

// -- Device Listener
void MikanWMFVideoDevice::addListener(IUsbVideoDeviceListener* listener)
{
	std::lock_guard<std::mutex> lock(m_listenerMutex);
	m_listeners.insert(listener);
}

// Takes the same lock the frame fan-out holds, so a listener removed here is
// never entered by a frame that was already being delivered
void MikanWMFVideoDevice::removeListener(IUsbVideoDeviceListener* listener)
{
	std::lock_guard<std::mutex> lock(m_listenerMutex);
	m_listeners.erase(listener);
}

// -- Device Properties
const char* MikanWMFVideoDevice::getDevicePath() const { return m_deviceInfo.deviceSymbolicLink.c_str(); }

const char* MikanWMFVideoDevice::getFriendlyName() const { return m_deviceInfo.deviceFriendlyName.c_str(); }

// -- Device Activation
bool MikanWMFVideoDevice::getIsOpen() const { return m_mediaSource != nullptr; }

bool MikanWMFVideoDevice::open()
{
	HRESULT hr;

	// Early out if already open
	if (getIsOpen())
	{
		return true;
	}

	if (m_currentVideoModeIndex >= 0 && m_currentVideoModeIndex < m_deviceInfo.deviceAvailableFormats.size())
	{
		IMFAttributes* pAttributes= NULL;
		IMFActivate* vd_pActivate= NULL;

		hr= MFCreateAttributes(&pAttributes, 1);

		if (SUCCEEDED(hr))
		{
			hr= pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
									 MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
		}

		// The activate is found by its symbolic link rather than by the position
		// the device held when the list was built: a camera plugged in or pulled
		// since then shifts every later index, and the enumeration order carries
		// no promise between calls.
		const char* failedStage= "create attributes";
		IMFActivate* deviceActivationInterface= nullptr;
		if (SUCCEEDED(hr))
		{
			failedStage= "enumerate devices";
			IMFActivate** ppDevices= nullptr;
			UINT32 wmfDeviceCount= 0;
			hr= MFEnumDeviceSources(pAttributes, &ppDevices, &wmfDeviceCount);

			if (SUCCEEDED(hr))
			{
				for (UINT32 i= 0; i < wmfDeviceCount; i++)
				{
					if (deviceActivationInterface == nullptr
						&& readSymbolicLink(ppDevices[i]) == m_deviceInfo.deviceSymbolicLink)
					{
						deviceActivationInterface= ppDevices[i];
						deviceActivationInterface->AddRef();
					}

					MemoryUtils::safeRelease(&ppDevices[i]);
				}

				// The array itself is task memory owned by the caller
				CoTaskMemFree(ppDevices);

				if (deviceActivationInterface == nullptr)
				{
					failedStage= "find device";
					hr= MF_E_NOT_FOUND;
				}
			}
		}

		if (SUCCEEDED(hr))
		{
			failedStage= "activate media source";
			hr= deviceActivationInterface->ActivateObject(__uuidof(IMFMediaSource), (void**)&m_mediaSource);
		}

		IMFPresentationDescriptor* pPD= nullptr;
		if (SUCCEEDED(hr))
		{
			failedStage= "create presentation descriptor";
			hr= m_mediaSource->CreatePresentationDescriptor(&pPD);
		}

		BOOL fSelected;
		IMFStreamDescriptor* pSD= nullptr;
		if (SUCCEEDED(hr))
		{
			failedStage= "get stream descriptor";
			hr= pPD->GetStreamDescriptorByIndex(0, &fSelected, &pSD);
		}

		IMFMediaTypeHandler* pHandler= nullptr;
		if (SUCCEEDED(hr))
		{
			failedStage= "get media type handler";
			hr= pSD->GetMediaTypeHandler(&pHandler);
		}

		DWORD cTypes= 0;
		if (SUCCEEDED(hr))
		{
			failedStage= "get media type count";
			hr= pHandler->GetMediaTypeCount(&cTypes);
		}

		IMFMediaType* pType= nullptr;
		if (SUCCEEDED(hr))
		{
			failedStage= "get media type";
			hr= pHandler->GetMediaTypeByIndex((DWORD)m_currentVideoModeIndex, &pType);
		}

		if (SUCCEEDED(hr))
		{
			failedStage= "set media type";
			hr= pHandler->SetCurrentMediaType(pType);
		}

		if (SUCCEEDED(hr))
		{
			failedStage= "init frame reader";
			const WMFDeviceFormatInfo& deviceFormat= m_deviceInfo.deviceAvailableFormats[m_currentVideoModeIndex];

			m_videoFrameProcessor= new WMFVideoFrameProcessor(m_deviceInfo.wmfDeviceIndex, deviceFormat, this);
			hr= m_videoFrameProcessor->init(m_mediaSource);
		}

		if (SUCCEEDED(hr))
		{
			// Update the property constraints for the current video format
			for (int prop_index= 0; prop_index < (int)eVideoSettingType::COUNT; ++prop_index)
			{
				getVideoSettingConstraint((eVideoSettingType)prop_index, m_videoPropertyConstraints[prop_index]);
			}
		}

		MemoryUtils::safeRelease(&pPD);
		MemoryUtils::safeRelease(&pSD);
		MemoryUtils::safeRelease(&pHandler);
		MemoryUtils::safeRelease(&pType);
		MemoryUtils::safeRelease(&deviceActivationInterface);
		MemoryUtils::safeRelease(&pAttributes);

		if (!SUCCEEDED(hr))
		{
			MIKAN_LOG_ERROR("MikanWMFVideoDevice::open")
				<< "Failed to open " << m_deviceInfo.deviceFriendlyName << " (" << m_deviceInfo.deviceSymbolicLink
				<< ") at stage '" << failedStage << "': " << getHresultMessage(hr);
			close();
		}
	}
	else
	{
		hr= E_INVALIDARG;
	}

	return SUCCEEDED(hr);
}

void MikanWMFVideoDevice::close()
{
	if (m_videoFrameProcessor != nullptr)
	{
		// Stopping waits for the reader's flush, so no callback is in flight by
		// the time the reader is released. The processor is a COM object the
		// reader also referenced, so it is released, never deleted.
		m_videoFrameProcessor->stopVideoFrameStream();
		m_videoFrameProcessor->dispose();
		m_videoFrameProcessor->Release();
		m_videoFrameProcessor= nullptr;
	}

	if (m_mediaSource != nullptr)
	{
		m_mediaSource->Stop();
		// Shutdown is what actually frees the capture device, so the same camera
		// can be opened again in this process
		m_mediaSource->Shutdown();
		MemoryUtils::safeRelease(&m_mediaSource);
	}
}

// -- Video Mode
size_t MikanWMFVideoDevice::getAvailableVideoModesCount() const { return m_deviceInfo.deviceAvailableFormats.size(); }

bool MikanWMFVideoDevice::getVideoModeProperties(size_t index, UsbVideoModeProperties& outProperties) const
{
	if (index < m_deviceInfo.deviceAvailableFormats.size())
	{
		const WMFDeviceFormatInfo& formatInfo= m_deviceInfo.deviceAvailableFormats[index];

		outProperties.index= index;
		outProperties.name= formatInfo.format_friendly_name.c_str();
		outProperties.width= formatInfo.width;
		outProperties.height= formatInfo.height;
		outProperties.frame_rate_numerator= formatInfo.frame_rate_numerator;
		outProperties.frame_rate_demonenator= formatInfo.frame_rate_denominator;
		outProperties.format= formatInfo.sub_type_name.c_str();

		switch (formatInfo.yuv_matrix)
		{
		case MFVideoTransferMatrix_Unknown:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::INVALID;
			break;
		case MFVideoTransferMatrix_BT709:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::BT709;
			break;
		case MFVideoTransferMatrix_BT601:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::BT601;
			break;
		case MFVideoTransferMatrix_SMPTE240M:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::SMPTE240M;
			break;
		case MFVideoTransferMatrix_BT2020_10:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::BT2020_10;
			break;
		case MFVideoTransferMatrix_BT2020_12:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::BT2020_12;
			break;
		case MFVideoTransferMatrix_Identity:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::Identity;
			break;
		case MFVideoTransferMatrix_FCC47:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::FCC47;
			break;
		case MFVideoTransferMatrix_YCgCo:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::YCgCo;
			break;
		case MFVideoTransferMatrix_SMPTE2085:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::SMPTE2085;
			break;
		case MFVideoTransferMatrix_Chroma:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::Chroma;
			break;
		case MFVideoTransferMatrix_Chroma_const:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::Chroma_const;
			break;
		case MFVideoTransferMatrix_ICtCp:
			outProperties.colorimetry.colorMatrix= eVideoColorMatrix::ICtCp;
			break;
		}

		switch (formatInfo.transfer_function)
		{
		case MFVideoTransFunc_Unknown:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::INVALID;
			break;
		case MFVideoTransFunc_10:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_1_0;
			break;
		case MFVideoTransFunc_18:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_1_8;
			break;
		case MFVideoTransFunc_20:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_2_0;
			break;
		case MFVideoTransFunc_22:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_2_2;
			break;
		case MFVideoTransFunc_709:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::BT709;
			break;
		case MFVideoTransFunc_240M:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::SPMTE_240M;
			break;
		case MFVideoTransFunc_sRGB:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::SRGB;
			break;
		case MFVideoTransFunc_28:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_2_8;
			break;
		case MFVideoTransFunc_Log_100:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::LOG_100;
			break;
		case MFVideoTransFunc_Log_316:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::LOG_316;
			break;
		case MFVideoTransFunc_709_sym:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::BT709_SYM;
			break;
		case MFVideoTransFunc_2020_const:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::BT2020_CONST;
			break;
		case MFVideoTransFunc_2020:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::BT2020;
			break;
		case MFVideoTransFunc_26:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::Gamma_2_6;
			break;
		case MFVideoTransFunc_2084:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::SMPTE_2084;
			break;
		case MFVideoTransFunc_HLG:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::HLG;
			break;
		case MFVideoTransFunc_10_rel:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::NoGamma;
			break;
		case MFVideoTransFunc_BT1361_ECG:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::BT1361_ECG;
			break;
		case MFVideoTransFunc_SMPTE428:
			outProperties.colorimetry.transferFunction= eVideoTransferFunction::SMPTE_428;
			break;
		}

		switch (formatInfo.video_nominal_range)
		{
		case MFNominalRange_Normal:
			outProperties.colorimetry.isFullRange= true;
			break;
		case MFNominalRange_Unknown:
		case MFNominalRange_Wide:
		case MFNominalRange_48_208:
		case MFNominalRange_64_127:
		default:
			outProperties.colorimetry.isFullRange= false;
			break;
			break;
		}

		return true;
	}

	return false;
}

int MikanWMFVideoDevice::getVideoModeIndex() const { return m_currentVideoModeIndex; }

const char* MikanWMFVideoDevice::getVideoModeName() const
{
	if (m_currentVideoModeIndex >= 0 && m_currentVideoModeIndex < (int)m_deviceInfo.deviceAvailableFormats.size())
	{
		return m_deviceInfo.deviceAvailableFormats[m_currentVideoModeIndex].format_friendly_name.c_str();
	}

	return nullptr;
}

bool MikanWMFVideoDevice::setVideoModeByName(const char* szVideoModeName)
{
	if (szVideoModeName != nullptr && !m_deviceInfo.deviceAvailableFormats.empty())
	{
		const int desiredFormatIndex= m_deviceInfo.findDeviceFormatByName(szVideoModeName);

		return setVideoModeByIndex(desiredFormatIndex);
	}

	return false;
}

bool MikanWMFVideoDevice::setVideoModeByIndex(size_t desiredFormatIndex)
{
	if (m_currentVideoModeIndex != desiredFormatIndex && desiredFormatIndex >= 0
		&& desiredFormatIndex < (int)m_deviceInfo.deviceAvailableFormats.size())
	{
		// Stop streaming and close device
		close();

		m_currentVideoModeIndex= (int)desiredFormatIndex;

		notifyVideoModePropertiesChanged();
		return true;
	}

	return false;
}

// -- Camera Settings
bool MikanWMFVideoDevice::isVideoSettingSupported(const eVideoSettingType property_type) const
{
	VideoSettingConstraint constraint;

	return getVideoSettingConstraint(property_type, constraint);
}

bool MikanWMFVideoDevice::getVideoSettingConstraint(const eVideoSettingType property_type,
													VideoSettingConstraint& outConstraint) const
{
	if (!getIsOpen())
	{
		MIKAN_LOG_ERROR("MikanWMFVideoDevice::getVideoSettingConstraint")
			<< "Unable to get video setting constraint: Device not open: " << m_deviceInfo.deviceFriendlyName;
		return false;
	}

	bool bSuccess= false;
	switch (property_type)
	{
	case eVideoSettingType::Brightness:
		bSuccess= getProcAmpRange(VideoProcAmp_Brightness, outConstraint);
		break;
	case eVideoSettingType::Contrast:
		bSuccess= getProcAmpRange(VideoProcAmp_Contrast, outConstraint);
		break;
	case eVideoSettingType::Hue:
		bSuccess= getProcAmpRange(VideoProcAmp_Hue, outConstraint);
		break;
	case eVideoSettingType::Saturation:
		bSuccess= getProcAmpRange(VideoProcAmp_Saturation, outConstraint);
		break;
	case eVideoSettingType::Sharpness:
		bSuccess= getProcAmpRange(VideoProcAmp_Sharpness, outConstraint);
		break;
	case eVideoSettingType::Gamma:
		bSuccess= getProcAmpRange(VideoProcAmp_Gamma, outConstraint);
		break;
	case eVideoSettingType::WhiteBalance:
		bSuccess= getProcAmpRange(VideoProcAmp_WhiteBalance, outConstraint);
		break;
	case eVideoSettingType::RedBalance:
	case eVideoSettingType::GreenBalance:
	case eVideoSettingType::BlueBalance:
		memset(&outConstraint, 0, sizeof(VideoSettingConstraint));
		bSuccess= false;
		break;
	case eVideoSettingType::Gain:
		bSuccess= getProcAmpRange(VideoProcAmp_Gain, outConstraint);
		break;
	case eVideoSettingType::Pan:
		bSuccess= getCameraControlRange(CameraControl_Pan, outConstraint);
		break;
	case eVideoSettingType::Tilt:
		bSuccess= getCameraControlRange(CameraControl_Tilt, outConstraint);
		break;
	case eVideoSettingType::Roll:
		bSuccess= getCameraControlRange(CameraControl_Roll, outConstraint);
		break;
	case eVideoSettingType::Zoom:
		bSuccess= getCameraControlRange(CameraControl_Zoom, outConstraint);
		break;
	case eVideoSettingType::Exposure:
		bSuccess= getCameraControlRange(CameraControl_Exposure, outConstraint);
		break;
	case eVideoSettingType::Iris:
		bSuccess= getCameraControlRange(CameraControl_Iris, outConstraint);
		break;
	case eVideoSettingType::Focus:
		bSuccess= getCameraControlRange(CameraControl_Focus, outConstraint);
		break;
	}

	if (bSuccess)
	{
		bSuccess= outConstraint.max_value > outConstraint.min_value;
	}

	return bSuccess;
}

void MikanWMFVideoDevice::setVideoSetting(const eVideoSettingType property_type, int desired_value)
{
	if (!getIsOpen())
	{
		MIKAN_LOG_ERROR("MikanWMFVideoDevice::setVideoSetting")
			<< "Unable to set video setting: Device not open: " << m_deviceInfo.deviceFriendlyName;

		return;
	}

	switch (property_type)
	{
	case eVideoSettingType::Brightness:
		setProcAmpProperty(VideoProcAmp_Brightness, desired_value, false);
		break;
	case eVideoSettingType::Contrast:
		setProcAmpProperty(VideoProcAmp_Contrast, desired_value, false);
		break;
	case eVideoSettingType::Hue:
		setProcAmpProperty(VideoProcAmp_Hue, desired_value, false);
		break;
	case eVideoSettingType::Saturation:
		setProcAmpProperty(VideoProcAmp_Saturation, desired_value, false);
		break;
	case eVideoSettingType::Sharpness:
		setProcAmpProperty(VideoProcAmp_Sharpness, desired_value, false);
		break;
	case eVideoSettingType::Gamma:
		setProcAmpProperty(VideoProcAmp_Gamma, desired_value, false);
		break;
	case eVideoSettingType::WhiteBalance:
		setProcAmpProperty(VideoProcAmp_WhiteBalance, desired_value, false);
		break;
	case eVideoSettingType::RedBalance:
	case eVideoSettingType::GreenBalance:
	case eVideoSettingType::BlueBalance:
		// not supported
		break;
	case eVideoSettingType::Gain:
		setProcAmpProperty(VideoProcAmp_Gain, desired_value, false);
		break;
	case eVideoSettingType::Pan:
		setCameraControlProperty(CameraControl_Pan, desired_value, false);
		break;
	case eVideoSettingType::Tilt:
		setCameraControlProperty(CameraControl_Tilt, desired_value, false);
		break;
	case eVideoSettingType::Roll:
		setCameraControlProperty(CameraControl_Roll, desired_value, false);
		break;
	case eVideoSettingType::Zoom:
		setCameraControlProperty(CameraControl_Zoom, desired_value, false);
		break;
	case eVideoSettingType::Exposure:
		setCameraControlProperty(CameraControl_Exposure, desired_value, false);
		break;
	case eVideoSettingType::Iris:
		setCameraControlProperty(CameraControl_Iris, desired_value, false);
		break;
	case eVideoSettingType::Focus:
		setCameraControlProperty(CameraControl_Focus, desired_value, false);
		break;
	}
}

int MikanWMFVideoDevice::getVideoSetting(const eVideoSettingType property_type) const
{
	if (!getIsOpen())
	{
		MIKAN_LOG_ERROR("MikanWMFVideoDevice::getVideoSetting")
			<< "Unable to get video setting: Device not open: " << m_deviceInfo.deviceFriendlyName;
		return 0;
	}

	int value= 0;
	switch (property_type)
	{
	case eVideoSettingType::Brightness:
		value= getProcAmpProperty(VideoProcAmp_Brightness);
		break;
	case eVideoSettingType::Contrast:
		value= getProcAmpProperty(VideoProcAmp_Contrast);
		break;
	case eVideoSettingType::Hue:
		value= getProcAmpProperty(VideoProcAmp_Hue);
		break;
	case eVideoSettingType::Saturation:
		value= getProcAmpProperty(VideoProcAmp_Saturation);
		break;
	case eVideoSettingType::Sharpness:
		value= getProcAmpProperty(VideoProcAmp_Sharpness);
		break;
	case eVideoSettingType::Gamma:
		value= getProcAmpProperty(VideoProcAmp_Gamma);
		break;
	case eVideoSettingType::WhiteBalance:
		value= getProcAmpProperty(VideoProcAmp_WhiteBalance);
		break;
	case eVideoSettingType::RedBalance:
	case eVideoSettingType::GreenBalance:
	case eVideoSettingType::BlueBalance:
		// not supported
		break;
	case eVideoSettingType::Gain:
		value= getProcAmpProperty(VideoProcAmp_Gain);
		break;
	case eVideoSettingType::Pan:
		value= getCameraControlProperty(CameraControl_Pan);
		break;
	case eVideoSettingType::Tilt:
		value= getCameraControlProperty(CameraControl_Tilt);
		break;
	case eVideoSettingType::Roll:
		value= getCameraControlProperty(CameraControl_Roll);
		break;
	case eVideoSettingType::Zoom:
		value= getCameraControlProperty(CameraControl_Zoom);
		break;
	case eVideoSettingType::Exposure:
		value= getCameraControlProperty(CameraControl_Exposure);
		break;
	case eVideoSettingType::Iris:
		value= getCameraControlProperty(CameraControl_Iris);
		break;
	case eVideoSettingType::Focus:
		value= getCameraControlProperty(CameraControl_Focus);
		break;
	}

	return value;
}

// -- Video Streaming
eVideoStreamingStatus MikanWMFVideoDevice::startVideoStream()
{
	if (getIsOpen())
	{
		m_videoFrameProcessor->startVideoFrameStream();

		return getVideoStreamingStatus();
	}

	return eVideoStreamingStatus::failed;
}

eVideoStreamingStatus MikanWMFVideoDevice::getVideoStreamingStatus() const
{
	if (getIsOpen())
	{
		switch (m_videoFrameProcessor->getState())
		{
		case WMFVideoFrameProcessor::State::Stopped:
			return eVideoStreamingStatus::stopped;
		case WMFVideoFrameProcessor::State::Starting:
			return eVideoStreamingStatus::pendingStart;
		case WMFVideoFrameProcessor::State::Running:
			return eVideoStreamingStatus::started;
		case WMFVideoFrameProcessor::State::Stopping:
			return eVideoStreamingStatus::stopped;
		case WMFVideoFrameProcessor::State::Failed:
			return eVideoStreamingStatus::failed;
		}
	}

	return eVideoStreamingStatus::failed;
}

void MikanWMFVideoDevice::stopVideoStream()
{
	if (getIsOpen())
	{
		m_videoFrameProcessor->stopVideoFrameStream();
	}
}

/*
	See https://msdn.microsoft.com/en-us/library/windows/desktop/dd407328(v=vs.85).aspx
	VideoProcAmp_Brightness		[-10k, 10k]
	VideoProcAmp_Contrast			[0, 10k]
	VideoProcAmp_Hue				[-180k, 180k]
	VideoProcAmp_Saturation		[0, 10k]
	VideoProcAmp_Sharpness		[0, 100]
	VideoProcAmp_Gamma			[1, 500]
	VideoProcAmp_ColorEnable		0=off, 1=on
	VideoProcAmp_WhiteBalance		device dependent
	VideoProcAmp_BacklightCompensation		0=off, 1=on
	VideoProcAmp_Gain				device dependent
*/
bool MikanWMFVideoDevice::setProcAmpProperty(VideoProcAmpProperty propId, long value, bool bAuto)
{
	bool bSuccess= false;

	IAMVideoProcAmp* pProcAmp= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pProcAmp));

	if (SUCCEEDED(hr))
	{
		hr= pProcAmp->Set(propId, value, bAuto ? VideoProcAmp_Flags_Auto : VideoProcAmp_Flags_Manual);
		pProcAmp->Release();
	}

	return SUCCEEDED(hr);
}

long MikanWMFVideoDevice::getProcAmpProperty(VideoProcAmpProperty propId, bool* bIsAuto) const
{
	long intValue= 0;
	IAMVideoProcAmp* pProcAmp= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pProcAmp));

	if (SUCCEEDED(hr))
	{
		long flags;
		hr= pProcAmp->Get(propId, &intValue, &flags);

		if (bIsAuto != nullptr)
		{
			*bIsAuto= flags == VideoProcAmp_Flags_Auto;
		}

		pProcAmp->Release();
	}

	return intValue;
}

bool MikanWMFVideoDevice::getProcAmpRange(VideoProcAmpProperty propId, VideoSettingConstraint& constraint) const
{
	IAMVideoProcAmp* pProcAmp= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pProcAmp));

	memset(&constraint, 0, sizeof(VideoSettingConstraint));

	if (SUCCEEDED(hr))
	{
		long minValue, maxValue, stepSize, defaultValue, flags;
		hr= pProcAmp->GetRange(propId, &minValue, &maxValue, &stepSize, &defaultValue, &flags);

		if (SUCCEEDED(hr))
		{
			constraint.default_value= defaultValue;
			constraint.min_value= minValue;
			constraint.max_value= maxValue;
			constraint.stepping_delta= stepSize;
			constraint.is_automatic= flags == VideoProcAmp_Flags_Auto;
		}

		pProcAmp->Release();
	}

	return SUCCEEDED(hr);
}

bool MikanWMFVideoDevice::setCameraControlProperty(CameraControlProperty propId, long value, bool bAuto)
{
	bool bSuccess= false;

	IAMCameraControl* pProcControl= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pProcControl));

	if (SUCCEEDED(hr))
	{
		hr= pProcControl->Set(propId, value, bAuto ? CameraControl_Flags_Auto : CameraControl_Flags_Manual);

		pProcControl->Release();
	}

	return SUCCEEDED(hr);
}

long MikanWMFVideoDevice::getCameraControlProperty(CameraControlProperty propId, bool* bIsAuto) const
{
	long intValue= 0;
	IAMCameraControl* pCameraControl= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pCameraControl));

	if (SUCCEEDED(hr))
	{
		long flags;
		hr= pCameraControl->Get(propId, &intValue, &flags);

		if (bIsAuto != nullptr)
		{
			*bIsAuto= flags == CameraControl_Flags_Auto;
		}

		pCameraControl->Release();
	}

	return intValue;
}

bool MikanWMFVideoDevice::getCameraControlRange(CameraControlProperty propId, VideoSettingConstraint& constraint) const
{
	double unitValue= 0;
	IAMCameraControl* pCameraControl= NULL;
	HRESULT hr= m_mediaSource->QueryInterface(IID_PPV_ARGS(&pCameraControl));

	memset(&constraint, 0, sizeof(VideoSettingConstraint));

	if (SUCCEEDED(hr))
	{
		long minValue, maxValue, stepSize, defaultValue, flags;
		hr= pCameraControl->GetRange(propId, &minValue, &maxValue, &stepSize, &defaultValue, &flags);

		if (SUCCEEDED(hr))
		{
			constraint.default_value= defaultValue;
			constraint.min_value= minValue;
			constraint.max_value= maxValue;
			constraint.stepping_delta= stepSize;
			constraint.is_automatic= flags == VideoProcAmp_Flags_Auto;
		}

		pCameraControl->Release();
	}

	return SUCCEEDED(hr);
}

// The main thread notifications walk a copy of the set, since a listener may
// remove itself in response
void MikanWMFVideoDevice::notifyVideoDeviceDisconnected()
{
	close();

	std::set<IUsbVideoDeviceListener*> listeners;
	{
		std::lock_guard<std::mutex> lock(m_listenerMutex);
		listeners= m_listeners;
	}
	for (auto listener : listeners)
	{
		listener->notifyVideoDeviceDisconnected(this);
	}
}

void MikanWMFVideoDevice::notifyVideoModePropertiesChanged()
{
	std::set<IUsbVideoDeviceListener*> listeners;
	{
		std::lock_guard<std::mutex> lock(m_listenerMutex);
		listeners= m_listeners;
	}
	for (auto listener : listeners)
	{
		listener->notifyVideoModePropertiesChanged(this);
	}
}

void MikanWMFVideoDevice::notifyVideoFrameReceived(const UsbVideoFrameBuffer& bufferInfo)
{
	std::lock_guard<std::mutex> lock(m_listenerMutex);
	for (auto listener : m_listeners)
	{
		listener->notifyVideoFrameReceived(bufferInfo);
	}
}