#pragma once

#include "Shared/GuiPanel_MikanComponent.h"
#include "Shared/GuiDataSource_ComboBox.h"
#include "ScriptComponent.h"

#include <map>
#include <memory>
#include <string>

// A component picker with a leading none entry, so a script's component
// reference can be cleared and an unresolved id reads as none
class GuiDataSource_OptionalComponentComboBox : public MkGui::ComboBoxDataSource
{
public:
	GuiDataSource_OptionalComponentComboBox(ProjectManagerPtr projectManager,
											const GuiDataSource_ComboBox::SystemComponentPair& systemComponentPair);

	void setFilter(GuiDataSource_ComboBox::ComponentFilter filter);
	void refreshEntries();
	// Index 0 is none; an id no entry matches also maps to none
	int getEntryIndexByComponentId(MikanComponentID componentId) const;
	MikanComponentID getComponentIdAtIndex(int index) const;

	virtual int getEntryCount() const override;
	virtual const std::string& getEntryDisplayString(int index) const override;

private:
	GuiDataSource_ComboBox m_components;
	std::string m_noneLabel;
};

class GuiPanel_ScriptComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_ScriptComponent(AppStage* ownerAppStage)
		: GuiPanel_MikanComponent(ownerAppStage)
	{
	}

	virtual bool init() override;
	virtual void onConstruct() override;
	virtual void onGui() override;

protected:
	ScriptComponentPtr getScriptComponent() const;
	void drawScriptVariable(ScriptComponentPtr component, const std::string& variableName,
							class TransactionHistory* transactionHistory);
	void drawComponentReference(ScriptComponentPtr component, const std::string& variableName,
								const ScriptVariable& entry, class TransactionHistory* transactionHistory);
	// One picker per referenced class, built on first use
	GuiDataSource_OptionalComponentComboBox* getComponentDataSource(const std::string& componentClass);

private:
	std::map<std::string, std::unique_ptr<GuiDataSource_OptionalComponentComboBox>> m_componentDataSources;
};

using GuiPanel_ScriptComponentPtr= std::shared_ptr<GuiPanel_ScriptComponent>;
