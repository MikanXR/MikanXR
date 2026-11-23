#include "MkGuiTheme.h"
#include "Logger.h"
#include "PathUtils.h"
#include "TextStyle.h"

#include "IconsForkAwesome.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace MkGuiTheme
{
void applyStyle()
{
	ImGui::StyleColorsDark();

	ImGuiStyle& style= ImGui::GetStyle();
	ImVec4* colors= style.Colors;

	// Backgrounds
	colors[ImGuiCol_WindowBg]= ImVec4(0.12f, 0.13f, 0.15f, 1.00f); // dark grey base
	colors[ImGuiCol_ChildBg]= ImVec4(0.14f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_PopupBg]= ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
	colors[ImGuiCol_Border]= ImVec4(0.30f, 0.33f, 0.42f, 0.40f);

	// Text
	colors[ImGuiCol_Text]= ImVec4(0.90f, 0.93f, 0.95f, 1.00f);
	colors[ImGuiCol_TextDisabled]= ImVec4(0.60f, 0.65f, 0.70f, 1.00f);

	// Headers
	colors[ImGuiCol_Header]= ImVec4(0.36f, 0.42f, 0.55f, 0.60f);
	colors[ImGuiCol_HeaderHovered]= ImVec4(0.44f, 0.50f, 0.68f, 0.80f);
	colors[ImGuiCol_HeaderActive]= ImVec4(0.46f, 0.55f, 0.75f, 1.00f);

	// Buttons
	colors[ImGuiCol_Button]= ImVec4(0.28f, 0.34f, 0.48f, 0.70f);
	colors[ImGuiCol_ButtonHovered]= ImVec4(0.36f, 0.45f, 0.65f, 0.85f);
	colors[ImGuiCol_ButtonActive]= ImVec4(0.40f, 0.50f, 0.70f, 1.00f);

	// Frames
	colors[ImGuiCol_FrameBg]= ImVec4(0.20f, 0.22f, 0.28f, 1.00f);
	colors[ImGuiCol_FrameBgHovered]= ImVec4(0.28f, 0.32f, 0.42f, 1.00f);
	colors[ImGuiCol_FrameBgActive]= ImVec4(0.32f, 0.38f, 0.50f, 1.00f);

	// Tabs
	colors[ImGuiCol_Tab]= ImVec4(0.26f, 0.30f, 0.42f, 0.80f);
	colors[ImGuiCol_TabHovered]= ImVec4(0.36f, 0.42f, 0.58f, 1.00f);
	colors[ImGuiCol_TabSelected]= ImVec4(0.42f, 0.50f, 0.68f, 1.00f);
	colors[ImGuiCol_TabDimmed]= ImVec4(0.20f, 0.24f, 0.32f, 0.80f);
	colors[ImGuiCol_TabDimmedSelected]= ImVec4(0.30f, 0.36f, 0.50f, 1.00f);

	// Titles
	colors[ImGuiCol_TitleBg]= ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
	colors[ImGuiCol_TitleBgActive]= ImVec4(0.25f, 0.30f, 0.40f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed]= ImVec4(0.10f, 0.12f, 0.15f, 0.75f);

	// Scrollbars
	colors[ImGuiCol_ScrollbarBg]= ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
	colors[ImGuiCol_ScrollbarGrab]= ImVec4(0.25f, 0.30f, 0.38f, 0.60f);
	colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.35f, 0.40f, 0.50f, 0.80f);
	colors[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.45f, 0.50f, 0.65f, 1.00f);

	// Checkboxes / radios
	colors[ImGuiCol_CheckMark]= ImVec4(0.80f, 0.85f, 1.00f, 1.00f);

	// Sliders
	colors[ImGuiCol_SliderGrab]= ImVec4(0.50f, 0.65f, 0.90f, 1.00f);
	colors[ImGuiCol_SliderGrabActive]= ImVec4(0.60f, 0.75f, 1.00f, 1.00f);

	// Resize grip
	colors[ImGuiCol_ResizeGrip]= ImVec4(0.30f, 0.40f, 0.50f, 0.60f);
	colors[ImGuiCol_ResizeGripHovered]= ImVec4(0.40f, 0.50f, 0.60f, 0.80f);
	colors[ImGuiCol_ResizeGripActive]= ImVec4(0.50f, 0.60f, 0.80f, 1.00f);

	// Separator
	colors[ImGuiCol_Separator]= ImVec4(0.35f, 0.40f, 0.48f, 0.7f);
	colors[ImGuiCol_SeparatorHovered]= ImVec4(0.50f, 0.60f, 0.72f, 0.9f);
	colors[ImGuiCol_SeparatorActive]= ImVec4(0.65f, 0.70f, 0.85f, 1.0f);

	// Menus
	colors[ImGuiCol_MenuBarBg]= ImVec4(0.14f, 0.15f, 0.17f, 1.00f);

	// Drag & drop
	colors[ImGuiCol_DragDropTarget]= ImVec4(0.50f, 0.85f, 1.00f, 0.90f);

	// Metrics
	style.WindowRounding= 8.0f;
	style.ChildRounding= 6.0f;
	style.FrameRounding= 5.0f;
	style.PopupRounding= 6.0f;
	style.ScrollbarRounding= 5.0f;
	style.GrabRounding= 4.0f;
	style.TabRounding= 5.0f;

	style.WindowBorderSize= 0.0f;
	style.FrameBorderSize= 0.0f;
	style.PopupBorderSize= 1.0f;

	style.WindowPadding= ImVec2(16, 16);
	style.FramePadding= ImVec2(10, 6);
	style.ItemSpacing= ImVec2(10, 10);
	style.ItemInnerSpacing= ImVec2(6, 4);
	style.IndentSpacing= 20.0f;
}

const ImWchar* getJapaneseGlyphRanges()
{
	// The range table is static data inside ImGui; the atlas only provides
	// access, so a local throwaway instance is fine (and needs no GL context,
	// which lets the localization self-test share these exact ranges)
	static ImFontAtlas s_rangeAtlas;
	return s_rangeAtlas.GetGlyphRangesJapanese();
}

const ImWchar* getUiGlyphRanges()
{
	static const std::vector<ImWchar> s_ranges= []
	{
		std::vector<ImWchar> ranges;
		for (const ImWchar* r= getJapaneseGlyphRanges(); *r != 0; ++r)
		{
			ranges.push_back(*r);
		}
		ranges.push_back((ImWchar)ICON_MIN_FK);
		ranges.push_back((ImWchar)ICON_MAX_FK);
		ranges.push_back(0);
		return ranges;
	}();

	return s_ranges.data();
}

static bool decodeNextCodepoint(const std::string& text, size_t& inOutIndex, unsigned int& outCodepoint)
{
	const unsigned char lead= (unsigned char)text[inOutIndex];
	size_t extraBytes= 0;
	if (lead < 0x80)
	{
		outCodepoint= lead;
	}
	else if ((lead & 0xE0) == 0xC0)
	{
		outCodepoint= lead & 0x1F;
		extraBytes= 1;
	}
	else if ((lead & 0xF0) == 0xE0)
	{
		outCodepoint= lead & 0x0F;
		extraBytes= 2;
	}
	else if ((lead & 0xF8) == 0xF0)
	{
		outCodepoint= lead & 0x07;
		extraBytes= 3;
	}
	else
	{
		return false;
	}

	for (size_t byteIndex= 0; byteIndex < extraBytes; ++byteIndex)
	{
		++inOutIndex;
		if (inOutIndex >= text.size() || ((unsigned char)text[inOutIndex] & 0xC0) != 0x80)
			return false;
		outCodepoint= (outCodepoint << 6) | ((unsigned char)text[inOutIndex] & 0x3F);
	}
	++inOutIndex;
	return true;
}

bool isTextRenderableWithUiGlyphs(const std::string& text, unsigned int* outBadCodepoint)
{
	const ImWchar* ranges= getUiGlyphRanges();

	size_t byteIndex= 0;
	while (byteIndex < text.size())
	{
		unsigned int codepoint= 0;
		if (!decodeNextCodepoint(text, byteIndex, codepoint))
		{
			if (outBadCodepoint != nullptr)
				*outBadCodepoint= 0;
			return false;
		}

		if (codepoint < 0x20)
			continue; // control characters (\n) are not glyphs

		bool bInRanges= false;
		for (const ImWchar* range= ranges; range[0] != 0; range+= 2)
		{
			if (codepoint >= range[0] && codepoint <= range[1])
			{
				bInRanges= true;
				break;
			}
		}

		if (!bInRanges)
		{
			if (outBadCodepoint != nullptr)
				*outBadCodepoint= codepoint;
			return false;
		}
	}

	return true;
}

std::string formatUiGlyphRanges()
{
	std::vector<std::pair<unsigned int, unsigned int>> sortedRanges;
	for (const ImWchar* range= getUiGlyphRanges(); range[0] != 0; range+= 2)
	{
		sortedRanges.emplace_back((unsigned int)range[0], (unsigned int)range[1]);
	}
	std::sort(sortedRanges.begin(), sortedRanges.end());

	// The Japanese table lists most kanji as one-codepoint ranges, so merging
	// neighbours is what keeps the file readable
	std::vector<std::pair<unsigned int, unsigned int>> mergedRanges;
	for (const auto& [first, last] : sortedRanges)
	{
		if (!mergedRanges.empty() && first <= mergedRanges.back().second + 1)
		{
			mergedRanges.back().second= std::max(mergedRanges.back().second, last);
		}
		else
		{
			mergedRanges.emplace_back(first, last);
		}
	}

	std::string text= "# Codepoints the UI font stack is baked with, one inclusive hex range per line.\n"
					  "# Generated by MikanCmd -writeUiGlyphRanges. Do not edit by hand.\n";
	for (const auto& [first, last] : mergedRanges)
	{
		char line[32];
		snprintf(line, sizeof(line), "%04X-%04X\n", first, last);
		text+= line;
	}

	return text;
}

ImFont* loadFonts()
{
	ImGuiIO& io= ImGui::GetIO();

	ImFont* uiFont= nullptr;
	const std::filesystem::path fontPath= PathUtils::getFontPath("MochiyPopOne-Regular");
	if (std::filesystem::exists(fontPath))
	{
		// Japanese ranges: the font ships CJK glyphs, so text that carries
		// them renders instead of drawing '?' boxes
		uiFont= io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.f, nullptr, getJapaneseGlyphRanges());
	}
	else
	{
		MIKAN_LOG_WARNING("MkGuiTheme::loadFonts")
			<< "UI font not found at " << fontPath << ", using the ImGui default";
	}

	// Merge the ForkAwesome icons into the UI font so icon codepoints render
	// inline with text at whatever size the font is pushed at
	const std::filesystem::path iconFontPath= getForkAwesomeWebFontPath();
	if (std::filesystem::exists(iconFontPath))
	{
		static const ImWchar s_iconRanges[]= {ICON_MIN_FK, ICON_MAX_FK, 0};
		ImFontConfig iconsConfig;
		iconsConfig.MergeMode= (uiFont != nullptr);
		iconsConfig.PixelSnapH= true;
		ImFont* iconFont= io.Fonts->AddFontFromFileTTF(iconFontPath.string().c_str(), 16.f, &iconsConfig, s_iconRanges);
		if (uiFont == nullptr)
		{
			uiFont= iconFont;
		}
	}
	else
	{
		MIKAN_LOG_WARNING("MkGuiTheme::loadFonts") << "Icon font not found at " << iconFontPath;
	}

	return uiFont;
}
} // namespace MkGuiTheme
