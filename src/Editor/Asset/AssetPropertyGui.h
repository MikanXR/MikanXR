#pragma once

#include "IMkGuiStyle.h"

#include <string>

class AssetReferenceFactory;

namespace AssetPropertyGui
{
// Label column, then the factory's type glyph and the asset's short name (or the
// "<None>" placeholder), with the stored path as a tooltip. The row is a drop target
// for an asset of the factory's type from either Assets panel, and a set row clears
// from its right-click menu. Returns true when a drop set outStoredPath or a clear
// emptied it.
bool drawAssetReferenceProperty(MkGuiStyleConstPtr style, const std::string& fieldName, const std::string& label,
								const AssetReferenceFactory& factory, const std::string& storedPath,
								std::string& outStoredPath);
} // namespace AssetPropertyGui
