#pragma once

//-- includes -----
#include "AppStage.h"
#include "CaptureManifest.h"
#include "ComponentFwd.h"
#include "MkRendererFwd.h"

#include <memory>
#include <vector>

class VideoFrameDistortionView;
typedef std::shared_ptr<VideoFrameDistortionView> VideoFrameDistortionViewPtr;
class VideoSourceRecorder;

//-- definitions -----
class AppStage_VideoSourceSettings : public AppStage
{
public:
	AppStage_VideoSourceSettings(class IEditorWindow* ownerWindow);
	virtual ~AppStage_VideoSourceSettings();

	void setVideoSourceComponent(VideoSourceComponentPtr videoSourceComponent)
	{
		m_videoSourceComponent= videoSourceComponent;
	}

	virtual void enter() override;
	virtual void exit() override;
	virtual void pause() override;
	virtual void resume() override;
	virtual void update(float deltaSeconds) override;
	virtual void onGui() override;
	virtual void render(IMkViewportPtr targetViewport) override;

	static const char* APP_STAGE_NAME;

protected:
	// UI Events
	void onReturnEvent();
	void drawRecordingGui();
	void drawCaptureKindSelector();
	void drawCaptureFlow();
	float getFallbackFps() const;

	// Remote Control
	virtual bool handleRemoteControlCommand(const std::string& command, const std::vector<std::string>& parameters,
											std::vector<std::string>& outResults) override;
	bool handleGetVideoSourceComponentId(std::vector<std::string>& outResults);
	bool handleReturnRequest(std::vector<std::string>& outResults);
	bool handleCaptureStart(const std::vector<std::string>& parameters, std::vector<std::string>& outResults);
	bool handleCaptureAdvance(std::vector<std::string>& outResults);
	bool handleCaptureCancel(std::vector<std::string>& outResults);
	bool handleGetCaptureState(std::vector<std::string>& outResults);

	VideoSourceComponentWeakPtr m_videoSourceComponent;
	VideoFrameDistortionViewPtr m_videoBufferView;
	std::unique_ptr<VideoSourceRecorder> m_recorder;
	// The kind the Start Capture button records
	eCaptureKind m_selectedKind= eCaptureKind::stillphoto;
};