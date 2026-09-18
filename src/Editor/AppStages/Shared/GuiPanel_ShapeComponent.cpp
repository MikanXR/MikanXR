#include "AppStage.h"
#include "AssetPropertyGui.h"
#include "AssetReferencePropertyMetaData.h"
#include "Shared/GuiPanel_ShapeComponent.h"
#include "MkGuiDrawUtils.h"
#include "AnchorComponent.h"
#include "AnchorObjectSystem.h"
#include "GuiDataSource_ComboBox.h"
#include "IconsForkAwesome.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MikanCoreTypes.h"
#include "QuadShapeComponent.h"
#include "QuadShapeSystem.h"
#include "BoxShapeComponent.h"
#include "BoxShapeSystem.h"
#include "ModelShapeComponent.h"
#include "ModelShapeSystem.h"
#include "SceneComponent.h"
#include "SceneObjectSystem.h"
#include "IEditorWindow.h"
#include "TransactionHistory.h"
#include "TransformComponent.h"

#include <filesystem>

GuiPanel_ShapeComponent::GuiPanel_ShapeComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_parentTransformDataSource(
		  ownerAppStage->getProjectManager(),
		  {{AnchorObjectSystem::k_objectSystemClassName, AnchorComponent::k_componentClassName},
		   {SceneObjectSystem::k_objectSystemClassName, SceneComponent::k_componentClassName},
		   {QuadShapeSystem::k_objectSystemClassName, QuadShapeComponent::k_componentClassName},
		   {BoxShapeSystem::k_objectSystemClassName, BoxShapeComponent::k_componentClassName},
		   {ModelShapeSystem::k_objectSystemClassName, ModelShapeComponent::k_componentClassName}})
{
}

void GuiPanel_ShapeComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	// Parent transform picker
	m_entityAccessor->setPropertyRenderer(
		TransformComponentDefinition::k_parentTransformIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			ShapeComponentPtr shapeComponent= getShapeComponent();
			if (!shapeComponent)
				return false;

			m_parentTransformDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_parentTransformDataSource,
														"componentPanel.parent", "componentPanel.noParents"))
				return true;

			const MikanTransformID parentTransformId=
				shapeComponent->getShapeComponentDefinition()->getParentTransformId();
			int selectedIndex= m_parentTransformDataSource.getEntryIndexByComponentId(parentTransformId);

			if (MkGui::drawComboBoxProperty(m_defaultGuiStyle,
											shapeComponent->makePropertyUIIdentifier(
												TransformComponentDefinition::k_parentTransformIdPropertyId),
											locText("componentPanel.parent"), &m_parentTransformDataSource,
											selectedIndex))
			{
				auto newParent= std::dynamic_pointer_cast<TransformComponent>(
					m_parentTransformDataSource.getEntryAtIndex(selectedIndex));
				if (newParent)
				{
					TransactionHistory* transactionHistory=
						getOwnerAppStage()->getOwnerWindow()->getTransactionHistory();

					addDeferredGuiEvent(
						[shapeComponent, newParent, transactionHistory]()
						{
							// Reparenting goes through the component, not the definition: the
							// definition setter alone leaves the runtime hierarchy stale until
							// the next project load. The gesture keeps the attach and the
							// relative-transform rewrite as one undo step.
							if (transactionHistory)
								transactionHistory->beginGesture("panel_reparent");

							shapeComponent->reparentPreservingWorldTransform(newParent);

							if (transactionHistory)
								transactionHistory->endGesture();
						});
				}
			}
			return true;
		});

	// Shape node graph path
	m_entityAccessor->setPropertyRenderer(
		ShapeComponentDefinition::k_shapeGraphPathPropertyId,
		[this](const PropertyDescriptorConstPtr& desc) -> bool
		{
			ShapeComponentPtr shapeComponent= getShapeComponent();
			if (!shapeComponent)
				return false;

			const auto* assetMeta= desc->getMetaDataOfType<AssetReferenceFactoryMetaData>();
			if (!assetMeta)
				return false;

			ShapeComponentDefinitionPtr componentDef= shapeComponent->getShapeComponentDefinition();
			const std::string graphPath= componentDef->getShapeGraphPath().generic_string();

			std::string newGraphPath;
			if (AssetPropertyGui::drawAssetReferenceProperty(
					m_defaultGuiStyle,
					shapeComponent->makePropertyUIIdentifier(ShapeComponentDefinition::k_shapeGraphPathPropertyId),
					locText("componentPanel.graph"), *assetMeta->getFactory(), graphPath, newGraphPath))
			{
				addDeferredGuiEvent([shapeComponent, newGraphPath]()
									{ shapeComponent->setShapeGraphAssetPath(std::filesystem::path(newGraphPath)); });
			}

			if (shapeComponent->hasValidShapeGraph())
			{
				if (MkGui::drawGlyphButtonWithLabel(
						shapeComponent->makePropertyUIIdentifier(ShapeComponent::k_editShapeGraphFunctionId),
						ICON_FK_PENCIL, locText("componentPanel.editGraph")))
				{
					addDeferredGuiEvent([shapeComponent]() { shapeComponent->editShapeGraph(); });
				}
				if (MkGui::drawGlyphButtonWithLabel(
						shapeComponent->makePropertyUIIdentifier(ShapeComponent::k_removeShapeGraphFunctionId),
						ICON_FK_TRASH_O, locText("componentPanel.deleteGraph")))
				{
					addDeferredGuiEvent([shapeComponent]() { shapeComponent->removeShapeGraph(); });
				}
			}
			else
			{
				if (MkGui::drawGlyphButtonWithLabel(
						shapeComponent->makePropertyUIIdentifier(ShapeComponent::k_addNewShapeGraphFunctionId),
						ICON_FK_PLUS, locText("componentPanel.addGraph")))
				{
					addDeferredGuiEvent([shapeComponent]() { shapeComponent->addNewShapeGraph(); });
				}
			}

			return true;
		});
}

ShapeComponentPtr GuiPanel_ShapeComponent::getShapeComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::static_pointer_cast<ShapeComponent>(component);
	}
	return nullptr;
}

bool GuiPanel_QuadShapeComponent::init() { return initTypedPropertyInterface<QuadShapeComponent>(); }

bool GuiPanel_BoxShapeComponent::init() { return initTypedPropertyInterface<BoxShapeComponent>(); }

bool GuiPanel_ModelShapeComponent::init() { return initTypedPropertyInterface<ModelShapeComponent>(); }
