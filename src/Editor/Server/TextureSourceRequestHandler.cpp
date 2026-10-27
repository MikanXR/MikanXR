#include "App.h"
#include "AppStage.h"
#include "TextureSourceRequestHandler.h"
#include "MainWindow.h"
#include "MikanServer.h"
#include "MikanTextureSourceEvents.h"
#include "ServerResponseHelpers.h"
#include "TextureSourceComponent.h"

#include <functional>

using namespace std::placeholders;

// -- TextureSourceRequestHandler -- //
bool TextureSourceRequestHandler::startup(MainWindow* mainWindow) { return true; }

// Video Source Events
void TextureSourceRequestHandler::publishTextureSourceOpenedEvent()
{
	MikanTextureSourceOpenedEvent openedEvent= {};

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(openedEvent));
}

void TextureSourceRequestHandler::publishTextureSourceClosedEvent()
{
	MikanTextureSourceClosedEvent closedEvent= {};

	m_owner->publishMikanJsonEvent(mikanEventToJsonRpc(closedEvent));
}