#pragma once

#include "ObjectSystemFwd.h"
#include "MulticastDelegate.h"
#include "MkRendererFwd.h"

#include <string>

class IEditorWindow
{
public:
	virtual bool startup()= 0;
	virtual void update(float deltaSeconds)= 0;
	virtual void render()= 0;
	virtual void shutdown()= 0;

	virtual bool getIsRenderingStage() const= 0;
	virtual IMkViewportPtr getRenderingViewport() const= 0;
	virtual const char* getTitle() const= 0;
	virtual float getWidth() const= 0;
	virtual float getHeight() const= 0;
	virtual float getAspectRatio() const= 0;
	virtual void getMouseScreenPosition(int& outScreenX, int& outScreenY) const= 0;
	virtual void getMouseDesktopPosition(int& outDesktopX, int& outDesktopY) const= 0;

	virtual class MikanModelResourceManager* getModelResourceManager()= 0;
	virtual class MikanTextureCache* getTextureCache()= 0;
	virtual ProjectManagerPtr getProjectManager() const= 0;
	virtual class MikanServer* getMikanServer() const= 0;
	virtual class TransactionHistory* getTransactionHistory() const { return nullptr; }
	virtual class ProjectAssetCatalog* getAssetCatalog() const { return nullptr; }
	virtual class IMkFontManager* getFontManager() const= 0;
	virtual class InputManager* getInputManager() const= 0;
	virtual class OpenCVManager* getOpenCVManager() const= 0;
	virtual class ClientSourceManager* getClientSourceManager() const= 0;
	virtual class LocalizationManager* getLocalizationManager() const= 0;
	virtual class EventBus* getEventBus() const= 0;
	virtual class MkGuiStyleManager* getMkGuiStyleManager() const= 0;

	virtual IMkGraphicsContextPtr getGraphicsContext() const= 0;
	virtual IMkWindowContextPtr getMkWindowContext() const= 0;
	virtual class App* getOwnerApp() const= 0;
	virtual class AppStage* getCurrentAppStage() const= 0;
	virtual class AppStage* getParentAppStage() const= 0;
	virtual class AppStage* pushAppStage(const std::string& appStageName)= 0;
	virtual void popAppState()= 0;

	template <typename t_app_stage>
	t_app_stage* pushAppStageOfType()
	{
		const std::string appStageName= t_app_stage::APP_STAGE_NAME;
		return static_cast<t_app_stage*>(pushAppStage(appStageName));
	}

	MulticastDelegate<void(class AppStage* oldAppStage, class AppStage* newAppStage)> OnAppStageEntered;
	MulticastDelegate<void(class AppStage* oldAppStage, class AppStage* newAppStage)> OnAppStageExited;
};