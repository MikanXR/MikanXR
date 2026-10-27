#include "App.h"
#include "AppStage.h"
#include "ARKitVideoSourceSystem.h"
#include "VideoSourceRequestHandler.h"
#include "MainWindow.h"
#include "MikanClientConnectionState.h"
#include "MikanServer.h"
#include "MikanVideoSourceEvents.h"
#include "MikanVideoSourceRequests.h"
#include "ProjectManager.h"
#include "ServerResponseHelpers.h"
#include "USBVideoSourceSystem.h"
#include "USBVideoSourceComponent.h"
#include "VideoSourceQueries.h"
#include "VideoSourceComponent.h"

#include <functional>

using namespace std::placeholders;

// -- VideoSourceRequestHandler -- //
bool VideoSourceRequestHandler::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	// Video Source Requests
	messageServer->setRequestHandler(
		GetVideoSourceIntrinsics::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::getVideoSourceIntrinsicsHandler, this, _1, _2));
	messageServer->setRequestHandler(GetVideoSourceMode::staticGetArchetype().getName(),
									 std::bind(&VideoSourceRequestHandler::getVideoSourceModeHandler, this, _1, _2));
	messageServer->setRequestHandler(
		GetVideoSourceAlignment::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::getVideoSourceAlignmentHandler, this, _1, _2));
	messageServer->setRequestHandler(
		SetVideoSourceAlignmentWatch::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::setVideoSourceAlignmentWatchHandler, this, _1, _2));
	messageServer->setRequestHandler(
		SetUSBVideoSourceDevice::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::setUSBVideoSourceDeviceHandler, this, _1, _2));
	messageServer->setRequestHandler(
		SetUSBVideoSourceResolution::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::setUSBVideoSourceResolutionHandler, this, _1, _2));
	messageServer->setRequestHandler(
		SetUSBVideoSourceFrameRate::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::setUSBVideoSourceFrameRateHandler, this, _1, _2));
	messageServer->setRequestHandler(
		SetUSBVideoSourceFormat::staticGetArchetype().getName(),
		std::bind(&VideoSourceRequestHandler::setUSBVideoSourceFormatHandler, this, _1, _2));

	return true;
}

// Video Source Events
void VideoSourceRequestHandler::publishVideoSourceOpenedEvent()
{
	MikanVideoSourceOpenedEvent openedEvent= {};

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(openedEvent));
}

void VideoSourceRequestHandler::publishVideoSourceClosedEvent()
{
	MikanVideoSourceClosedEvent closedEvent= {};

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(closedEvent));
}

void VideoSourceRequestHandler::publishVideoSourceModeChangedEvent()
{
	MikanVideoSourceModeChangedEvent modeChangeEvent= {};

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(modeChangeEvent));
}

void VideoSourceRequestHandler::publishVideoSourceAlignmentEvent(const MikanVideoSourceAlignmentEvent& alignmentEvent)
{
	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(alignmentEvent));
}

// Video Source Requests
void VideoSourceRequestHandler::getVideoSourceIntrinsicsHandler(const ClientRequest& request, ClientResponse& response)
{
	GetVideoSourceIntrinsics intrinsicsRequest;
	if (!readTypedRequest(request.utf8RequestString, intrinsicsRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	VideoSourceComponentPtr videoSourceComponent=
		VideoSourceQueries::getVideoSourceById(getProjectManager(), intrinsicsRequest.video_source_id);
	if (videoSourceComponent)
	{
		MikanVideoSourceIntrinsicsResponse intrinsicsResponse;
		videoSourceComponent->getCameraIntrinsics(intrinsicsResponse.intrinsics);

		writeTypedJsonResponse(request.requestId, intrinsicsResponse, response);
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}

void VideoSourceRequestHandler::getVideoSourceModeHandler(const ClientRequest& request, ClientResponse& response)
{
	GetVideoSourceMode modeRequest;
	if (!readTypedRequest(request.utf8RequestString, modeRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	VideoSourceComponentPtr videoSourceComponent=
		VideoSourceQueries::getVideoSourceById(getProjectManager(), modeRequest.video_source_id);
	if (videoSourceComponent)
	{
		const std::string devicePath= videoSourceComponent->getDevicePath();
		const std::string deviceAPI= videoSourceComponent->getDeviceAPI();

		std::string videoModeName;
		int pixelWidth, pixelHeight;
		float frameRate= 0.0f;
		if (videoSourceComponent->getVideoModeName(videoModeName)
			&& videoSourceComponent->getVideoPixelDimensions(pixelWidth, pixelHeight)
			&& videoSourceComponent->getFrameRate(frameRate))
		{
			MikanVideoSourceModeResponse info;

			info.device_path= devicePath.c_str();
			info.frame_rate= frameRate;
			info.resolution_x= pixelWidth;
			info.resolution_y= pixelHeight;
			info.video_mode_name= videoModeName.c_str();
			info.video_source_api= deviceAPI.c_str();
			// TODO: For the moment we only support monoscopic cameras.
			info.video_source_type= MikanVideoSourceType::MONO;

			writeTypedJsonResponse(request.requestId, info, response);
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}

void VideoSourceRequestHandler::getVideoSourceAlignmentHandler(const ClientRequest& request, ClientResponse& response)
{
	GetVideoSourceAlignment alignmentRequest;
	if (!readTypedRequest(request.utf8RequestString, alignmentRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	VideoSourceComponentPtr videoSourceComponent=
		VideoSourceQueries::getVideoSourceById(getProjectManager(), alignmentRequest.video_source_id);
	if (!videoSourceComponent)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
		return;
	}

	MikanVideoSourceAlignmentResponse alignmentResponse;
	alignmentResponse.video_source_id= alignmentRequest.video_source_id;

	// Only an ARKit source's poses need a stage alignment. Every other source's
	// poses are stage space by construction.
	ARKitVideoSourceSystemPtr arkitSystem= getObjectSystemOfType<ARKitVideoSourceSystem>();
	if (!arkitSystem
		|| !arkitSystem->getAlignmentState(alignmentRequest.video_source_id, alignmentResponse.is_aligned,
										   alignmentResponse.marker_visible, alignmentResponse.is_watching))
	{
		alignmentResponse.is_aligned= true;
		alignmentResponse.marker_visible= false;
		alignmentResponse.is_watching= false;
	}

	writeTypedJsonResponse(request.requestId, alignmentResponse, response);
}

void VideoSourceRequestHandler::setVideoSourceAlignmentWatchHandler(const ClientRequest& request,
																	ClientResponse& response)
{
	SetVideoSourceAlignmentWatch watchRequest;
	if (!readTypedRequest(request.utf8RequestString, watchRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	MikanClientConnectionStatePtr clientState= m_owner->getConnectedClientState(request.connectionId);
	if (!clientState)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::UnknownClient, response);
		return;
	}

	if (!VideoSourceQueries::getVideoSourceById(getProjectManager(), watchRequest.video_source_id))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
		return;
	}

	// The watch lives on the connection state, so it ends with the connection
	if (watchRequest.watch)
	{
		clientState->addAlignmentWatch(watchRequest.video_source_id);
	}
	else
	{
		clientState->removeAlignmentWatch(watchRequest.video_source_id);
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void VideoSourceRequestHandler::setUSBVideoSourceDeviceHandler(const ClientRequest& request, ClientResponse& response)
{
	SetUSBVideoSourceDevice deviceRequest;
	if (!readTypedRequest(request.utf8RequestString, deviceRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	USBVideoSourceComponentPtr videoSourceComponent=
		getObjectSystemOfType<USBVideoSourceSystem>()->getTypedComponentById(deviceRequest.video_source_id);
	if (videoSourceComponent)
	{
		if (videoSourceComponent->setDevicePath(deviceRequest.device_path.getUtf8Value()))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
			publishVideoSourceModeChangedEvent();
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}

void VideoSourceRequestHandler::setUSBVideoSourceResolutionHandler(const ClientRequest& request,
																   ClientResponse& response)
{
	SetUSBVideoSourceResolution resolutionRequest;
	if (!readTypedRequest(request.utf8RequestString, resolutionRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	USBVideoSourceComponentPtr videoSourceComponent=
		getObjectSystemOfType<USBVideoSourceSystem>()->getTypedComponentById(resolutionRequest.video_source_id);
	if (videoSourceComponent)
	{
		std::string frameRate;
		std::string format;

		videoSourceComponent->getVideoModeFrameRateName(frameRate);
		videoSourceComponent->getVideoModeFormatName(format);

		const std::string resolution= resolutionRequest.resolution.getUtf8Value();
		if (videoSourceComponent->setVideoModeToBestMatch(resolution, frameRate, format))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
			publishVideoSourceModeChangedEvent();
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}

void VideoSourceRequestHandler::setUSBVideoSourceFrameRateHandler(const ClientRequest& request,
																  ClientResponse& response)
{
	SetUSBVideoSourceFrameRate frameRateRequest;
	if (!readTypedRequest(request.utf8RequestString, frameRateRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	USBVideoSourceComponentPtr videoSourceComponent=
		getObjectSystemOfType<USBVideoSourceSystem>()->getTypedComponentById(frameRateRequest.video_source_id);
	if (videoSourceComponent)
	{
		std::string resolution;
		std::string format;

		videoSourceComponent->getVideoModeResolutionName(resolution);
		videoSourceComponent->getVideoModeFormatName(format);

		const std::string frameRate= frameRateRequest.frame_rate.getUtf8Value();
		if (videoSourceComponent->setVideoModeToBestMatch(resolution, frameRate, format))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
			publishVideoSourceModeChangedEvent();
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}

void VideoSourceRequestHandler::setUSBVideoSourceFormatHandler(const ClientRequest& request, ClientResponse& response)
{
	SetUSBVideoSourceFormat formatRequest;
	if (!readTypedRequest(request.utf8RequestString, formatRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	USBVideoSourceComponentPtr videoSourceComponent=
		getObjectSystemOfType<USBVideoSourceSystem>()->getTypedComponentById(formatRequest.video_source_id);
	if (videoSourceComponent)
	{
		const std::string format= formatRequest.format.getUtf8Value();

		std::string resolution;
		std::string frameRate;
		videoSourceComponent->getVideoModeResolutionName(resolution);
		videoSourceComponent->getVideoModeFrameRateName(frameRate);

		if (videoSourceComponent->setVideoModeToBestMatch(resolution, frameRate, format))
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
			publishVideoSourceModeChangedEvent();
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::NoVideoSource, response);
	}
}