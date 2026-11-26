#include "AppStage.h"
#include "Shared/GuiPanel_SceneComponent.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

GuiPanel_SceneComponent::GuiPanel_SceneComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_cameraDataSource(ownerAppStage->getProjectManager(),
						 {{CameraObjectSystem::k_objectSystemClassName, CameraComponent::k_componentClassName}})
{
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
}

SceneComponentPtr GuiPanel_SceneComponent::getSceneComponent() const
{
	return std::dynamic_pointer_cast<SceneComponent>(m_component.lock());
}
