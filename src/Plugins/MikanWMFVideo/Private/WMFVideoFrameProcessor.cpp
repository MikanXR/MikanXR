#include "Logger.h"
#include "MemoryUtils.h"
#include "MikanWMFVideoDevice.h"
#include "WMFDeviceInfo.h"
#include "WMFVideoFrameProcessor.h"
#include "WMFUtility.h"

#include <Mfidl.h>
#include <Mfapi.h>
#include <Mferror.h>
#include <Strmif.h>
#include <Shlwapi.h>

using namespace WMFUtility;

// -- WMF Video Frame Processor -----
WMFVideoFrameProcessor::WMFVideoFrameProcessor(int deviceIndex, const WMFDeviceFormatInfo& deviceFormat,
											   MikanWMFVideoDevice* listener)
	: m_deviceIndex(deviceIndex)
	, m_deviceFormat(deviceFormat)
	, m_videoSourceListener(listener)
	, m_referenceCount(1)
	, m_pSourceReader(nullptr)
	, m_pDecoderTransform(nullptr)
	, m_pNativeInputType(nullptr)
	, m_bNeedsDecoder(false)
	, m_decoderOutputInfo({})
	, m_state(State::Stopped)
	, m_sampleIndex(0)
	, m_outputFormat(eUSBVideoFrameBufferFormat::USBVideo_UNKNOWN)
	, m_wmfOutputFormat(GUID_NULL)
	, m_nv12_offsets_detected(false)
	, m_nv12_uv_plane_offset(0)
{
}

WMFVideoFrameProcessor::~WMFVideoFrameProcessor(void) { dispose(); }

HRESULT WMFVideoFrameProcessor::init(IMFMediaSource* pSource)
{
	// Clean up previous source reader, if any
	// This is critical when changing video modes to avoid stuck transforms
	if (m_pSourceReader)
	{
		// Stop any ongoing streaming first, which flushes the reader and waits
		// for the flush to land so no sample is still in flight
		stopVideoFrameStream();

		// Release our reference to the source reader. Anything Media Foundation
		// still holds is its own to release.
		MemoryUtils::safeRelease(&m_pSourceReader);

		// Give MFTs time to clean up (workaround for AMD/NVIDIA MFT cleanup issues)
		Sleep(100);
	}

	// Create attributes for the source reader
	IMFAttributes* pAttributes= nullptr;
	HRESULT hr= MFCreateAttributes(&pAttributes, 8);

	// Enable async callbacks
	if (SUCCEEDED(hr))
		hr= pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, this);

	// Check for H.264 decoders (we'll decide whether to enable hardware later)
	CLSID selectedDecoderCLSID= GUID_NULL;
	bool microsoftDecoderAvailable= false;

	HRESULT hrDecoder= findBestH264Decoder(&selectedDecoderCLSID);
	if (SUCCEEDED(hrDecoder) && selectedDecoderCLSID != GUID_NULL)
	{
		microsoftDecoderAvailable= true;
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Microsoft H.264 decoder available for compressed formats";
	}

	// Always enable converters (we may need format conversion)
	if (SUCCEEDED(hr))
		hr= pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, FALSE);

	// Defer hardware transforms decision until we know if format is compressed
	// (Will be set after we detect the native format)

	// Make D3D support optional - allows software fallback if hardware fails
	if (SUCCEEDED(hr))
		hr= pAttributes->SetUINT32(MF_READWRITE_D3D_OPTIONAL, TRUE);

	// Enable low latency mode to reduce buffering
	if (SUCCEEDED(hr))
		hr= pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);

	// NOTE: We intentionally do NOT set MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING
	// That flag loads vendor-specific video processors which can hang when reconfiguring.

	// Create the source reader
	// If hardware decoding is safe, the source reader will automatically select Microsoft's decoder
	if (SUCCEEDED(hr))
		hr= MFCreateSourceReaderFromMediaSource(pSource, pAttributes, &m_pSourceReader);

	// Determine output format based on whether source is compressed
	bool useCompressedOutput= m_deviceFormat.isCompressedFormat();

	// Save the native input type BEFORE setting output format
	// This preserves the H.264 media type with codec private data for our manual decoder
	if (SUCCEEDED(hr) && useCompressedOutput)
	{
		// Use the selected format index, not 0!
		int formatIndex= (m_deviceFormat.device_format_index >= 0) ? m_deviceFormat.device_format_index : 0;
		HRESULT hrNative= m_pSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, formatIndex,
															  &m_pNativeInputType);

		if (SUCCEEDED(hrNative) && m_pNativeInputType)
		{
			logNativeMediaType("WMFVideoFrameProcessor::init", m_pNativeInputType);

			GUID nativeSubtype;
			m_pNativeInputType->GetGUID(MF_MT_SUBTYPE, &nativeSubtype);

			// If the native format is NOT actually compressed, disable manual decoder
			if (nativeSubtype != MFVideoFormat_H264 && nativeSubtype != MFVideoFormat_H265
				&& nativeSubtype != MFVideoFormat_MJPG)
			{
				MIKAN_LOG_WARNING("WMFVideoFrameProcessor::init")
					<< "Expected compressed format (" << m_deviceFormat.sub_type_name
					<< ") but camera native format is uncompressed (" << getWMFVideoFormatName(nativeSubtype)
					<< "). Disabling manual decoder - will use source reader output directly.";
				useCompressedOutput= false;
			}
		}
	}

	// Determine preferred output format
	GUID preferredFormat= GUID_NULL;
	GUID fallbackFormat= GUID_NULL;
	eUSBVideoFrameBufferFormat preferredFrameBufferFormat;
	eUSBVideoFrameBufferFormat fallbackFrameBufferFormat;

	if (useCompressedOutput)
	{
		// For compressed formats, prefer NV12 (native decoder output, better performance)
		// with YUY2 as fallback (universally supported but slower due to conversion)
		preferredFormat= MFVideoFormat_NV12;
		preferredFrameBufferFormat= eUSBVideoFrameBufferFormat::USBVideo_NV12;
		fallbackFormat= MFVideoFormat_YUY2;
		fallbackFrameBufferFormat= eUSBVideoFrameBufferFormat::USBVideo_YUY2;

		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Compressed format detected (" << m_deviceFormat.sub_type_name
													   << "), will try NV12 output (fallback: YUY2)";
	}
	else
	{
		// For uncompressed formats, pass through the native format without conversion
		// Color conversion (YUY2→RGB24) can load AMD video processors which cause hangs
		preferredFormat= GUID_NULL; // Will use native format
		preferredFrameBufferFormat= eUSBVideoFrameBufferFormat::USBVideo_UNKNOWN;
		fallbackFormat= GUID_NULL;
		fallbackFrameBufferFormat= eUSBVideoFrameBufferFormat::USBVideo_UNKNOWN;

		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
			<< "Uncompressed format - will use native format without conversion";
	}

	// Now configure hardware transforms based on whether format is compressed
	if (useCompressedOutput && microsoftDecoderAvailable)
	{
		// Safe to use hardware decoding for compressed formats
		if (SUCCEEDED(hr))
			hr= pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);

		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Enabling hardware transforms for H.264 decoding";
	}
	else
	{
		// Disable hardware transforms for uncompressed formats or if no safe decoder
		// This prevents AMD/NVIDIA video processors from being loaded
		if (SUCCEEDED(hr))
			hr= pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE);

		if (!useCompressedOutput)
		{
			MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
				<< "Disabling hardware transforms for uncompressed format (avoids vendor video processors)";
		}
	}

	m_wmfOutputFormat= preferredFormat;
	m_outputFormat= preferredFrameBufferFormat;

	// Configure the output media type by cloning the native type and changing only the subtype
	// This preserves all attributes (frame rate, interlace mode, color space, etc.)
	// Try preferred format first, fallback to alternative if not supported
	// SKIP if m_wmfOutputFormat is GUID_NULL (passthrough mode for uncompressed)
	IMFMediaType* pType= nullptr;
	bool tryFallback= false;
	bool useNativePassthrough= (m_wmfOutputFormat == GUID_NULL);

	if (SUCCEEDED(hr) && !useNativePassthrough && m_deviceFormat.device_format_index >= 0)
	{
		// Clone the native type to preserve all attributes,
		// except for the subtype to trigger decoding (e.g., H264 -> YUY2)
		pType= makeWMFMediaTypeFromSourceReader(m_pSourceReader, m_deviceFormat.device_format_index, preferredFormat);
	}
	else if (useNativePassthrough)
	{
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Using native passthrough - no output format conversion";
	}

	// Fallback: create minimal media type if we couldn't get native type by index
	if (!pType && SUCCEEDED(hr))
	{
		MIKAN_LOG_WARNING("WMFVideoFrameProcessor::init")
			<< "Could not get native type by index, creating minimal output type";

		pType= makeWMFMediaTypeFromDeviceFormatInfo(m_deviceFormat, m_wmfOutputFormat);
	}

	// Try to set the output type on the first video stream (skip if passthrough)
	if (SUCCEEDED(hr) && pType && !useNativePassthrough)
	{
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
			<< "Attempting to set output format to " << getWMFVideoFormatName(m_wmfOutputFormat) << "...";

		hr= m_pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);

		if (SUCCEEDED(hr))
		{
			MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
				<< "Successfully set output format to " << getWMFVideoFormatName(m_wmfOutputFormat);
		}
		else if (fallbackFormat != GUID_NULL)
		{
			// Preferred format failed, try fallback
			MIKAN_LOG_WARNING("WMFVideoFrameProcessor::init")
				<< "Failed to set preferred output format: " << getHresultMessage(hr) << ", trying fallback format";

			tryFallback= true;
		}
	}
	else if (useNativePassthrough)
	{
		// For passthrough, detect the native format and use it
		IMFMediaType* pNativeType= nullptr;
		HRESULT hrNative= m_pSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
															  MF_SOURCE_READER_CURRENT_TYPE_INDEX, &pNativeType);

		if (SUCCEEDED(hrNative))
		{
			GUID nativeSubtype;
			pNativeType->GetGUID(MF_MT_SUBTYPE, &nativeSubtype);

			// Update our tracked format to match native
			m_wmfOutputFormat= nativeSubtype;

			if (nativeSubtype == MFVideoFormat_YUY2)
			{
				m_outputFormat= eUSBVideoFrameBufferFormat::USBVideo_YUY2;
				MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Using native YUY2 format (passthrough)";
			}
			else if (nativeSubtype == MFVideoFormat_NV12)
			{
				m_outputFormat= eUSBVideoFrameBufferFormat::USBVideo_NV12;
				MIKAN_LOG_INFO("WMFVideoFrameProcessor::init") << "Using native NV12 format (passthrough)";
			}
			else
			{
				MIKAN_LOG_WARNING("WMFVideoFrameProcessor::init") << "Unknown native format for passthrough";
			}

			MemoryUtils::safeRelease(&pNativeType);
		}
	}

	// Try fallback format if preferred failed (skip if passthrough)
	if (tryFallback && !useNativePassthrough)
	{
		MemoryUtils::safeRelease(&pType);

		// Recreate media type with fallback format
		if (m_deviceFormat.device_format_index >= 0)
		{
			pType=
				makeWMFMediaTypeFromSourceReader(m_pSourceReader, m_deviceFormat.device_format_index, fallbackFormat);
		}

		if (SUCCEEDED(hr) && pType)
		{
			MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
				<< "Attempting to set fallback format to " << getWMFVideoFormatName(fallbackFormat) << "...";

			hr= m_pSourceReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);

			if (SUCCEEDED(hr))
			{
				// Update our tracked format to the fallback
				m_wmfOutputFormat= fallbackFormat;
				m_outputFormat= fallbackFrameBufferFormat;

				MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
					<< "Fallback successful - using " << getWMFVideoFormatName(fallbackFormat) << " output format";
			}
			else
			{
				MIKAN_LOG_ERROR("WMFVideoFrameProcessor::init")
					<< "Fallback format also failed: " << getHresultMessage(hr);
			}
		}
	}

	// Verify we actually got the format we requested
	IMFMediaType* pActualType= nullptr;
	if (SUCCEEDED(hr))
	{
		hr= m_pSourceReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pActualType);

		if (SUCCEEDED(hr))
		{
			GUID actualSubtype;
			hr= pActualType->GetGUID(MF_MT_SUBTYPE, &actualSubtype);

			if (SUCCEEDED(hr))
			{
				// Get the native media type to see what the camera actually provides
				IMFMediaType* pNativeType= nullptr;
				HRESULT hrNative=
					m_pSourceReader->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pNativeType);

				if (SUCCEEDED(hrNative))
				{
					GUID nativeSubtype;
					pNativeType->GetGUID(MF_MT_SUBTYPE, &nativeSubtype);

					MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
						<< "Native camera format: " << getWMFVideoFormatName(nativeSubtype)
						<< ", Requested output: " << getWMFVideoFormatName(m_wmfOutputFormat)
						<< ", Actual output: " << (actualSubtype == m_wmfOutputFormat ? "MATCH" : "MISMATCH");

					MemoryUtils::safeRelease(&pNativeType);
				}

				if (actualSubtype != m_wmfOutputFormat)
				{
					MIKAN_LOG_ERROR("WMFVideoFrameProcessor::init")
						<< "Failed to set desired output format. Requested: "
						<< getWMFVideoFormatName(m_wmfOutputFormat)
						<< ", but got a different format (possibly still compressed)";
					hr= E_FAIL;
				}
				else
				{
					MIKAN_LOG_INFO("WMFVideoFrameProcessor::init")
						<< "Successfully configured output format: " << getWMFVideoFormatName(m_wmfOutputFormat);
				}
			}
		}
	}

	// We're using explicit decoder in the pipeline (no manual decoder object needed)
	m_bNeedsDecoder= false;

	// Clean up
	if (FAILED(hr))
	{
		MemoryUtils::safeRelease(&m_pSourceReader);
	}

	MemoryUtils::safeRelease(&pActualType);
	MemoryUtils::safeRelease(&pType);
	MemoryUtils::safeRelease(&pAttributes);

	return hr;
}

void WMFVideoFrameProcessor::dispose()
{
	stopVideoFrameStream();

	// One release per reference held here. Driving the reader's count to zero
	// destroyed it under Media Foundation's own references and the work queue
	// thread that was still delivering into it.
	MemoryUtils::safeRelease(&m_pNativeInputType);
	MemoryUtils::safeRelease(&m_pDecoderTransform);
	MemoryUtils::safeRelease(&m_pSourceReader);

	MIKAN_LOG_INFO("WMFVideoFrameProcessor::dispose") << "Disposing video frame reader for device: " << m_deviceIndex;
}

void WMFVideoFrameProcessor::startVideoFrameStream()
{
	if (m_state == State::Stopped || m_state == State::Failed)
	{
		m_sampleIndex= 0;
		m_nv12_offsets_detected= false;
		m_nv12_uv_plane_offset= 0;
		m_state= State::Starting;

		// Request the first sample - this starts the async callback loop
		HRESULT hr= m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
												0,        // No flags
												nullptr,  // Don't need actual stream index back
												nullptr,  // Don't need flags back
												nullptr,  // Don't need timestamp back
												nullptr); // Sample will be delivered to OnReadSample()

		if (SUCCEEDED(hr))
		{
			m_state= State::Running;
			MIKAN_LOG_INFO("WMFVideoFrameProcessor::startVideoFrameStream")
				<< "Started video stream for device: " << m_deviceIndex;
		}
		else
		{
			MIKAN_LOG_ERROR("WMFVideoFrameProcessor::startVideoFrameStream")
				<< "Failed to start video stream: " << getHresultMessage(hr);
			m_state= State::Failed;
		}
	}
	else
	{
		MIKAN_LOG_WARNING("WMFVideoFrameProcessor::startVideoFrameStream")
			<< "Cannot start stream due to pending stream operation";
	}
}

void WMFVideoFrameProcessor::stopVideoFrameStream()
{
	const State state= m_state.load();
	if (state == State::Stopped)
		return;

	MIKAN_LOG_INFO("WMFVideoFrameProcessor::stopVideoFrameStream")
		<< "Stopping video frame reading on device: " << m_deviceIndex;

	// A reader with a sample request outstanding may already be dispatching
	// OnReadSample on its work queue thread. Flush is asynchronous: it cancels
	// the pending request and reports back through OnFlush, after which the
	// reader delivers nothing more for the stream. Waiting for that report is
	// what makes it safe for the caller to free anything the callback touches.
	if (m_pSourceReader != nullptr && (state == State::Running || state == State::Starting))
	{
		m_state= State::Stopping;
		{
			std::lock_guard<std::mutex> lock(m_flushMutex);
			m_bFlushComplete= false;
		}

		const HRESULT hr= m_pSourceReader->Flush((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM);
		if (SUCCEEDED(hr))
		{
			std::unique_lock<std::mutex> lock(m_flushMutex);
			if (!m_flushCondVar.wait_for(lock, std::chrono::seconds(2), [this]() { return m_bFlushComplete; }))
			{
				MIKAN_LOG_WARNING("WMFVideoFrameProcessor::stopVideoFrameStream")
					<< "Timed out waiting for the source reader flush on device: " << m_deviceIndex;
			}
		}
		else
		{
			MIKAN_LOG_WARNING("WMFVideoFrameProcessor::stopVideoFrameStream")
				<< "Source reader flush failed on device " << m_deviceIndex << ": " << getHresultMessage(hr);
		}
	}

	m_state= State::Stopped;
}

STDMETHODIMP WMFVideoFrameProcessor::OnFlush(DWORD dwStreamIndex)
{
	{
		std::lock_guard<std::mutex> lock(m_flushMutex);
		m_bFlushComplete= true;
	}
	m_flushCondVar.notify_all();

	return S_OK;
}

STDMETHODIMP WMFVideoFrameProcessor::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
												  LONGLONG llTimestamp, IMFSample* pSample)
{
	// Check if we're still running
	if (m_state != State::Running)
	{
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::OnReadSample")
			<< "Frame " << m_sampleIndex << ": State is not Running (state=" << (int)m_state.load() << ")";
		return S_OK;
	}

	// Handle errors
	if (FAILED(hrStatus))
	{
		MIKAN_LOG_ERROR("WMFVideoFrameProcessor::OnReadSample")
			<< "Read sample failed: " << getHresultMessage(hrStatus);
		m_state= State::Failed;
		return hrStatus;
	}

	// Check for end of stream
	if (dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM)
	{
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::OnReadSample") << "End of stream";
		return S_OK;
	}

	// Process the sample if we have one
	if (pSample && m_videoSourceListener)
	{
		const size_t width= m_deviceFormat.width;
		const size_t height= m_deviceFormat.height;

		// If we have a manual decoder and this is a compressed sample, decode it first
		IMFSample* pProcessedSample= pSample;
		IMFSample* pDecodedSample= nullptr;
		if (m_bNeedsDecoder && m_pDecoderTransform)
		{
			HRESULT hrDecode= processCompressedSample(pSample, &pDecodedSample);
			if (hrDecode == S_OK && pDecodedSample)
			{
				pProcessedSample= pDecodedSample;
			}
			else if (hrDecode == S_FALSE)
			{
				// Decoder needs more input - skip this frame and continue
				m_sampleIndex++;
				if (m_state == State::Running)
				{
					m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr,
												nullptr, nullptr);
				}
				else
				{
					MIKAN_LOG_WARNING("WMFVideoFrameProcessor::OnReadSample")
						<< "State is no longer Running after decode S_FALSE, stopping stream";
				}
				return S_OK;
			}
			else
			{
				MIKAN_LOG_ERROR("WMFVideoFrameProcessor::OnReadSample")
					<< "Manual decoder failed: " << getHresultMessage(hrDecode);

				// Skip this frame and continue
				m_sampleIndex++;
				if (m_state == State::Running)
				{
					m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr,
												nullptr, nullptr);
				}
				return S_OK;
			}
		}

		// Convert to contiguous buffer
		IMFMediaBuffer* pBuffer= nullptr;
		HRESULT hr= pProcessedSample->ConvertToContiguousBuffer(&pBuffer);

		if (SUCCEEDED(hr))
		{
			BYTE* pData= nullptr;
			DWORD dwBufferLength= 0;

			hr= pBuffer->Lock(&pData, nullptr, &dwBufferLength);

			if (SUCCEEDED(hr))
			{
				// Create frame buffer info
				UsbVideoFrameBuffer frameBuffer;
				frameBuffer.data= pData;
				frameBuffer.byte_count= dwBufferLength;
				frameBuffer.data_format= m_outputFormat;

				// Populate sections based on format
				if (m_outputFormat == eUSBVideoFrameBufferFormat::USBVideo_NV12)
				{
					const size_t expectedNV12Size= width * (height + height / 2);

					// Detect UV plane offset on first frame if buffer is larger than expected
					if (!m_nv12_offsets_detected && dwBufferLength > expectedNV12Size)
					{
						const size_t yPlaneSize= width * height;

						// Search for first non-zero byte after Y-plane to find UV-plane start
						bool foundUVStart= false;
						for (size_t offset= yPlaneSize; offset < dwBufferLength; offset++)
						{
							if (pData[offset] != 0)
							{
								m_nv12_uv_plane_offset= offset;
								foundUVStart= true;

								MIKAN_LOG_INFO("WMFVideoFrameProcessor::OnReadSample")
									<< "NV12 UV plane detected at offset: " << offset
									<< " (padding after Y-plane: " << (offset - yPlaneSize) << " bytes)";
								break;
							}
						}

						if (!foundUVStart)
						{
							// Fallback: assume no padding (shouldn't happen but be safe)
							m_nv12_uv_plane_offset= yPlaneSize;
							MIKAN_LOG_WARNING("WMFVideoFrameProcessor::OnReadSample")
								<< "Could not detect UV plane start (all zeros), assuming no padding";
						}

						m_nv12_offsets_detected= true;
					}
					else if (!m_nv12_offsets_detected)
					{
						// No padding detected - UV plane immediately follows Y plane
						m_nv12_uv_plane_offset= width * height;
						m_nv12_offsets_detected= true;

						MIKAN_LOG_INFO("WMFVideoFrameProcessor::OnReadSample")
							<< "NV12 format with no inter-plane padding detected";
					}

					// Populate sections for NV12 format
					// Section 0: Y plane
					frameBuffer.sections[0].pixel_width= (int)width;
					frameBuffer.sections[0].pixel_height= (int)height;
					frameBuffer.sections[0].stride= width;
					frameBuffer.sections[0].start_offset= 0;
					frameBuffer.sections[0].byte_count= width * height;

					// Section 1: UV plane (interleaved, half height)
					frameBuffer.sections[1].pixel_width= (int)width;
					frameBuffer.sections[1].pixel_height= (int)(height / 2);
					frameBuffer.sections[1].stride= width; // bytes per row
					frameBuffer.sections[1].start_offset= m_nv12_uv_plane_offset;
					frameBuffer.sections[1].byte_count= width * (height / 2);

					frameBuffer.section_count= 2;
				}
				else if (m_outputFormat == eUSBVideoFrameBufferFormat::USBVideo_YUY2)
				{
					// YUY2 format: single section, 2 bytes per pixel
					frameBuffer.sections[0].pixel_width= (int)width;
					frameBuffer.sections[0].pixel_height= (int)height;
					frameBuffer.sections[0].stride= width * 2; // bytes per row (2 bytes per pixel)
					frameBuffer.sections[0].start_offset= 0;
					frameBuffer.sections[0].byte_count= width * height * 2;

					frameBuffer.section_count= 1;
				}
				else if (m_outputFormat == eUSBVideoFrameBufferFormat::USBVideo_RGB24)
				{
					// RGB24 format: single section, 3 bytes per pixel
					frameBuffer.sections[0].pixel_width= (int)width;
					frameBuffer.sections[0].pixel_height= (int)height;
					frameBuffer.sections[0].stride= width * 3; // bytes per row (3 bytes per pixel)
					frameBuffer.sections[0].start_offset= 0;
					frameBuffer.sections[0].byte_count= width * height * 3;

					frameBuffer.section_count= 1;
				}
				else
				{
					// Unknown format: create minimal section
					frameBuffer.sections[0].pixel_width= (int)width;
					frameBuffer.sections[0].pixel_height= (int)height;
					frameBuffer.sections[0].stride= (int)width;
					frameBuffer.sections[0].start_offset= 0;
					frameBuffer.sections[0].byte_count= dwBufferLength;

					frameBuffer.section_count= 1;
				}

				// Notify listener
				m_videoSourceListener->notifyVideoFrameReceived(frameBuffer);

				pBuffer->Unlock();
			}

			MemoryUtils::safeRelease(&pBuffer);
		}

		// Clean up decoded sample if we created one
		if (pDecodedSample)
		{
			MemoryUtils::safeRelease(&pDecodedSample);
		}

		m_sampleIndex++;
	}

	// Request next sample to continue the async loop
	if (m_state == State::Running)
	{
		m_pSourceReader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, nullptr, nullptr, nullptr);
	}

	return S_OK;
}

STDMETHODIMP WMFVideoFrameProcessor::QueryInterface(REFIID riid, void** ppv)
{
	static const QITAB qit[]= {QITABENT(WMFVideoFrameProcessor, IMFSourceReaderCallback), {0}};
	return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG)
WMFVideoFrameProcessor::AddRef() { return InterlockedIncrement(&m_referenceCount); }

STDMETHODIMP_(ULONG)
WMFVideoFrameProcessor::Release()
{
	ULONG cRef= InterlockedDecrement(&m_referenceCount);
	if (cRef == 0)
	{
		delete this;
	}
	return cRef;
}

HRESULT WMFVideoFrameProcessor::processCompressedSample(IMFSample* pCompressedSample, IMFSample** ppDecodedSample)
{
	if (!m_pDecoderTransform || !pCompressedSample || !ppDecodedSample)
	{
		return E_POINTER;
	}

	*ppDecodedSample= nullptr;

	// Send compressed sample to decoder
	HRESULT hr= m_pDecoderTransform->ProcessInput(0, pCompressedSample, 0);
	bool bInputAccepted= SUCCEEDED(hr);

	if (hr == MF_E_NOTACCEPTING)
	{
		// Decoder's input buffer is full - we need to drain output first before adding more input
		// This is normal for H.264 decoders. Try to get output without adding more input.
		MIKAN_LOG_INFO("WMFVideoFrameProcessor::processCompressedSample")
			<< "Decoder input buffer full (frame " << m_sampleIndex << "), draining output";

		// Don't return error, just try to get output below
		hr= S_OK;
		bInputAccepted= false;
	}
	else if (FAILED(hr))
	{
		MIKAN_LOG_ERROR("WMFVideoFrameProcessor::processCompressedSample")
			<< "ProcessInput failed: " << getHresultMessage(hr);
		return hr;
	}
	else if (m_sampleIndex < 10)
	{
		MIKAN_LOG_DEBUG("WMFVideoFrameProcessor::processCompressedSample")
			<< "ProcessInput succeeded for frame " << m_sampleIndex;
	}

	// Check if we need to allocate output sample (most MFTs don't provide their own)
	bool bNeedOutputSample= (m_decoderOutputInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0;

	// Try to get output - may need to call multiple times as decoder can buffer frames
	// H.264 decoders often need several input frames before producing first output
	for (int attempt= 0; attempt < 10; attempt++)
	{
		// Prepare output buffer
		MFT_OUTPUT_DATA_BUFFER outputBuffer= {0};
		outputBuffer.dwStreamID= 0;
		outputBuffer.pSample= nullptr;
		outputBuffer.dwStatus= 0;
		outputBuffer.pEvents= nullptr;

		// Allocate output sample if needed
		if (bNeedOutputSample)
		{
			hr= MFCreateSample(&outputBuffer.pSample);
			if (FAILED(hr))
			{
				MIKAN_LOG_ERROR("WMFVideoFrameProcessor::processCompressedSample")
					<< "MFCreateSample failed: " << getHresultMessage(hr);
				return hr;
			}

			IMFMediaBuffer* pBuffer= nullptr;
			hr= MFCreateMemoryBuffer(m_decoderOutputInfo.cbSize, &pBuffer);
			if (SUCCEEDED(hr))
			{
				hr= outputBuffer.pSample->AddBuffer(pBuffer);
				MemoryUtils::safeRelease(&pBuffer);
			}

			if (FAILED(hr))
			{
				MIKAN_LOG_ERROR("WMFVideoFrameProcessor::processCompressedSample")
					<< "Buffer allocation failed: " << getHresultMessage(hr);
				MemoryUtils::safeRelease(&outputBuffer.pSample);
				return hr;
			}
		}

		// Get decoded output
		DWORD dwStatus= 0;
		hr= m_pDecoderTransform->ProcessOutput(0, 1, &outputBuffer, &dwStatus);

		if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			// Decoder needs more input before producing output (this is normal for first few frames)
			if (outputBuffer.pSample)
			{
				MemoryUtils::safeRelease(&outputBuffer.pSample);
			}
			if (attempt == 0 && m_sampleIndex < 10)
			{
				MIKAN_LOG_INFO("WMFVideoFrameProcessor::processCompressedSample")
					<< "Decoder buffering (frame " << m_sampleIndex << ", attempt " << attempt << ", input "
					<< (bInputAccepted ? "accepted" : "not accepted") << ")";
			}

			// If input wasn't accepted and we have no output, we're stuck
			// This shouldn't happen but log it if it does
			if (!bInputAccepted && attempt == 0)
			{
				MIKAN_LOG_WARNING("WMFVideoFrameProcessor::processCompressedSample")
					<< "Frame " << m_sampleIndex << ": Input rejected but no output available - decoder may be stuck";
			}

			return S_FALSE;
		}

		if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
		{
			// Output format changed, need to query new type and set it
			if (outputBuffer.pSample)
			{
				MemoryUtils::safeRelease(&outputBuffer.pSample);
			}

			MIKAN_LOG_INFO("WMFVideoFrameProcessor::processCompressedSample")
				<< "Stream format change detected, updating output type";

			// Query available output types
			IMFMediaType* pNewOutputType= nullptr;
			hr= m_pDecoderTransform->GetOutputAvailableType(0, 0, &pNewOutputType);
			if (SUCCEEDED(hr))
			{
				// Set the new output type
				hr= m_pDecoderTransform->SetOutputType(0, pNewOutputType, 0);
				MemoryUtils::safeRelease(&pNewOutputType);

				if (SUCCEEDED(hr))
				{
					// Update stream info with new format
					hr= m_pDecoderTransform->GetOutputStreamInfo(0, &m_decoderOutputInfo);

					if (SUCCEEDED(hr))
					{
						// Continue loop to try getting output again with new format
						continue;
					}
				}
			}

			MIKAN_LOG_ERROR("WMFVideoFrameProcessor::processCompressedSample")
				<< "Failed to handle stream change: " << getHresultMessage(hr);
			return hr;
		}

		if (SUCCEEDED(hr) && outputBuffer.pSample)
		{
			*ppDecodedSample= outputBuffer.pSample;
			if (outputBuffer.pEvents)
			{
				outputBuffer.pEvents->Release();
			}

			return S_OK;
		}

		// Some other error occurred
		if (outputBuffer.pSample)
		{
			MemoryUtils::safeRelease(&outputBuffer.pSample);
		}

		if (outputBuffer.pEvents)
		{
			outputBuffer.pEvents->Release();
		}

		MIKAN_LOG_ERROR("WMFVideoFrameProcessor::processCompressedSample")
			<< "ProcessOutput failed (attempt " << attempt << "): " << getHresultMessage(hr);
		return hr;
	}

	// Should not reach here
	return E_FAIL;
}