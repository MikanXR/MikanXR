#pragma once

#include "Shared/GuiPanel_MikanComponent.h"
#include "CompositorComponent.h"

class GuiPanel_CompositorComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_CompositorComponent(AppStage* ownerAppStage);

	virtual bool init() override;
	virtual void onConstruct() override;

protected:
	CompositorComponentPtr getCompositorComponent() const;
};

using GuiPanel_CompositorComponentPtr= std::shared_ptr<GuiPanel_CompositorComponent>;
