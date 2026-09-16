#pragma once

#include "DMXSequenceComponent.h"
#include "LightSystemFwd.h"
#include "Shared/GuiDataSource_ComboBox.h"
#include "Shared/GuiPanel_ScriptComponent.h"
#include "Shared/GuiPanel_MikanComponent.h"

#include <string>
#include <vector>

class GuiPanel_DMXSequenceComponent : public GuiPanel_MikanComponent
{
public:
	GuiPanel_DMXSequenceComponent(class AppStage* ownerAppStage);

	virtual bool init() override;
	virtual void onConstruct() override;

protected:
	DMXSequenceComponentPtr getDMXSequenceComponent() const;

private:
	// Suppress a property the active content source does not read, by claiming
	// it and drawing nothing. Claiming it is what keeps it off the sheet, since
	// an unclaimed property falls through to the default renderer.
	void hideUnlessContentSource(const std::string& propertyName,
								 const std::vector<eDMXSequenceContentSource>& sources);
	// A normalized RGB vector drawn as a color picker rather than three floats,
	// and hidden outside the sources that read it. One renderer does both jobs
	// because the accessor holds a single renderer per property.
	void addColorPropertyRenderer(const std::string& propertyName, const std::string& labelKey,
								  const std::vector<eDMXSequenceContentSource>& sources);

	GuiDataSource_ComboBox m_groupDataSource;
	GuiDataSource_OptionalComponentComboBox m_scriptDataSource;
};
