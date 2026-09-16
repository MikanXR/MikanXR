#include "ScriptComponent.h"
#include "AssetReferencePropertyMetaData.h"
#include "Logger.h"
#include "ProjectScriptContext.h"
#include "MikanObject.h"
#include "MikanScriptTypes.h"
#include "PathUtils.h"
#include "ScriptAssetReference.h"
#include "ScriptObjectSystem.h"

// -- ScriptDefinition -----
const std::string ScriptDefinition::k_scriptPathPropertyId= "script_path";
const std::string ScriptDefinition::k_scriptVariablesPropertyId= "script_variables";

ScriptDefinition::ScriptDefinition()
	: MikanComponentDefinition()
	, m_scriptAssetRefConfig(ScriptAssetReferenceFactory().allocateAssetReferenceConfig())
{
}

ScriptDefinition::ScriptDefinition(MikanScriptID scriptId)
	: MikanComponentDefinition(scriptId, "")
	, m_scriptAssetRefConfig(ScriptAssetReferenceFactory().allocateAssetReferenceConfig())
{
}

configuru::Config ScriptDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	if (m_scriptAssetRefConfig->isValid())
	{
		pt[k_scriptPathPropertyId]= m_scriptAssetRefConfig->writeToJSON();
	}

	if (!m_scriptVariables.empty())
	{
		pt[k_scriptVariablesPropertyId]= m_scriptVariables.writeToJSON();
	}

	return pt;
}

void ScriptDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_scriptAssetRefConfig= ScriptAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_scriptPathPropertyId))
	{
		m_scriptAssetRefConfig->readFromJSON(pt[k_scriptPathPropertyId]);
	}

	m_scriptVariables.clear();
	if (pt.has_key(k_scriptVariablesPropertyId))
	{
		m_scriptVariables.readFromJSON(pt[k_scriptVariablesPropertyId]);
	}
}

bool ScriptDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
										  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanScriptComponentValues>();
	if (componentValues)
	{
		m_scriptAssetRefConfig->assetPath= PathUtils::utf8CStrToPathString(componentValues->script_path.getUtf8Value());
	}

	return true;
}

bool ScriptDefinition::hasScriptPath() const { return !m_scriptAssetRefConfig->assetPath.empty(); }

std::filesystem::path ScriptDefinition::getScriptPath() const { return m_scriptAssetRefConfig->assetPath; }

void ScriptDefinition::setScriptPath(const std::filesystem::path& scriptPath)
{
	const std::string stored= scriptPath.empty() ? std::string() : PathUtils::makeStoredProjectPath(scriptPath);

	if (stored != m_scriptAssetRefConfig->assetPath)
	{
		m_scriptAssetRefConfig->assetPath= stored;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scriptPathPropertyId));
	}
}

bool ScriptDefinition::getScriptVariable(const std::string& name, MikanVariant& outValue) const
{
	return m_scriptVariables.get(name, outValue);
}

void ScriptDefinition::setScriptVariables(const ScriptVariableTable& table)
{
	m_scriptVariables= table;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scriptVariablesPropertyId));
}

bool ScriptDefinition::getScriptVariableOfType(const std::string& name, MikanVariantType type,
											   MikanVariant& outValue) const
{
	return m_scriptVariables.getOfType(name, type, outValue);
}

void ScriptDefinition::setScriptVariable(const std::string& name, const MikanVariant& value)
{
	if (m_scriptVariables.set(name, value))
	{
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scriptVariablesPropertyId));
	}
}

bool ScriptDefinition::getScriptComponentVariable(const std::string& name, const std::string& componentClass,
												  MikanComponentID& outComponentId) const
{
	return m_scriptVariables.getComponentId(name, componentClass, outComponentId);
}

void ScriptDefinition::setScriptComponentVariable(const std::string& name, const std::string& componentClass,
												  MikanComponentID componentId)
{
	if (m_scriptVariables.setComponentReference(name, componentClass, componentId))
	{
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_scriptVariablesPropertyId));
	}
}

// -- ScriptComponent -----
ScriptComponent::ScriptComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
	, m_scriptAssetRef(ScriptAssetReferenceFactory().allocateAssetReference())
{
}

void ScriptComponent::init()
{
	MikanComponent::init();

	m_scriptAssetRef->setAssetPath(getScriptDefinition()->getScriptPath());

	// The project context reloads on the system's next update so a script
	// added mid-frame never tears down a running state
	if (ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem())
	{
		scriptSystem->requestReload();
	}
}

void ScriptComponent::dispose()
{
	if (m_bWasDisposed)
		return;

	if (ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem())
	{
		scriptSystem->requestReload();
	}

	MikanComponent::dispose();
}

ScriptObjectSystemPtr ScriptComponent::getOwnerScriptSystem() const
{
	MikanObjectPtr ownerObject= getOwnerObject();
	if (!ownerObject)
		return nullptr;

	return std::dynamic_pointer_cast<ScriptObjectSystem>(ownerObject->getOwnerSystem());
}

std::filesystem::path ScriptComponent::getResolvedScriptPath() const
{
	return m_scriptAssetRef->getResolvedAssetPath();
}

bool ScriptComponent::isScriptLoaded() const
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	return scriptContext && scriptContext->isScriptLoaded(getComponentId());
}

void ScriptComponent::getTriggerNames(std::vector<std::string>& outNames) const
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	if (scriptContext)
	{
		scriptContext->getBehaviorTriggerNames(getComponentId(), outNames);
	}
}

void ScriptComponent::getHttpTriggerNames(std::vector<std::string>& outNames) const
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	if (scriptContext)
	{
		scriptContext->getBehaviorHttpTriggerNames(getComponentId(), outNames);
	}
}

bool ScriptComponent::hasBehaviorMethod(const std::string& methodName) const
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	return scriptContext && scriptContext->behaviorHasMethod(getComponentId(), methodName);
}

bool ScriptComponent::invokeTrigger(const std::string& triggerName)
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	return scriptContext && scriptContext->invokeScriptTrigger(triggerName, {}, getComponentId());
}

void ScriptComponent::getScriptVariableNames(std::vector<std::string>& outNames) const
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;

	if (scriptContext)
	{
		scriptContext->getVariableNamesForScript(getComponentId(), outNames);
	}
}

bool ScriptComponent::getScriptVariable(const std::string& name, MikanVariant& outValue) const
{
	return getScriptDefinition()->getScriptVariable(name, outValue);
}

bool ScriptComponent::getScriptVariableEntry(const std::string& name, ScriptVariable& outEntry) const
{
	return getScriptDefinition()->getScriptVariables().getEntry(name, outEntry);
}

bool ScriptComponent::setScriptVariable(const std::string& name, const MikanVariant& value)
{
	if (!ScriptVariableTable::isSupportedType(value.value_type))
		return false;

	getScriptDefinition()->setScriptVariable(name, value);
	return true;
}

void ScriptComponent::pushScriptVariablesToContext()
{
	ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem();
	CommonScriptContextPtr scriptContext= scriptSystem ? scriptSystem->getScriptContext() : nullptr;
	if (!scriptContext)
		return;

	// setVariableValue ignores names this script's behavior did not declare
	for (const auto& [name, entry] : getScriptDefinition()->getScriptVariables().getAll())
	{
		scriptContext->setVariableValue(getComponentId(), name, entry.value);
	}
}

void ScriptComponent::editScript()
{
	// The same path the Assets panel takes on a double-click
	if (getScriptDefinition()->hasScriptPath())
		m_scriptAssetRef->editorOpen();
}

void ScriptComponent::reloadScript()
{
	if (ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem())
	{
		scriptSystem->reloadAllScripts();
	}
}

void ScriptComponent::removeScript() { getScriptDefinition()->setScriptPath(std::filesystem::path()); }

void ScriptComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
											  const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(ScriptDefinition::k_scriptPathPropertyId))
	{
		m_scriptAssetRef->setAssetPath(getScriptDefinition()->getScriptPath());

		if (ScriptObjectSystemPtr scriptSystem= getOwnerScriptSystem())
		{
			scriptSystem->requestReload();
		}
	}

	// Panel edits, undo and redo, and automation sets all reach Lua here
	if (changedPropertySet.hasPropertyName(ScriptDefinition::k_scriptVariablesPropertyId))
	{
		pushScriptVariablesToContext();
	}
}

// -- IEntityAccessor ----
rfk::Struct const* ScriptComponent::getClientAPIValuesStructType() const
{
	return &MikanScriptComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void ScriptComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(ScriptDefinition::k_scriptPathPropertyId, MikanVariantType::STRING)
			->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
				AssetReferenceFactory::createFactory<ScriptAssetReferenceFactory>())));

	// The variable table as one JSON string: the panel draws the variables
	// itself, and the string form is what undo re-applies verbatim
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(ScriptDefinition::k_scriptVariablesPropertyId, MikanVariantType::STRING)
			->setUIHidden()
			->setClientAPIHidden());
}

bool ScriptComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == ScriptDefinition::k_scriptPathPropertyId)
	{
		outValue= getScriptDefinition()->getScriptPath();
		return true;
	}
	else if (propertyName == ScriptDefinition::k_scriptVariablesPropertyId)
	{
		outValue= getScriptDefinition()->getScriptVariables().toJsonString();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool ScriptComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == ScriptDefinition::k_scriptPathPropertyId)
	{
		if (inValue.value_type == MikanVariantType::STRING)
		{
			getScriptDefinition()->setScriptPath(std::filesystem::path(inValue.getUtf8Value()));
			return true;
		}
		else if (inValue.value_type == MikanVariantType::INVALID)
		{
			getScriptDefinition()->setScriptPath(std::filesystem::path());
			return true;
		}

		return false;
	}
	else if (propertyName == ScriptDefinition::k_scriptVariablesPropertyId)
	{
		if (inValue.value_type != MikanVariantType::STRING)
			return false;

		// Malformed text is rejected without touching the table
		ScriptVariableTable table;
		if (!table.fromJsonString(inValue.getUtf8Value()))
			return false;

		getScriptDefinition()->setScriptVariables(table);
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string ScriptComponent::k_editScriptFunctionId= "edit_script";
const std::string ScriptComponent::k_reloadScriptFunctionId= "reload_script";
const std::string ScriptComponent::k_removeScriptFunctionId= "remove_script";

void ScriptComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);

	// The script panel draws these itself, beside the path row
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_editScriptFunctionId, "Edit Script")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_reloadScriptFunctionId, "Reload Script")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_removeScriptFunctionId, "Remove Script")->setUIHidden());
}

bool ScriptComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_editScriptFunctionId)
	{
		editScript();
		return true;
	}
	else if (functionName == k_reloadScriptFunctionId)
	{
		reloadScript();
		return true;
	}
	else if (functionName == k_removeScriptFunctionId)
	{
		removeScript();
		return true;
	}

	return MikanComponent::invokeFunction(functionName);
}
