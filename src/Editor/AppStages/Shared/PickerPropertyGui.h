#pragma once

#include "IMkGuiStyle.h"

namespace MkGui
{
class ComboBoxDataSource;
}

namespace PickerPropertyGui
{
// A picker property renderer whose data source has nothing to offer must still
// draw its row. Declining instead (returning false) falls through to the
// descriptor's generic widget, which for the id properties these pickers stand
// in front of is an editable raw int field.
//
// Returns true when the source was empty, having drawn the row as the label
// beside a disabled message. The caller returns true in turn.
bool drawEmptyPlaceholder(MkGuiStyleConstPtr style, const MkGui::ComboBoxDataSource& dataSource, const char* labelKey,
						  const char* emptyKey);
} // namespace PickerPropertyGui
