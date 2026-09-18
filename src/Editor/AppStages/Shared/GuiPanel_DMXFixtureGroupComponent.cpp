#include "AppStage.h"
#include "DMXFixtureComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "IconsForkAwesome.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MkGuiDrawUtils.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "Shared/GuiPanel_DMXFixtureGroupComponent.h"
#include "StageComponent.h"
#include "StageObjectSystem.h"

#include "imgui.h"

#include <algorithm>

GuiPanel_DMXFixtureGroupComponent::GuiPanel_DMXFixtureGroupComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_stageDataSource(ownerAppStage->getProjectManager(),
						{{StageObjectSystem::k_objectSystemClassName, StageComponent::k_componentClassName}})
	, m_fixtureDataSource(ownerAppStage->getProjectManager(),
						  {{RGBSpotLightSystem::k_objectSystemClassName, RGBSpotLightComponent::k_componentClassName},
						   {RGBPixelGridSystem::k_objectSystemClassName, RGBPixelGridComponent::k_componentClassName}})
{
}

bool GuiPanel_DMXFixtureGroupComponent::init() { return initTypedPropertyInterface<DMXFixtureGroupComponent>(); }

void GuiPanel_DMXFixtureGroupComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	// The owning stage as a picker over the live stages
	m_entityAccessor->setPropertyRenderer(
		DMXFixtureGroupDefinition::k_ownerStageIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			DMXFixtureGroupComponentPtr groupComp= getDMXFixtureGroupComponent();
			if (!groupComp)
				return false;

			m_stageDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_stageDataSource, "componentPanel.stage",
														"componentPanel.noStages"))
				return true;

			DMXFixtureGroupDefinitionPtr groupDef= groupComp->getDMXFixtureGroupDefinition();
			int selectedIndex= m_stageDataSource.getEntryIndexByComponentId(groupDef->getOwnerStageId());
			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle,
					groupComp->makePropertyUIIdentifier(DMXFixtureGroupDefinition::k_ownerStageIdPropertyId),
					locText("componentPanel.stage"), &m_stageDataSource, selectedIndex))
			{
				MikanComponentPtr newStage= m_stageDataSource.getEntryAtIndex(selectedIndex);
				if (newStage)
				{
					const MikanStageID newStageId= newStage->getComponentId();
					addDeferredGuiEvent([groupComp, newStageId]()
										{ groupComp->getDMXFixtureGroupDefinition()->setOwnerStageId(newStageId); });
				}
			}

			return true;
		});

	m_entityAccessor->setPropertyRenderer(
		DMXFixtureGroupDefinition::k_fixtureIdsPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			DMXFixtureGroupComponentPtr groupComp= getDMXFixtureGroupComponent();
			if (!groupComp)
				return false;

			DMXFixtureGroupDefinitionPtr groupDef= groupComp->getDMXFixtureGroupDefinition();

			ImGui::TextUnformatted(locText("componentPanel.fixtures"));

			for (MikanLightID fixtureId : groupDef->getFixtureIds())
			{
				DMXFixtureComponentPtr fixture= groupComp->resolveFixture(fixtureId);
				if (!fixture)
					continue;

				ImGui::TextUnformatted(fixture->getName().c_str());
				ImGui::SameLine();
				if (MkGui::drawGlyphButtonWithLabel(
						groupComp->makePropertyUIIdentifier("remove_fixture_" + std::to_string(fixtureId)),
						ICON_FK_TRASH_O, locText("componentPanel.removeFixture")))
				{
					addDeferredGuiEvent([groupComp, fixtureId]()
										{ groupComp->getDMXFixtureGroupDefinition()->removeFixture(fixtureId); });
				}
			}

			// The candidate picker offers fixtures on the group's own stage that
			// are not already members
			const MikanStageID stageId= groupDef->getOwnerStageId();
			m_fixtureDataSource.setFilter(
				[groupDef, stageId](MikanComponentPtr component) -> bool
				{
					DMXFixtureComponentPtr fixture= std::dynamic_pointer_cast<DMXFixtureComponent>(component);
					if (!fixture)
						return false;

					return fixture->getDMXFixtureDefinition()->getOwnerStageId() == stageId
						   && !groupDef->containsFixture(fixture->getComponentId());
				});
			m_fixtureDataSource.refreshEntries();

			if (m_fixtureDataSource.getEntryCount() > 0)
			{
				m_selectedCandidateIndex=
					std::clamp(m_selectedCandidateIndex, 0, m_fixtureDataSource.getEntryCount() - 1);

				MkGui::drawComboBoxProperty(m_defaultGuiStyle, groupComp->makePropertyUIIdentifier("add_fixture_pick"),
											locText("componentPanel.addFixture"), &m_fixtureDataSource,
											m_selectedCandidateIndex);
				// On its own row: beside the combo it runs off the panel edge
				if (MkGui::drawGlyphButtonWithLabel(groupComp->makePropertyUIIdentifier("add_fixture_button"),
													ICON_FK_PLUS, locText("componentPanel.addFixture")))
				{
					MikanComponentPtr candidate= m_fixtureDataSource.getEntryAtIndex(m_selectedCandidateIndex);
					if (candidate)
					{
						const MikanLightID candidateId= candidate->getComponentId();
						addDeferredGuiEvent([groupComp, candidateId]()
											{ groupComp->getDMXFixtureGroupDefinition()->addFixture(candidateId); });
					}
				}
			}

			return true;
		});
}

DMXFixtureGroupComponentPtr GuiPanel_DMXFixtureGroupComponent::getDMXFixtureGroupComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
		return std::static_pointer_cast<DMXFixtureGroupComponent>(component);
	return nullptr;
}
