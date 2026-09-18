#include "AppStage.h"
#include "DMXFixtureComponent.h"
#include "DMXFixtureGroupComponent.h"
#include "DMXFixtureGroupSystem.h"
#include "DMXPresetComponent.h"
#include "DMXPresetSystem.h"
#include "IEditorWindow.h"
#include "LocText.h"
#include "Shared/PickerPropertyGui.h"
#include "MkGuiDrawUtils.h"
#include "RGBPixelGridComponent.h"
#include "RGBSpotLightComponent.h"
#include "Shared/GuiPanel_DMXPresetComponent.h"
#include "TransactionHistory.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

GuiPanel_DMXPresetComponent::GuiPanel_DMXPresetComponent(AppStage* ownerAppStage)
	: GuiPanel_MikanComponent(ownerAppStage)
	, m_groupDataSource(ownerAppStage->getProjectManager(), {{DMXFixtureGroupSystem::k_objectSystemClassName,
															  DMXFixtureGroupComponent::k_componentClassName}})
{
}

bool GuiPanel_DMXPresetComponent::init() { return initTypedPropertyInterface<DMXPresetComponent>(); }

void GuiPanel_DMXPresetComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	// The owning fixture group as a picker over the live groups
	m_entityAccessor->setPropertyRenderer(
		DMXPresetDefinition::k_groupIdPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			DMXPresetComponentPtr presetComp= getDMXPresetComponent();
			if (!presetComp)
				return false;

			m_groupDataSource.refreshEntries();
			if (PickerPropertyGui::drawEmptyPlaceholder(m_defaultGuiStyle, m_groupDataSource,
														"componentPanel.presetGroup", "componentPanel.noFixtureGroups"))
				return true;

			DMXPresetDefinitionPtr presetDef= presetComp->getDMXPresetDefinition();
			int selectedIndex= m_groupDataSource.getEntryIndexByComponentId(presetDef->getGroupId());
			if (MkGui::drawComboBoxProperty(
					m_defaultGuiStyle, presetComp->makePropertyUIIdentifier(DMXPresetDefinition::k_groupIdPropertyId),
					locText("componentPanel.presetGroup"), &m_groupDataSource, selectedIndex))
			{
				MikanComponentPtr newGroup= m_groupDataSource.getEntryAtIndex(selectedIndex);
				if (newGroup)
				{
					const MikanDMXFixtureGroupID newGroupId= newGroup->getComponentId();
					addDeferredGuiEvent([presetComp, newGroupId]()
										{ presetComp->getDMXPresetDefinition()->setGroupId(newGroupId); });
				}
			}

			return true;
		});

	// The stored channel bytes, drawn per fixture in the owning group according
	// to the fixture's own kind: a single swatch for a spot light, a grid of
	// swatches for a pixel grid, and raw sliders for anything else
	m_entityAccessor->setPropertyRenderer(
		DMXPresetDefinition::k_presetDataPropertyId,
		[this](const PropertyDescriptorConstPtr& /*desc*/) -> bool
		{
			DMXPresetComponentPtr presetComp= getDMXPresetComponent();
			if (!presetComp)
				return false;

			DMXFixtureGroupComponentPtr group= presetComp->getGroup();
			if (!group)
				return true;

			DMXPresetDefinitionPtr presetDef= presetComp->getDMXPresetDefinition();
			TransactionHistory* transactionHistory= getOwnerAppStage()->getOwnerWindow()->getTransactionHistory();

			auto bracketGesture= [transactionHistory](MikanLightID fixtureId)
			{
				if (transactionHistory == nullptr)
					return;

				if (ImGui::IsItemActive())
					transactionHistory->beginGesture("preset:" + std::to_string(fixtureId));
				else if (ImGui::IsItemDeactivated())
					transactionHistory->endGesture();
			};

			// Spot light swatches flow back to back and wrap at the panel edge;
			// a pixel grid keeps its rows and starts a new line. The fixture name
			// is a hover tooltip rather than a label, to keep the strip dense.
			const float swatchSize= ImGui::GetFrameHeight();
			bool bStripOpen= false;
			for (MikanLightID fixtureId : group->getDMXFixtureGroupDefinition()->getFixtureIds())
			{
				DMXFixtureComponentPtr fixture= group->resolveFixture(fixtureId);
				if (!fixture)
					continue;

				std::vector<uint8_t> values;
				presetDef->getFixtureValues(fixtureId, values);
				values.resize(fixture->getDMXFixtureDefinition()->getDMXChannelCount(), 0);

				const std::string componentClass= fixture->getComponentClassName();
				if (componentClass == RGBSpotLightComponent::k_componentClassName)
				{
					float rgb[3]= {values.size() > 0 ? values[0] / 255.0f : 0.0f,
								   values.size() > 1 ? values[1] / 255.0f : 0.0f,
								   values.size() > 2 ? values[2] / 255.0f : 0.0f};

					if (bStripOpen)
						MkGui::sameLineIfFits(swatchSize);
					bStripOpen= true;

					const std::string id=
						"##" + presetComp->makePropertyUIIdentifier("preset_" + std::to_string(fixtureId));
					const bool bChanged= ImGui::ColorEdit3(id.c_str(), rgb,
														   ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel
															   | ImGuiColorEditFlags_NoTooltip);
					ImGui::SetItemTooltip("%s", fixture->getName().c_str());
					if (bChanged)
					{
						const uint8_t r= (uint8_t)std::clamp((int)std::round(rgb[0] * 255.0f), 0, 255);
						const uint8_t g= (uint8_t)std::clamp((int)std::round(rgb[1] * 255.0f), 0, 255);
						const uint8_t b= (uint8_t)std::clamp((int)std::round(rgb[2] * 255.0f), 0, 255);
						addDeferredGuiEvent([presetDef, fixtureId, r, g, b]()
											{ presetDef->setFixtureChannelRange(fixtureId, 0, {r, g, b}); });
					}
					bracketGesture(fixtureId);
				}
				else if (componentClass == RGBPixelGridComponent::k_componentClassName)
				{
					RGBPixelGridComponentPtr pixelGrid= std::static_pointer_cast<RGBPixelGridComponent>(fixture);
					const int columns= pixelGrid->getRGBPixelGridDefinition()->getColumns();
					const int rows= pixelGrid->getRGBPixelGridDefinition()->getRows();
					bStripOpen= false;

					for (int row= 0; row < rows; ++row)
					{
						for (int col= 0; col < columns; ++col)
						{
							if (col > 0)
								ImGui::SameLine();

							const int wireIndex= pixelGrid->getRGBPixelGridDefinition()->getPixelWireIndex(col, row);
							const size_t offset= wireIndex >= 0 ? (size_t)wireIndex * 3 : values.size();
							float rgb[3]= {0.0f, 0.0f, 0.0f};
							if (offset + 3 <= values.size())
							{
								rgb[0]= values[offset] / 255.0f;
								rgb[1]= values[offset + 1] / 255.0f;
								rgb[2]= values[offset + 2] / 255.0f;
							}

							const std::string id= "##"
												  + presetComp->makePropertyUIIdentifier(
													  "preset_" + std::to_string(fixtureId) + "_" + std::to_string(row)
													  + "_" + std::to_string(col));
							const bool bChanged=
								ImGui::ColorEdit3(id.c_str(), rgb,
												  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel
													  | ImGuiColorEditFlags_NoTooltip);
							ImGui::SetItemTooltip("%s (%d, %d)", fixture->getName().c_str(), col, row);
							if (bChanged)
							{
								const uint8_t r= (uint8_t)std::clamp((int)std::round(rgb[0] * 255.0f), 0, 255);
								const uint8_t g= (uint8_t)std::clamp((int)std::round(rgb[1] * 255.0f), 0, 255);
								const uint8_t b= (uint8_t)std::clamp((int)std::round(rgb[2] * 255.0f), 0, 255);
								addDeferredGuiEvent(
									[presetDef, fixtureId, offset, r, g, b]()
									{ presetDef->setFixtureChannelRange(fixtureId, offset, {r, g, b}); });
							}
							bracketGesture(fixtureId);
						}
					}
				}
				else
				{
					bStripOpen= false;
					ImGui::TextUnformatted(fixture->getName().c_str());
					const std::string baseId=
						presetComp->makePropertyUIIdentifier("preset_" + std::to_string(fixtureId));
					for (int channelIndex= 0; channelIndex < (int)values.size(); ++channelIndex)
					{
						ImGui::Text("%d", channelIndex + 1);
						ImGui::SameLine();

						int channelValue= values[channelIndex];
						const std::string id= "##" + baseId + "_" + std::to_string(channelIndex);
						if (ImGui::SliderInt(id.c_str(), &channelValue, 0, 255))
						{
							const uint8_t byteValue= (uint8_t)std::clamp(channelValue, 0, 255);
							addDeferredGuiEvent(
								[presetDef, fixtureId, channelIndex, byteValue]()
								{ presetDef->setFixtureChannelRange(fixtureId, (size_t)channelIndex, {byteValue}); });
						}
						bracketGesture(fixtureId);
					}
				}
			}

			return true;
		});
}

DMXPresetComponentPtr GuiPanel_DMXPresetComponent::getDMXPresetComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
		return std::static_pointer_cast<DMXPresetComponent>(component);
	return nullptr;
}
