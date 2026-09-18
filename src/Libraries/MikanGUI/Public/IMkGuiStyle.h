#pragma once

#include "MkGuiExport.h"
#include "MkGuiStyle.h"

#include <memory>
#include <string>

class MIKAN_GUI_CLASS IMkGuiStyle
{
public:
	static constexpr int k_defaultLabelWidth= 200;
	static constexpr int k_defaultValueWidth= 150;

	virtual ~IMkGuiStyle()= default;

	virtual const std::string& getName() const= 0;
	// Both widths come back scaled for the display the current window is on, so
	// they can be used as pixel positions directly
	virtual int getLabelWidth() const= 0;
	// The value column's minimum width. A property sheet grows its value column
	// to fill the remaining panel width and falls back to this when the panel is
	// narrower than the column.
	virtual int getValueWidth() const= 0;
	virtual struct ImFont* getFont() const= 0;
	// A size <= 0 means the font's load size
	virtual float getFontSize() const= 0;

	virtual int getFloatVarCount() const= 0;
	virtual const MkGuiStyleFloatEntry& getFloatVar(int index) const= 0;

	virtual int getVec2VarCount() const= 0;
	virtual const MkGuiStyleVec2Entry& getVec2Var(int index) const= 0;

	virtual int getColorCount() const= 0;
	virtual const MkGuiStyleColorEntry& getColor(int index) const= 0;

	virtual const MkGuiStyleTextureEntry* findTexture(const std::string& name) const= 0;
};

using MkGuiStylePtr= std::shared_ptr<IMkGuiStyle>;
using MkGuiStyleConstPtr= std::shared_ptr<const IMkGuiStyle>;
