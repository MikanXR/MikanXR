#pragma once

#include "MkGuiExport.h"
#include "IMkGuiStyle.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class IMkTexture;
using IMkTextureConstPtr= std::shared_ptr<const IMkTexture>;

namespace MkGui
{
MIKAN_GUI_FUNC(bool) drawPropertySheetHeader(MkGuiStyleConstPtr style, const std::string headerText);
MIKAN_GUI_FUNC(void) drawStaticTextProperty(MkGuiStyleConstPtr style, const std::string label, const std::string text);
MIKAN_GUI_FUNC(bool) drawCheckBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName,
										  const std::string label, bool& inout_value);
MIKAN_GUI_FUNC(bool) drawIntProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
									 int& inout_value);
MIKAN_GUI_FUNC(bool) drawFloatProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
									   float& inout_value);
MIKAN_GUI_FUNC(bool) drawFloatSliderProperty(MkGuiStyleConstPtr style, const std::string fieldName,
											 const std::string label, float& inout_value, float srcMin, float srcMax,
											 float displayMin, float displayMax);
MIKAN_GUI_FUNC(bool) drawFloat2Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
										float* inout_v);
MIKAN_GUI_FUNC(bool) drawFloat3Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
										float* inout_v);
MIKAN_GUI_FUNC(bool) drawFloat4Property(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
										float* inout_v);
MIKAN_GUI_FUNC(bool) drawStringProperty(MkGuiStyleConstPtr style, const std::string fieldName, const std::string label,
										char* buf, size_t bufSize);
MIKAN_GUI_FUNC(bool) drawSimpleComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName,
												const std::string label, const char* items, int& inout_selectedIdex);
MIKAN_GUI_FUNC(void) drawImageProperty(MkGuiStyleConstPtr style, const std::string label, IMkTextureConstPtr image);
MIKAN_GUI_FUNC(void) drawImage(IMkTextureConstPtr image, float width, float height);
MIKAN_GUI_FUNC(bool) drawImageButton(MkGuiStyleConstPtr style, const std::string& fieldName,
									 const std::string& imageName);
// A square icon-font button with its action named alongside it. A button or
// glyph size <= 0 falls back to the current row height and font size, which is
// what a property panel wants; callers pass explicit sizes for larger buttons.
MIKAN_GUI_FUNC(bool)
drawGlyphButtonWithLabel(const std::string& fieldName, const std::string& glyph, const std::string& label,
						 float buttonSize= 0.f, float glyphSize= 0.f);

// Keep the next item on the current row when it fits inside the window's
// content width, otherwise let it wrap. Call between items of a flowing strip.
MIKAN_GUI_FUNC(void) sameLineIfFits(float itemWidth);

// One list's inline rename, owned by the panel that draws the list: the row
// being edited by its own id, the text so far, and whether the field still
// has to take keyboard focus on its first frame. Each ImGui context's lists
// own their own state, so nothing here is shared across windows.
struct MIKAN_GUI_CLASS InlineRenameState
{
	int activeId= -1;
	char buffer[256]= {};
	bool bFocusPending= false;

	void begin(int id, const std::string& currentName);
	bool isEditing(int id) const { return activeId == id; }
	void end();
};

enum class eInlineRenameResult
{
	editing,
	committed,
	cancelled
};

// Draws the field where a row label would go, filling the rest of the row, and
// takes keyboard focus with the text selected on its first frame. Enter or a
// click elsewhere commits, Escape cancels. A commit leaves the trimmed text in
// the state's buffer, and one that trims to nothing reports as cancelled.
MIKAN_GUI_FUNC(eInlineRenameResult) drawInlineRenameField(InlineRenameState& state, const std::string& fieldId);

// The click-on-selected gesture that starts a rename: true on the frame a
// press that began on this already selected row releases on it without
// travelling past the drag threshold and without being a double click. Call
// it right after the row item, with the selection state from before the click.
MIKAN_GUI_FUNC(bool) isRenameClickOnSelectedItem(bool bWasSelected, int itemId, int& inout_pressedId);

class MIKAN_GUI_CLASS ComboBoxDataSource
{
public:
	virtual int getEntryCount() const= 0;
	virtual const std::string& getEntryDisplayString(int index) const= 0;

	static const char* itemGetter(void* data, int idx);
};
MIKAN_GUI_FUNC(bool) drawComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName,
										  const std::string label, ComboBoxDataSource* dataSource,
										  int& inout_selectedIdex);
MIKAN_GUI_FUNC(bool) drawEnumComboBoxProperty(MkGuiStyleConstPtr style, const std::string fieldName,
											  const std::string label, const std::vector<std::string>& entries,
											  int& inout_selectedIndex);
MIKAN_GUI_FUNC(bool) drawRadioButtonsProperty(MkGuiStyleConstPtr style, const std::string fieldName,
											  const std::string label, const std::vector<std::string>& entries,
											  int& inout_selectedIndex);

MIKAN_GUI_FUNC(const std::string&) getVariableIcon();
MIKAN_GUI_FUNC(const std::string&) getArrayIcon();

MIKAN_GUI_FUNC(ImVec4) getPinHoveredColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getBooleanColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getEnumColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getIntColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getIntVectorColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getFloatColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getFloatVectorColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getMatrixColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getPropertyColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getTextureColor(float alpha= 1.f);
MIKAN_GUI_FUNC(ImVec4) getComponentColor(float alpha= 1.f);

// Receives a drag-drop payload of the given type, invoking the callback with the
// payload bytes. The callback must copy what it needs rather than retain the
// pointer: ImGui frees its payload buffer as the drop target closes, and the
// delivery frame is the only frame the payload is handed out at all.
MIKAN_GUI_FUNC(bool)
receiveDragDropPayload(const std::string& payloadType, const std::function<void(const void*)>& onPayloadReceived);

template <class t_payload_type>
std::shared_ptr<t_payload_type> receiveTypedDragDropPayload(const std::string& PayloadType)
{
	std::shared_ptr<t_payload_type> result;

	receiveDragDropPayload(PayloadType, [&result](const void* payloadData)
						   { result= *reinterpret_cast<const std::shared_ptr<t_payload_type>*>(payloadData); });

	return result;
}
}; // namespace MkGui
