#include "AppStage.h"
#include "Shared/GuiPanel_RTPOutputComponent.h"
#include "RTPOutputComponent.h"

bool GuiPanel_RTPOutputComponent::init() { return initTypedPropertyInterface<RTPOutputComponent>(); }
