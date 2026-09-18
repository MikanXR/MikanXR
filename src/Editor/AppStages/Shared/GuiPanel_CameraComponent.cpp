#include "AppStage.h"
#include "Shared/GuiPanel_CameraComponent.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MkGuiDrawUtils.h"
#include "ARKitVideoSourceComponent.h"
#include "ARKitVideoSourceSystem.h"
#include "FileVideoSourceComponent.h"
#include "FileVideoSourceSystem.h"
#include "NetworkVideoSourceComponent.h"
#include "NetworkVideoSourceSystem.h"
#include "TrackingMountComponent.h"
#include "TrackingMountObjectSystem.h"
#include "USBVideoSourceComponent.h"
#include "USBVideoSourceSystem.h"

GuiPanel_CameraComponent::GuiPanel_CameraComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_videoSourceDataSource(
		  ownerAppStage->getProjectManager(),
		  {{USBVideoSourceSystem::k_objectSystemClassName, USBVideoSourceComponent::k_componentClassName},
		   {NetworkVideoSourceSystem::k_objectSystemClassName, NetworkVideoSourceComponent::k_componentClassName},
		   {ARKitVideoSourceSystem::k_objectSystemClassName, ARKitVideoSourceComponent::k_componentClassName},
		   {FileVideoSourceSystem::k_objectSystemClassName, FileVideoSourceComponent::k_componentClassName}})
	, m_trackingMountDataSource(
		  ownerAppStage->getProjectManager(),
		  {{TrackingMountObjectSystem::k_objectSystemClassName, TrackingMountComponent::k_componentClassName}})
{
}

bool GuiPanel_CameraComponent::init() { return initTypedPropertyInterface<CameraComponent>(); }

void GuiPanel_CameraComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_videoSourceIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			m_videoSourceDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_videoSourceDataSource,
														"componentPanel.videoSource", "componentPanel.noVideoSources"))
				return true;

			const MikanVideoSourceID currentVideoSourceId= cameraComp->getCameraDefinition()->getVideoSourceId();
			int selectedIndex= m_videoSourceDataSource.getEntryIndexByComponentId(currentVideoSourceId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					cameraComp->makePropertyUIIdentifier(CameraDefinition::k_videoSourceIdPropertyId),
					locText("componentPanel.videoSource"), &m_videoSourceDataSource, selectedIndex))
			{
				MikanComponentPtr newVideoSource= m_videoSourceDataSource.getEntryAtIndex(selectedIndex);
				if (newVideoSource)
				{
					addDeferredGuiEvent(
						[cameraComp, newVideoSource]()
						{ cameraComp->getCameraDefinition()->setVideoSourceId(newVideoSource->getComponentId()); });
				}
			}
			return true;
		});

	m_entityAccessor->setPropertyRenderer(
		CameraDefinition::k_trackingMountIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			CameraComponentPtr cameraComp= getCameraComponent();
			if (!cameraComp)
				return false;

			m_trackingMountDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_trackingMountDataSource,
														"componentPanel.trackingMount",
														"componentPanel.noTrackingMounts"))
				return true;

			const MikanTrackingMountID currentMountId= cameraComp->getCameraDefinition()->getTrackingMountId();
			int selectedIndex= m_trackingMountDataSource.getEntryIndexByComponentId(currentMountId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					cameraComp->makePropertyUIIdentifier(CameraDefinition::k_trackingMountIdPropertyId),
					locText("componentPanel.trackingMount"), &m_trackingMountDataSource, selectedIndex))
			{
				MikanComponentPtr newMount= m_trackingMountDataSource.getEntryAtIndex(selectedIndex);
				if (newMount)
				{
					addDeferredGuiEvent(
						[cameraComp, newMount]()
						{ cameraComp->getCameraDefinition()->setTrackingMountId(newMount->getComponentId()); });
				}
			}
			return true;
		});
}

CameraComponentPtr GuiPanel_CameraComponent::getCameraComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::static_pointer_cast<CameraComponent>(component);
	}
	return nullptr;
}
