#include "AppStage.h"
#include "Shared/GuiPanel_SpoutOutputComponent.h"
#include "SpoutOutputComponent.h"

bool GuiPanel_SpoutOutputComponent::init() { return initTypedPropertyInterface<SpoutOutputComponent>(); }
