#include "AppStage.h"
#include "Shared/GuiPanel_AnchorComponent.h"
#include "AnchorObjectSystem.h"
#include "BoxStencilComponent.h"
#include "BoxStencilSystem.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "MkGuiDrawUtils.h"
#include "QuadStencilComponent.h"
#include "QuadStencilSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "IEditorWindow.h"
#include "TransactionHistory.h"
#include "TransformComponent.h"

GuiPanel_AnchorComponent::GuiPanel_AnchorComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_parentTransformDataSource(
		  ownerAppStage->getProjectManager(),
		  {{AnchorObjectSystem::k_objectSystemClassName, AnchorComponent::k_componentClassName},
		   {SceneObjectSystem::k_objectSystemClassName, SceneComponent::k_componentClassName},
		   {QuadStencilSystem::k_objectSystemClassName, QuadStencilComponent::k_componentClassName},
		   {BoxStencilSystem::k_objectSystemClassName, BoxStencilComponent::k_componentClassName},
		   {ModelStencilSystem::k_objectSystemClassName, ModelStencilComponent::k_componentClassName}})
{
}

bool GuiPanel_AnchorComponent::init() { return initTypedPropertyInterface<AnchorComponent>(); }

void GuiPanel_AnchorComponent::onConstruct()
{
	m_entityAccessor->setPropertyRenderer(
		TransformComponentDefinition::k_parentTransformIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			AnchorComponentPtr anchorComp= std::static_pointer_cast<AnchorComponent>(getComponent());
			if (!anchorComp)
				return false;

			m_parentTransformDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_parentTransformDataSource,
														"componentPanel.parent", "componentPanel.noParents"))
				return true;

			const MikanTransformID parentTransformId= anchorComp->getParentTransformId();
			int selectedIndex= m_parentTransformDataSource.getEntryIndexByComponentId(parentTransformId);

			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					anchorComp->makePropertyUIIdentifier(TransformComponentDefinition::k_parentTransformIdPropertyId),
					locText("componentPanel.parent"), &m_parentTransformDataSource, selectedIndex))
			{
				auto newParent= std::dynamic_pointer_cast<TransformComponent>(
					m_parentTransformDataSource.getEntryAtIndex(selectedIndex));
				if (newParent)
				{
					TransactionHistory* transactionHistory=
						getOwnerAppStage()->getOwnerWindow()->getTransactionHistory();

					addDeferredGuiEvent(
						[anchorComp, newParent, transactionHistory]()
						{
							// Reparenting goes through the component, not the definition: the
							// definition setter alone leaves the runtime hierarchy stale until
							// the next project load. The gesture keeps the attach and the
							// relative-transform rewrite as one undo step.
							if (transactionHistory)
								transactionHistory->beginGesture("panel_reparent");

							anchorComp->reparentPreservingWorldTransform(newParent);

							if (transactionHistory)
								transactionHistory->endGesture();
						});
				}
			}
			return true;
		});
}
