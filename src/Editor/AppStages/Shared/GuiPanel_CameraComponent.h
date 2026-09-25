#pragma once

#include "Shared/GuiPanel_MikanComponent.h"
#include "Shared/GuiDataSource_ComboBox.h"
#include "CameraComponent.h"

#include <functional>
#include <string>

class GuiPanel_CameraComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_CameraComponent(AppStage* ownerAppStage);

	virtual bool init() override;
	virtual void onConstruct() override;

protected:
	CameraComponentPtr getCameraComponent() const;

	// The slider plus the size row for one of the two client render scales. Both scales
	// draw the same pair of widgets, differing only in which property they read and write.
	bool drawClientRenderScaleProperty(const std::string& propertyId, const char* labelKey, float currentScale,
									   const std::function<void(CameraComponentPtr, float)>& applyScale);

	// The row under a scale slider reporting the size that scale actually publishes,
	// labelled by whether the buffer dimension ceiling is what decided it
	void drawClientRenderSizeRow(CameraComponentPtr cameraComp, const std::string& propertyId, float scale);

private:
	GuiDataSource_ComboBox m_videoSourceDataSource;
	GuiDataSource_ComboBox m_trackingMountDataSource;
};
