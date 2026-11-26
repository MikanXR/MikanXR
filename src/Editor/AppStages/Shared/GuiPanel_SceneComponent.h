#pragma once

#include "Shared/GuiPanel_MikanComponent.h"
#include "SceneComponent.h"

class GuiPanel_SceneComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_SceneComponent(AppStage* ownerAppStage);

	virtual bool init() override;
};
