#include "Shared/GuiPanel_ScriptComponent.h"
#include "AppStage.h"
#include "AssetPropertyGui.h"
#include "AssetReferencePropertyMetaData.h"
#include "IconsForkAwesome.h"
#include "IEditorWindow.h"
#include "LocText.h"
#include "MikanComponent.h"
#include "MikanObjectSystem.h"
#include "MikanVariantTypes.h"
#include "MkGuiDrawUtils.h"
#include "ProjectManager.h"
#include "ScriptComponent.h"
#include "TransactionHistory.h"

#include "imgui.h"

#include <filesystem>
#include <string.h>

// -- GuiDataSource_OptionalComponentComboBox -----
GuiDataSource_OptionalComponentComboBox::GuiDataSource_OptionalComponentComboBox(
	ProjectManagerPtr projectManager, const GuiDataSource_ComboBox::SystemComponentPair& systemComponentPair)
	: m_components(projectManager, {systemComponentPair})
{
}

void GuiDataSource_OptionalComponentComboBox::setFilter(GuiDataSource_ComboBox::ComponentFilter filter)
{
	m_components.setFilter(filter);
}

void GuiDataSource_OptionalComponentComboBox::refreshEntries()
{
	m_components.refreshEntries();
	m_noneLabel= locText("componentPanel.none");
}

int GuiDataSource_OptionalComponentComboBox::getEntryIndexByComponentId(MikanComponentID componentId) const
{
	const int componentIndex= m_components.getEntryIndexByComponentId(componentId);
	return componentIndex < 0 ? 0 : componentIndex + 1;
}

MikanComponentID GuiDataSource_OptionalComponentComboBox::getComponentIdAtIndex(int index) const
{
	if (index <= 0 || index > m_components.getEntryCount())
		return INVALID_MIKAN_ID;

	MikanComponentPtr component= m_components.getEntryAtIndex(index - 1);
	return component ? component->getComponentId() : INVALID_MIKAN_ID;
}

int GuiDataSource_OptionalComponentComboBox::getEntryCount() const { return m_components.getEntryCount() + 1; }

const std::string& GuiDataSource_OptionalComponentComboBox::getEntryDisplayString(int index) const
{
	return index == 0 ? m_noneLabel : m_components.getEntryDisplayString(index - 1);
}

// -- GuiPanel_ScriptComponent -----

bool GuiPanel_ScriptComponent::init() { return initTypedPropertyInterface<ScriptComponent>(); }

void GuiPanel_ScriptComponent::onConstruct()
{
	GuiPanel_MikanComponent::onConstruct();

	m_entityAccessor->setPropertyRenderer(
		ScriptDefinition::k_scriptPathPropertyId,
		[this](const PropertyDescriptorConstPtr& desc) -> bool
		{
			ScriptComponentPtr component= getScriptComponent();
			if (!component)
				return false;

			const auto* assetMeta= desc->getMetaDataOfType<AssetReferenceFactoryMetaData>();
			if (!assetMeta)
				return false;

			ScriptDefinitionPtr definition= component->getScriptDefinition();
			const std::string scriptPath= definition->getScriptPath().generic_string();

			std::string newScriptPath;
			if (AssetPropertyGui::drawAssetReferenceProperty(
					m_defaultGuiStyle, component->makePropertyUIIdentifier(ScriptDefinition::k_scriptPathPropertyId),
					locText("componentPanel.script"), *assetMeta->getFactory(), scriptPath, newScriptPath))
			{
				addDeferredGuiEvent(
					[component, newScriptPath]()
					{ component->getScriptDefinition()->setScriptPath(std::filesystem::path(newScriptPath)); });
			}

			if (definition->hasScriptPath())
			{
				if (!component->isScriptLoaded())
				{
					ImGui::TextUnformatted(locText("componentPanel.scriptNotLoaded"));
				}

				if (MkGui::drawGlyphButtonWithLabel(
						component->makePropertyUIIdentifier(ScriptComponent::k_editScriptFunctionId), ICON_FK_PENCIL,
						locText("componentPanel.editScript")))
				{
					addDeferredGuiEvent([component]() { component->editScript(); });
				}
				if (MkGui::drawGlyphButtonWithLabel(
						component->makePropertyUIIdentifier(ScriptComponent::k_reloadScriptFunctionId), ICON_FK_REFRESH,
						locText("componentPanel.reloadScript")))
				{
					addDeferredGuiEvent([component]() { component->reloadScript(); });
				}
				if (MkGui::drawGlyphButtonWithLabel(
						component->makePropertyUIIdentifier(ScriptComponent::k_removeScriptFunctionId), ICON_FK_TRASH_O,
						locText("componentPanel.removeScript")))
				{
					addDeferredGuiEvent([component]() { component->removeScript(); });
				}
			}

			return true;
		});
}

void GuiPanel_ScriptComponent::onGui()
{
	GuiPanel_MikanComponent::onGui();

	ScriptComponentPtr component= getScriptComponent();
	if (!component)
		return;

	TransactionHistory* transactionHistory= getOwnerAppStage()->getOwnerWindow()->getTransactionHistory();

	// Script triggers as buttons. A trigger run is bracketed as one gesture so
	// its property writes coalesce; object creates and destroys still seal on
	// their own.
	std::vector<std::string> triggerNames;
	component->getTriggerNames(triggerNames);
	if (!triggerNames.empty())
	{
		ImGui::TextUnformatted(locText("componentPanel.scriptTriggers"));
		for (const std::string& triggerName : triggerNames)
		{
			if (ImGui::Button(triggerName.c_str()))
			{
				addDeferredGuiEvent(
					[component, triggerName, transactionHistory]()
					{
						if (transactionHistory != nullptr)
							transactionHistory->beginGesture("script:" + triggerName);
						component->invokeTrigger(triggerName);
						if (transactionHistory != nullptr)
							transactionHistory->endGesture();
					});
			}
		}
	}

	// HTTP triggers are fired through routes, so they are listed, not buttons
	std::vector<std::string> httpTriggerNames;
	component->getHttpTriggerNames(httpTriggerNames);
	if (!httpTriggerNames.empty())
	{
		ImGui::TextUnformatted(locText("componentPanel.scriptHttpTriggers"));
		for (const std::string& httpTriggerName : httpTriggerNames)
		{
			ImGui::BulletText("%s", httpTriggerName.c_str());
		}
	}

	// Script parameters as widgets, labeled by their field name
	std::vector<std::string> variableNames;
	component->getScriptVariableNames(variableNames);
	if (!variableNames.empty())
	{
		ImGui::TextUnformatted(locText("componentPanel.scriptVariables"));
		for (const std::string& variableName : variableNames)
		{
			drawScriptVariable(component, variableName, transactionHistory);
		}
	}
}

void GuiPanel_ScriptComponent::drawScriptVariable(ScriptComponentPtr component, const std::string& variableName,
												  TransactionHistory* transactionHistory)
{
	ScriptVariable entry;
	if (!component->getScriptVariableEntry(variableName, entry))
		return;

	if (entry.isComponentReference())
	{
		drawComponentReference(component, variableName, entry, transactionHistory);
		return;
	}

	const MikanVariant& value= entry.value;
	const std::string uiFieldId= component->makePropertyUIIdentifier("var_" + variableName);
	const char* label= variableName.c_str();

	bool bValueChanged= false;
	MikanVariant newValue= value;

	switch (value.value_type)
	{
	case MikanVariantType::BOOL:
	{
		bool v= value.getBoolValue();
		if (MkGui::drawCheckBoxProperty(m_defaultGuiStyle, uiFieldId, label, v))
		{
			newValue= v;
			bValueChanged= true;
		}
	}
	break;
	case MikanVariantType::INT:
	{
		int v= value.getIntValue();
		if (MkGui::drawIntProperty(m_defaultGuiStyle, uiFieldId, label, v))
		{
			newValue= v;
			bValueChanged= true;
		}
	}
	break;
	case MikanVariantType::FLOAT:
	{
		float v= value.getFloatValue();
		if (MkGui::drawFloatProperty(m_defaultGuiStyle, uiFieldId, label, v))
		{
			newValue= v;
			bValueChanged= true;
		}
	}
	break;
	case MikanVariantType::STRING:
	{
		char buf[256];
		strncpy_s(buf, sizeof(buf), value.getUtf8Value(), _TRUNCATE);
		if (MkGui::drawStringProperty(m_defaultGuiStyle, uiFieldId, label, buf, sizeof(buf)))
		{
			newValue= buf;
			bValueChanged= true;
		}
	}
	break;
	case MikanVariantType::VECTOR3F:
	{
		const MikanVector3f& vec= value.getVector3fValue();
		float v[3]= {vec.x, vec.y, vec.z};
		if (MkGui::drawFloat3Property(m_defaultGuiStyle, uiFieldId, label, v))
		{
			newValue= MikanVector3f{v[0], v[1], v[2]};
			bValueChanged= true;
		}
	}
	break;
	default:
		return;
	}

	// Bracket an in-progress widget edit as one transaction gesture, keeping
	// the per-frame value writes flowing for live preview
	if (transactionHistory != nullptr)
	{
		if (ImGui::IsItemActive())
		{
			transactionHistory->beginGesture("script_var:" + variableName);
		}
		else if (ImGui::IsItemDeactivated())
		{
			transactionHistory->endGesture();
		}
	}

	if (bValueChanged)
	{
		addDeferredGuiEvent([component, variableName, newValue]() mutable
							{ component->setScriptVariable(variableName, newValue); });
	}
}

void GuiPanel_ScriptComponent::drawComponentReference(ScriptComponentPtr component, const std::string& variableName,
													  const ScriptVariable& entry,
													  TransactionHistory* transactionHistory)
{
	GuiDataSource_OptionalComponentComboBox* dataSource= getComponentDataSource(entry.componentClass);
	if (dataSource == nullptr)
		return;

	dataSource->refreshEntries();

	const std::string uiFieldId= component->makePropertyUIIdentifier("var_" + variableName);
	int selectedIndex= dataSource->getEntryIndexByComponentId(entry.value.getIntValue());
	if (MkGui::drawComboBoxProperty(m_defaultGuiStyle, uiFieldId, variableName, dataSource, selectedIndex))
	{
		const MikanComponentID newId= dataSource->getComponentIdAtIndex(selectedIndex);
		addDeferredGuiEvent(
			[component, variableName, newId, transactionHistory]()
			{
				if (transactionHistory != nullptr)
					transactionHistory->beginGesture("script_var:" + variableName);
				component->setScriptVariable(variableName, MikanVariant(static_cast<int>(newId)));
				if (transactionHistory != nullptr)
					transactionHistory->endGesture();
			});
	}
}

GuiDataSource_OptionalComponentComboBox* GuiPanel_ScriptComponent::getComponentDataSource(
	const std::string& componentClass)
{
	auto it= m_componentDataSources.find(componentClass);
	if (it != m_componentDataSources.end())
		return it->second.get();

	// A typed system answers getComponentIdList only for its own component class
	ProjectManagerPtr projectManager= getOwnerAppStage()->getProjectManager();
	for (const MikanObjectSystemPtr& system : projectManager->getSystems())
	{
		std::vector<int> componentIds;
		if (system->getComponentIdList(componentClass, componentIds))
		{
			auto dataSource= std::make_unique<GuiDataSource_OptionalComponentComboBox>(
				projectManager,
				GuiDataSource_ComboBox::SystemComponentPair{system->getObjectSystemClassName(), componentClass});
			GuiDataSource_OptionalComponentComboBox* result= dataSource.get();
			m_componentDataSources[componentClass]= std::move(dataSource);
			return result;
		}
	}

	return nullptr;
}

ScriptComponentPtr GuiPanel_ScriptComponent::getScriptComponent() const
{
	MikanComponentPtr component= m_component.lock();
	if (component)
	{
		return std::static_pointer_cast<ScriptComponent>(component);
	}
	return nullptr;
}
