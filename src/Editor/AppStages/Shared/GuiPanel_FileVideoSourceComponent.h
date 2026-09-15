#pragma once

#include "Shared/GuiPanel_MikanComponent.h"
#include "FileVideoSourceComponent.h"

// The path and loop properties render generically. This panel draws the
// playback status line and the scrub slider, which the generic renderer has
// no widget for, and the compact row the project outliner shows.
class GuiPanel_FileVideoSourceComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_FileVideoSourceComponent(AppStage* ownerAppStage)
		: GuiPanel_MikanComponent(ownerAppStage)
	{
	}

	void drawCompactGui();

	// -- GuiPanel_MikanComponent Interface
	virtual bool init() override;
	virtual void onConstruct() override;

protected:
	FileVideoSourceComponentPtr getFileVideoSourceComponent() const;
};
