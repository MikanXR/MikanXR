#pragma once

#include "Shared/GuiPanel_MikanComponent.h"

// The property panel of a Spout output. Every row is drawn from the component's
// property descriptors.
class GuiPanel_SpoutOutputComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_SpoutOutputComponent(AppStage* ownerAppStage)
		: GuiPanel_MikanComponent(ownerAppStage)
	{
	}

	virtual bool init() override;
};
