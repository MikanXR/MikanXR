#include "App.h"
#include "AppStage.h"
#include "IRemoteControllable.h"
#include "MainWindow.h"
#include "MikanServer.h"
#include "MikanRemoteControlEvents.h"
#include "MikanRemoteControlRequests.h"
#include "RemoteControlManager.h"
#include "ServerResponseHelpers.h"

#include <functional>

using namespace std::placeholders;

// -- RemoteControlRequestHandler -- //
bool RemoteControlManager::startup(MainWindow* mainWindow)
{
	IInterprocessMessageServer* messageServer= m_owner->getMessageServer();

	// Register remote control request handlers
	messageServer->setRequestHandler(PushAppStage::staticGetArchetype().getName(),
									 std::bind(&RemoteControlManager::pushAppStageHandler, this, _1, _2));
	messageServer->setRequestHandler(PopAppStage::staticGetArchetype().getName(),
									 std::bind(&RemoteControlManager::popAppStageHandler, this, _1, _2));
	messageServer->setRequestHandler(GetAppStageInfo::staticGetArchetype().getName(),
									 std::bind(&RemoteControlManager::getAppStageInfoHandler, this, _1, _2));
	messageServer->setRequestHandler(MikanRemoteControlCommand::staticGetArchetype().getName(),
									 std::bind(&RemoteControlManager::remoteControlCommandHandler, this, _1, _2));

	m_mainWindow= mainWindow;
	m_mainWindow->OnAppStageEntered+= MakeDelegate(this, &RemoteControlManager::onAppStageEntered);
	m_mainWindow->OnAppStageExited+= MakeDelegate(this, &RemoteControlManager::onAppStageExited);

	return true;
}

void RemoteControlManager::shutdown()
{
	if (m_mainWindow != nullptr)
	{
		m_mainWindow->OnAppStageEntered-= MakeDelegate(this, &RemoteControlManager::onAppStageEntered);
		m_mainWindow->OnAppStageExited-= MakeDelegate(this, &RemoteControlManager::onAppStageExited);
		m_mainWindow= nullptr;
	}
}

void RemoteControlManager::pushAppStageHandler(const ClientRequest& request, ClientResponse& response)
{
	PushAppStage appStageRequest;
	if (!readTypedRequest(request.utf8RequestString, appStageRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	const std::string desiredAppStageName= appStageRequest.app_state_name.getUtf8Value();
	MainWindow* mainWindow= App::getInstance()->getMainWindow();
	if (mainWindow->getCurrentAppStage()->getAppStageName() != desiredAppStageName
		&& mainWindow->pushAppStage(desiredAppStageName) == nullptr)
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::RequestFailed, response);
		return;
	}

	writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
}

void RemoteControlManager::popAppStageHandler(const ClientRequest& request, ClientResponse& response)
{
	PopAppStage appStageRequest;
	if (!readTypedRequest(request.utf8RequestString, appStageRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	MainWindow* mainWindow= App::getInstance()->getMainWindow();
	if (mainWindow->getParentAppStage() != nullptr)
	{
		mainWindow->popAppState();
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::Success, response);
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		return;
	}
}

void RemoteControlManager::getAppStageInfoHandler(const ClientRequest& request, ClientResponse& response)
{
	GetAppStageInfo getAppStageRequest;
	if (!readTypedRequest(request.utf8RequestString, getAppStageRequest))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	AppStage* currentAppStage= App::getInstance()->getMainWindow()->getCurrentAppStage();

	MikanAppStageInfoResponse appStageInfoResult;
	appStageInfoResult.app_stage_info.app_state_name= currentAppStage->getAppStageName().c_str();

	writeTypedJsonResponse(request.requestId, appStageInfoResult, response);
}

void RemoteControlManager::remoteControlCommandHandler(const ClientRequest& request, ClientResponse& response)
{
	MikanRemoteControlCommand remoteControlCommand;
	if (!readTypedRequest(request.utf8RequestString, remoteControlCommand))
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::MalformedParameters, response);
		return;
	}

	// Get the current app stage and check if it is remote controllable
	MainWindow* mainWindow= App::getInstance()->getMainWindow();
	AppStage* appStage= mainWindow->getCurrentAppStage();
	auto* remoteControllableAppStage= dynamic_cast<IRemoteControllable*>(appStage);

	if (remoteControllableAppStage != nullptr)
	{
		// Pull args out of Serialization types
		const std::string command= remoteControlCommand.command.getUtf8Value();
		std::vector<std::string> parameters;
		for (const auto& parameter : remoteControlCommand.parameters)
		{
			parameters.push_back(parameter.getUtf8Value());
		}

		// Pass the command to the app stage
		// If the command is not recognized/supported, return an error
		std::vector<std::string> results;
		if (remoteControllableAppStage->handleRemoteControlCommand(command, parameters, results))
		{
			MikanRemoteControlCommandResult commandResponse= {};
			const size_t resultCount= results.size();
			if (resultCount > 0)
			{
				commandResponse.results.resize(resultCount);
				for (size_t i= 0; i < resultCount; i++)
				{
					commandResponse.results[i].setUtf8Value(results[i].c_str());
				}
			}

			writeTypedJsonResponse(request.requestId, commandResponse, response);
		}
		else
		{
			writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
		}
	}
	else
	{
		writeSimpleJsonResponse(request.requestId, MikanAPIResult::InvalidParam, response);
	}
}

// -- App Events ----
void RemoteControlManager::onAppStageEntered(AppStage* oldAppStage, AppStage* newAppStage)
{
	// If the new app stage is remote-controllable,
	// store a reference to the remote control manager on it so that post reote control events
	auto remoteControllable= dynamic_cast<IRemoteControllable*>(newAppStage);
	if (remoteControllable != nullptr)
	{
		remoteControllable->setRemoteControlManager(this);
	}

	publishAppStageChangedEvent(oldAppStage != nullptr ? oldAppStage->getAppStageName() : "",
								newAppStage != nullptr ? newAppStage->getAppStageName() : "");
}

void RemoteControlManager::onAppStageExited(AppStage* oldAppStage, AppStage* newAppStage)
{
	publishAppStageChangedEvent(oldAppStage != nullptr ? oldAppStage->getAppStageName() : "",
								newAppStage != nullptr ? newAppStage->getAppStageName() : "");
}

void RemoteControlManager::publishAppStageChangedEvent(const std::string& oldAppStageName,
													   const std::string& newAppStageName)
{
	MikanAppStageChangedEvent appStageChangedEvent= {};
	appStageChangedEvent.old_app_state_name.setUtf8Value(oldAppStageName.c_str());
	appStageChangedEvent.new_app_state_name.setUtf8Value(newAppStageName.c_str());

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(appStageChangedEvent));
}

void RemoteControlManager::sendRemoteControlEvent(const std::string& event, const std::vector<std::string>& parameters)
{
	MikanRemoteControlEvent remoteControlEvent= {};
	remoteControlEvent.remoteControlEvent.setUtf8Value(event.c_str());

	const size_t parameterCount= parameters.size();
	if (parameterCount > 0)
	{
		remoteControlEvent.parameters.resize(parameterCount);

		for (size_t i= 0; i < parameterCount; i++)
		{
			remoteControlEvent.parameters[i].setUtf8Value(parameters[i].c_str());
		}
	}

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(remoteControlEvent));
}