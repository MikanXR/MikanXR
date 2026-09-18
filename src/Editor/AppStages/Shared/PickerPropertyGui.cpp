#include "Shared/PickerPropertyGui.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"

#include "imgui.h"

namespace PickerPropertyGui
{
bool drawEmptyPlaceholder(MkGuiStyleConstPtr style, const MkGui::ComboBoxDataSource& dataSource, const char* labelKey,
						  const char* emptyKey)
{
	if (dataSource.getEntryCount() > 0)
		return false;

	ImGui::BeginDisabled(true);
	MkGui::drawStaticTextProperty(style, locText(labelKey), locText(emptyKey));
	ImGui::EndDisabled();

	return true;
}
} // namespace PickerPropertyGui
