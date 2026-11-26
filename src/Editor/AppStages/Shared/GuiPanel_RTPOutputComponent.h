#pragma once

#include "Shared/GuiPanel_MikanComponent.h"

// The property panel of an RTP output. Every row is drawn from the component's
// property descriptors, the read-only status row included.
class GuiPanel_RTPOutputComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_RTPOutputComponent(AppStage* ownerAppStage)
		: GuiPanel_MikanComponent(ownerAppStage)
	{
	}

	virtual bool init() override;
};
