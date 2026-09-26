#pragma once

#include "AssetFwd.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "ObjectFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "ScriptVariableTable.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// One Lua script file registered to the project. Every script's behavior
// class loads into the project's single script context, in pool order. The
// definition also owns the values of the parameters the behavior declares.
class ScriptDefinition : public MikanComponentDefinition, public IScriptVariableStore
{
public:
	ScriptDefinition();
	ScriptDefinition(MikanScriptID scriptId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanScriptID getScriptId() const { return getComponentId(); }

	static const std::string k_scriptPathPropertyId;
	bool hasScriptPath() const;
	std::filesystem::path getScriptPath() const;
	void setScriptPath(const std::filesystem::path& scriptPath);

	// Every variable edit notifies this one name, so the whole table is the
	// unit the transaction recorder captures and re-applies
	static const std::string k_scriptVariablesPropertyId;
	inline const ScriptVariableTable& getScriptVariables() const { return m_scriptVariables; }
	bool getScriptVariable(const std::string& name, MikanVariant& outValue) const;
	// Replace the whole table with one notification
	void setScriptVariables(const ScriptVariableTable& table);

	// -- IScriptVariableStore ----
	virtual bool getScriptVariableOfType(const std::string& name, MikanVariantType type,
										 MikanVariant& outValue) const override;
	virtual void setScriptVariable(const std::string& name, const MikanVariant& value) override;
	virtual bool getScriptComponentVariable(const std::string& name, const std::string& componentClass,
											MikanComponentID& outComponentId) const override;
	virtual void setScriptComponentVariable(const std::string& name, const std::string& componentClass,
											MikanComponentID componentId) override;

private:
	AssetReferenceConfigPtr m_scriptAssetRefConfig;
	ScriptVariableTable m_scriptVariables;
};

class ScriptComponent : public MikanComponent
{
public:
	ScriptComponent(MikanObjectWeakPtr owner);
	virtual void init() override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "ScriptComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	ScriptObjectSystemPtr getOwnerScriptSystem() const;
	inline ScriptDefinitionPtr getScriptDefinition() const
	{
		return std::static_pointer_cast<ScriptDefinition>(m_definition);
	}

	// The definition's path resolved against the project directory
	std::filesystem::path getResolvedScriptPath() const;
	// True once this script's behavior instance exists in the project context
	bool isScriptLoaded() const;
	// The Trigger_ and HttpTrigger_ methods of this script's behavior, without
	// their prefixes, in declaration order
	void getTriggerNames(std::vector<std::string>& outNames) const;
	void getHttpTriggerNames(std::vector<std::string>& outNames) const;
	bool hasBehaviorMethod(const std::string& methodName) const;
	// Fire one of this script's own triggers, never another component's
	bool invokeTrigger(const std::string& triggerName);

	// The parameters this script's behavior declared, in declaration order;
	// empty while the script is not loaded
	void getScriptVariableNames(std::vector<std::string>& outNames) const;
	bool getScriptVariable(const std::string& name, MikanVariant& outValue) const;
	// The stored entry with its component class, for the panel's widget choice
	bool getScriptVariableEntry(const std::string& name, ScriptVariable& outEntry) const;
	// Writes the definition; the instance field follows through onDefinitionMarkedDirty
	bool setScriptVariable(const std::string& name, const MikanVariant& value);

	void editScript();
	void reloadScript();
	void removeScript();

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_editScriptFunctionId;
	static const std::string k_reloadScriptFunctionId;
	static const std::string k_removeScriptFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

private:
	// Write every stored value this script declared into its instance field
	void pushScriptVariablesToContext();

	AssetReferencePtr m_scriptAssetRef;
};
