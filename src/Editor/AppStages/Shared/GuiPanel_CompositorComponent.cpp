#include "AppStage.h"
#include "Shared/GuiPanel_CompositorComponent.h"
#include "IconsForkAwesome.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include "imgui.h"
#include "AssetPropertyGui.h"
#include "AssetReferencePropertyMetaData.h"

#include <filesystem>

GuiPanel_CompositorComponent::GuiPanel_CompositorComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
{
}

bool GuiPanel_CompositorComponent::init() { return initTypedPropertyInterface<CompositorComponent>(); }

void GuiPanel_CompositorComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		CompositorDefinition::k_compositorGraphPathPropertyId,
		[this](const PropertyDescriptorConstPtr& desc) -> bool
		{
			CompositorComponentPtr compositorComp= getCompositorComponent();
			if (!compositorComp)
				return false;

			const auto* assetMeta= desc->getMetaDataOfType<AssetReferenceFactoryMetaData>();
			if (!assetMeta)
				return false;

			CompositorDefinitionPtr componentDef= compositorComp->getCompositorDefinition();
			const std::string graphPath= componentDef->getCompositorGraphPath().generic_string();

			std::string newGraphPath;
			if (AssetPropertyGui::drawAssetReferenceProperty(
					m_defaultGuiStyle,
					compositorComp->makePropertyUIIdentifier(CompositorDefinition::k_compositorGraphPathPropertyId),
					locText("componentPanel.graph"), *assetMeta->getFactory(), graphPath, newGraphPath))
			{
				addDeferredGuiEvent(
					[compositorComp, newGraphPath]()
					{ compositorComp->setCompositorGraphAssetPath(std::filesystem::path(newGraphPath)); });
			}

			if (compositorComp->hasValidCompositorGraph())
			{
				if (MkGui::drawGlyphButtonWithLabel(
						compositorComp->makePropertyUIIdentifier(CompositorComponent::k_editCompositorGraphFunctionId),
						ICON_FK_PENCIL, locText("componentPanel.editGraph")))
				{
					addDeferredGuiEvent([compositorComp]() { compositorComp->editCompositorGraph(); });
				}
				if (MkGui::drawGlyphButtonWithLabel(compositorComp->makePropertyUIIdentifier(
														CompositorComponent::k_removeCompositorGraphFunctionId),
													ICON_FK_TRASH_O, locText("componentPanel.deleteGraph")))
				{
					addDeferredGuiEvent([compositorComp]() { compositorComp->removeCompositorGraph(); });
				}
			}
			else
			{
				if (MkGui::drawGlyphButtonWithLabel(compositorComp->makePropertyUIIdentifier(
														CompositorComponent::k_addNewCompositorGraphFunctionId),
													ICON_FK_PLUS, locText("componentPanel.addGraph")))
				{
					addDeferredGuiEvent([compositorComp]() { compositorComp->addNewCompositorGraph(); });
				}
			}

			return true;
		});
}

CompositorComponentPtr GuiPanel_CompositorComponent::getCompositorComponent() const
{
	return std::static_pointer_cast<CompositorComponent>(m_component.lock());
}
