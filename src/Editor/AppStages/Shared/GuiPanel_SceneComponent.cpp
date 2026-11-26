#include "AppStage.h"
#include "Shared/GuiPanel_SceneComponent.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXPresetComponent.h"
#include "DMXPresetSystem.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

GuiPanel_SceneComponent::GuiPanel_SceneComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_cameraDataSource(ownerAppStage->getProjectManager(),
						 {{CameraObjectSystem::k_objectSystemClassName, CameraComponent::k_componentClassName}})
	, m_lightingPresetDataSource(ownerAppStage->getProjectManager(),
								 {{DMXPresetSystem::k_objectSystemClassName, DMXPresetComponent::k_componentClassName}})
{
	m_lightingPresetDataSource.setFilter(
		[this](MikanComponentPtr component) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			auto preset= std::static_pointer_cast<DMXPresetComponent>(component);
			DMXFixtureGroupComponentPtr group= preset ? preset->getGroup() : nullptr;

			return sceneComp && group
				   && group->getDMXFixtureGroupDefinition()->getOwnerStageId() == sceneComp->getParentStageId();
		});

	m_cameraDataSource.setFilter(
		[this](MikanComponentPtr component) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			auto camera= std::static_pointer_cast<CameraComponent>(component);

			return sceneComp && camera
				   && camera->getCameraDefinition()->getOwnerStageId() == sceneComp->getParentStageId();
		});
}

bool GuiPanel_SceneComponent::init() { return initTypedPropertyInterface<SceneComponent>(); }

void GuiPanel_SceneComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		SceneComponentDefinition::k_cameraIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			if (!sceneComp)
				return false;

			m_cameraDataSource.refreshEntries();

			const MikanCameraID currentCameraId= sceneComp->getSceneComponentDefinition()->getCameraId();
			int selectedIndex= m_cameraDataSource.getEntryIndexByComponentId(currentCameraId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					sceneComp->makePropertyUIIdentifier(SceneComponentDefinition::k_cameraIdPropertyId),
					locText("componentPanel.camera"), &m_cameraDataSource, selectedIndex))
			{
				if (selectedIndex >= 0)
				{
					MikanComponentPtr newCamera= m_cameraDataSource.getEntryAtIndex(selectedIndex);
					if (newCamera)
					{
						addDeferredGuiEvent(
							[sceneComp, newCamera]()
							{
								sceneComp->getSceneComponentDefinition()->setCameraId(
									(MikanCameraID)newCamera->getComponentId());
							});
					}
				}
			}
			return true;
		});

	m_entityAccessor->setPropertyRenderer(
		SceneComponentDefinition::k_lightingPresetIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			if (!sceneComp)
				return false;

			m_lightingPresetDataSource.refreshEntries();

			// A scene may have no preset, so the list leads with a none entry
			std::vector<std::string> entries;
			entries.push_back(locText("componentPanel.none"));
			for (int entryIndex= 0; entryIndex < m_lightingPresetDataSource.getEntryCount(); ++entryIndex)
			{
				entries.push_back(m_lightingPresetDataSource.getEntryDisplayString(entryIndex));
			}

			const MikanDMXPresetID currentPresetId= sceneComp->getSceneComponentDefinition()->getLightingPresetId();
			int selectedIndex= m_lightingPresetDataSource.getEntryIndexByComponentId(currentPresetId) + 1;

			if (MkGui::drawEnumComboBoxProperty(
					m_defaultGuiStyle,
					sceneComp->makePropertyUIIdentifier(SceneComponentDefinition::k_lightingPresetIdPropertyId),
					locText("componentPanel.lightingPreset"), entries, selectedIndex))
			{
				MikanComponentPtr newPreset=
					selectedIndex > 0 ? m_lightingPresetDataSource.getEntryAtIndex(selectedIndex - 1) : nullptr;
				const MikanDMXPresetID newPresetId=
					newPreset ? (MikanDMXPresetID)newPreset->getComponentId() : INVALID_MIKAN_ID;

				addDeferredGuiEvent([sceneComp, newPresetId]()
									{ sceneComp->getSceneComponentDefinition()->setLightingPresetId(newPresetId); });
			}
			return true;
		});
}

SceneComponentPtr GuiPanel_SceneComponent::getSceneComponent() const
{
	return std::dynamic_pointer_cast<SceneComponent>(m_component.lock());
}
