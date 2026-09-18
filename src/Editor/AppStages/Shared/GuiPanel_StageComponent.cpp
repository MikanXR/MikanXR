#include "AppStage.h"
#include "Shared/GuiPanel_StageComponent.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MarkerTrackingVolumeComponent.h"
#include "MarkerTrackingVolumeSystem.h"
#include "MkGuiDrawUtils.h"
#include "VRTrackingVolumeComponent.h"
#include "VRTrackingVolumeSystem.h"

GuiPanel_StageComponent::GuiPanel_StageComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_trackingVolumeDataSource(
		  ownerAppStage->getProjectManager(),
		  {{VRTrackingVolumeSystem::k_objectSystemClassName, VRTrackingVolumeComponent::k_componentClassName},
		   {MarkerTrackingVolumeSystem::k_objectSystemClassName, MarkerTrackingVolumeComponent::k_componentClassName}})
{
}

bool GuiPanel_StageComponent::init() { return initTypedPropertyInterface<StageComponent>(); }

void GuiPanel_StageComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		StageComponentDefinition::k_trackingVolumeIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			StageComponentPtr stageComp= getStageComponent();
			if (!stageComp)
				return false;

			m_trackingVolumeDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_trackingVolumeDataSource,
														"componentPanel.trackingVolume",
														"componentPanel.noTrackingVolumes"))
				return true;

			const MikanTrackingVolumeID currentVolumeId=
				stageComp->getStageComponentDefinition()->getTrackingVolumeId();
			int selectedIndex= m_trackingVolumeDataSource.getEntryIndexByComponentId(currentVolumeId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					stageComp->makePropertyUIIdentifier(StageComponentDefinition::k_trackingVolumeIdPropertyId),
					locText("componentPanel.trackingVolume"), &m_trackingVolumeDataSource, selectedIndex))
			{
				MikanComponentPtr newVolume= m_trackingVolumeDataSource.getEntryAtIndex(selectedIndex);
				if (newVolume)
				{
					addDeferredGuiEvent([stageComp, newVolume]()
										{ stageComp->setTrackingVolumeId(newVolume->getComponentId()); });
				}
			}
			return true;
		});
}

StageComponentPtr GuiPanel_StageComponent::getStageComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::dynamic_pointer_cast<StageComponent>(component);
	}
	return nullptr;
}
