#include "AppStage.h"
#include "Shared/GuiPanel_SceneComponent.h"
#include "CompositorComponent.h"
#include "CompositorObjectSystem.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"
#include "Shared/PickerPropertyGui.h"

GuiPanel_SceneComponent::GuiPanel_SceneComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_compositorDataSource(ownerAppStage->getProjectManager(), {{CompositorObjectSystem::k_objectSystemClassName,
																   CompositorComponent::k_componentClassName}})
{
	m_compositorDataSource.setFilter(
		[this](MikanComponentPtr comp) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			if (!sceneComp)
				return false;

			auto compositor= std::static_pointer_cast<CompositorComponent>(comp);
			return compositor && compositor->getCompositorDefinition()->getOwnerSceneId() == sceneComp->getSceneId();
		});
}

bool GuiPanel_SceneComponent::init() { return initTypedPropertyInterface<SceneComponent>(); }

void GuiPanel_SceneComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		SceneComponentDefinition::k_displayCompositorIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			SceneComponentPtr sceneComp= getSceneComponent();
			if (!sceneComp)
				return false;

			m_compositorDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_compositorDataSource,
														"componentPanel.displayCompositor",
														"componentPanel.noCompositors"))
				return true;

			const MikanCompositorID currentCompositorId=
				sceneComp->getSceneComponentDefinition()->getDisplayCompositorId();
			int selectedIndex= m_compositorDataSource.getEntryIndexByComponentId(currentCompositorId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					sceneComp->makePropertyUIIdentifier(SceneComponentDefinition::k_displayCompositorIdPropertyId),
					locText("componentPanel.displayCompositor"), &m_compositorDataSource, selectedIndex))
			{
				MikanComponentPtr newCompositor= m_compositorDataSource.getEntryAtIndex(selectedIndex);
				if (newCompositor)
				{
					addDeferredGuiEvent(
						[sceneComp, newCompositor]()
						{
							sceneComp->getSceneComponentDefinition()->setDisplayCompositorId(
								newCompositor->getComponentId());
						});
				}
			}
			return true;
		});
}

SceneComponentPtr GuiPanel_SceneComponent::getSceneComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::dynamic_pointer_cast<SceneComponent>(component);
	}
	return nullptr;
}
