#pragma once

#include "MkGuiExport.h"

#include "imgui.h" // ImWchar

#include <string>

// The editor's ImGui look: a dark pastel palette over StyleColorsDark, rounded
// metrics, and the Mochiy Pop One UI font (with Japanese glyph coverage) with
// the ForkAwesome icon font merged in. Applied once per gui context, after
// ImGui::CreateContext and before the first frame.
namespace MkGuiTheme
{
MIKAN_GUI_FUNC(void) applyStyle();
// Returns the UI font, or nullptr when no font file could be loaded (ImGui's
// built-in font is used instead)
MIKAN_GUI_FUNC(ImFont*) loadFonts();
MIKAN_GUI_FUNC(const ImWchar*) getJapaneseGlyphRanges();
// The Japanese ranges plus the ForkAwesome icon range: every codepoint the UI
// font stack is expected to cover. Usable without a GL context.
MIKAN_GUI_FUNC(const ImWchar*) getUiGlyphRanges();
// Would every codepoint of this UTF-8 text render, or would some draw as a
// tofu box? Control characters below U+0020 are ignored. On failure
// outBadCodepoint (when given) receives the first offending codepoint, or 0
// when the text is not valid UTF-8.
MIKAN_GUI_FUNC(bool) isTextRenderableWithUiGlyphs(const std::string& text, unsigned int* outBadCodepoint= nullptr);
// The UI glyph ranges as text: comment lines, then one inclusive "XXXX-YYYY"
// hex range per line, sorted, with touching ranges merged. This is the content
// of resources/font/ui_glyph_ranges.txt, which lets a tool without the font
// stack run the same coverage check.
MIKAN_GUI_FUNC(std::string) formatUiGlyphRanges();
} // namespace MkGuiTheme
