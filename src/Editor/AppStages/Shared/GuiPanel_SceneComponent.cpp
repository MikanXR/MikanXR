#include "AppStage.h"
#include "Shared/GuiPanel_SceneComponent.h"

GuiPanel_SceneComponent::GuiPanel_SceneComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
{
}

bool GuiPanel_SceneComponent::init() { return initTypedPropertyInterface<SceneComponent>(); }
