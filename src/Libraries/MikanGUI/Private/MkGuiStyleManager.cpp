#include "MkGuiStyleManager.h"
#include "MkGuiStyle.h"
#include "MkGuiContext.h"
#include "IMkTextureCache.h"
#include "PathUtils.h"
#include "Logger.h"

#include "nlohmann/json.hpp"

#include <fstream>
#include <unordered_map>

struct MkGuiStyleManager::Impl
{
	MkGuiContext* guiContext= nullptr;
	std::unordered_map<std::string, MkGuiStylePtr> styles;
};

MkGuiStyleManager::MkGuiStyleManager()
	: m_impl(new Impl())
{
}

MkGuiStyleManager::~MkGuiStyleManager() { delete m_impl; }

// A style var's ImGui enum plus whether its value is a pixel size, which decides
// if it takes the UI scale when pushed. The sizes are the ones
// ImGuiStyle::ScaleAllSizes scales; alpha and text alignment are ratios.
struct MkGuiStyleVarEntry
{
	ImGuiStyleVar var;
	bool bScalesWithDpi= true;
};

// Maps JSON var name to ImGuiStyleVar enum
static const std::unordered_map<std::string, MkGuiStyleVarEntry> k_styleFloatTable= {
	{"Alpha", {ImGuiStyleVar_Alpha, false}},
	{"DisabledAlpha", {ImGuiStyleVar_DisabledAlpha, false}},
	{"WindowRounding", {ImGuiStyleVar_WindowRounding}},
	{"WindowBorderSize", {ImGuiStyleVar_WindowBorderSize}},
	{"ChildRounding", {ImGuiStyleVar_ChildRounding}},
	{"ChildBorderSize", {ImGuiStyleVar_ChildBorderSize}},
	{"PopupRounding", {ImGuiStyleVar_PopupRounding}},
	{"PopupBorderSize", {ImGuiStyleVar_PopupBorderSize}},
	{"FrameRounding", {ImGuiStyleVar_FrameRounding}},
	{"FrameBorderSize", {ImGuiStyleVar_FrameBorderSize}},
	{"IndentSpacing", {ImGuiStyleVar_IndentSpacing}},
	{"ScrollbarSize", {ImGuiStyleVar_ScrollbarSize}},
	{"ScrollbarRounding", {ImGuiStyleVar_ScrollbarRounding}},
	{"GrabMinSize", {ImGuiStyleVar_GrabMinSize}},
	{"GrabRounding", {ImGuiStyleVar_GrabRounding}},
	{"TabRounding", {ImGuiStyleVar_TabRounding}},
};

// Maps JSON var name to ImGuiStyleVar enum
static const std::unordered_map<std::string, MkGuiStyleVarEntry> k_styleVec2Table= {
	{"WindowPadding", {ImGuiStyleVar_WindowPadding}},
	{"WindowMinSize", {ImGuiStyleVar_WindowMinSize}},
	{"WindowTitleAlign", {ImGuiStyleVar_WindowTitleAlign, false}},
	{"FramePadding", {ImGuiStyleVar_FramePadding}},
	{"ItemSpacing", {ImGuiStyleVar_ItemSpacing}},
	{"ItemInnerSpacing", {ImGuiStyleVar_ItemInnerSpacing}},
	{"CellPadding", {ImGuiStyleVar_CellPadding}},
	{"ButtonTextAlign", {ImGuiStyleVar_ButtonTextAlign, false}},
	{"SelectableTextAlign", {ImGuiStyleVar_SelectableTextAlign, false}},
};

// Maps JSON color name to ImGuiCol enum
static const std::unordered_map<std::string, ImGuiCol> k_styleColorTable= {
	{"Text", ImGuiCol_Text},
	{"TextDisabled", ImGuiCol_TextDisabled},
	{"WindowBg", ImGuiCol_WindowBg},
	{"ChildBg", ImGuiCol_ChildBg},
	{"PopupBg", ImGuiCol_PopupBg},
	{"Border", ImGuiCol_Border},
	{"BorderShadow", ImGuiCol_BorderShadow},
	{"FrameBg", ImGuiCol_FrameBg},
	{"FrameBgHovered", ImGuiCol_FrameBgHovered},
	{"FrameBgActive", ImGuiCol_FrameBgActive},
	{"TitleBg", ImGuiCol_TitleBg},
	{"TitleBgActive", ImGuiCol_TitleBgActive},
	{"TitleBgCollapsed", ImGuiCol_TitleBgCollapsed},
	{"MenuBarBg", ImGuiCol_MenuBarBg},
	{"ScrollbarBg", ImGuiCol_ScrollbarBg},
	{"ScrollbarGrab", ImGuiCol_ScrollbarGrab},
	{"ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered},
	{"ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive},
	{"CheckMark", ImGuiCol_CheckMark},
	{"SliderGrab", ImGuiCol_SliderGrab},
	{"SliderGrabActive", ImGuiCol_SliderGrabActive},
	{"Button", ImGuiCol_Button},
	{"ButtonHovered", ImGuiCol_ButtonHovered},
	{"ButtonActive", ImGuiCol_ButtonActive},
	{"Header", ImGuiCol_Header},
	{"HeaderHovered", ImGuiCol_HeaderHovered},
	{"HeaderActive", ImGuiCol_HeaderActive},
	{"Separator", ImGuiCol_Separator},
	{"SeparatorHovered", ImGuiCol_SeparatorHovered},
	{"SeparatorActive", ImGuiCol_SeparatorActive},
	{"ResizeGrip", ImGuiCol_ResizeGrip},
	{"ResizeGripHovered", ImGuiCol_ResizeGripHovered},
	{"ResizeGripActive", ImGuiCol_ResizeGripActive},
	{"Tab", ImGuiCol_Tab},
	{"TabHovered", ImGuiCol_TabHovered},
	{"TabSelected", ImGuiCol_TabSelected},
	{"TabSelectedOverline", ImGuiCol_TabSelectedOverline},
	{"TabDimmed", ImGuiCol_TabDimmed},
	{"TabDimmedSelected", ImGuiCol_TabDimmedSelected},
	{"TabDimmedSelectedOverline", ImGuiCol_TabDimmedSelectedOverline},
	// Pre-1.90.9 names kept as aliases so older style JSON keeps loading
	{"TabActive", ImGuiCol_TabSelected},
	{"TabUnfocused", ImGuiCol_TabDimmed},
	{"TabUnfocusedActive", ImGuiCol_TabDimmedSelected},
	{"PlotLines", ImGuiCol_PlotLines},
	{"PlotLinesHovered", ImGuiCol_PlotLinesHovered},
	{"PlotHistogram", ImGuiCol_PlotHistogram},
	{"PlotHistogramHovered", ImGuiCol_PlotHistogramHovered},
	{"TableHeaderBg", ImGuiCol_TableHeaderBg},
	{"TableBorderStrong", ImGuiCol_TableBorderStrong},
	{"TableBorderLight", ImGuiCol_TableBorderLight},
	{"TableRowBg", ImGuiCol_TableRowBg},
	{"TableRowBgAlt", ImGuiCol_TableRowBgAlt},
	{"TextSelectedBg", ImGuiCol_TextSelectedBg},
	{"TextLink", ImGuiCol_TextLink},
	{"TreeLines", ImGuiCol_TreeLines},
	{"InputTextCursor", ImGuiCol_InputTextCursor},
	{"DragDropTarget", ImGuiCol_DragDropTarget},
	{"NavCursor", ImGuiCol_NavCursor},
	// Pre-1.91.4 name kept as an alias so older style JSON keeps loading
	{"NavHighlight", ImGuiCol_NavCursor},
	{"NavWindowingHighlight", ImGuiCol_NavWindowingHighlight},
	{"NavWindowingDimBg", ImGuiCol_NavWindowingDimBg},
	{"ModalWindowDimBg", ImGuiCol_ModalWindowDimBg},
};

bool MkGuiStyleManager::startup(MkGuiContext* guiContext, const std::filesystem::path& stylesDir)
{
	m_impl->guiContext= guiContext;

	if (!std::filesystem::exists(stylesDir))
	{
		MIKAN_LOG_WARNING("MkGuiStyleManager::startup") << "Styles directory does not exist: " << stylesDir;
		return true; // Not a fatal error — styles are optional
	}

	bool bSuccess= true;
	for (const auto& entry : std::filesystem::directory_iterator(stylesDir))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".json")
		{
			if (!loadStyleFile(entry.path()))
			{
				bSuccess= false;
			}
		}
	}

	return bSuccess;
}

void MkGuiStyleManager::shutdown()
{
	m_impl->styles.clear();
	m_impl->guiContext= nullptr;
}

MkGuiStyleConstPtr MkGuiStyleManager::getStyle(const std::string& name) const
{
	auto it= m_impl->styles.find(name);
	if (it == m_impl->styles.end())
	{
		MIKAN_LOG_ERROR("MkGuiStyleManager::getStyle") << "Style not found: " << name;
		return nullptr;
	}
	return it->second;
}

bool MkGuiStyleManager::loadStyleFile(const std::filesystem::path& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open())
	{
		MIKAN_LOG_ERROR("MkGuiStyleManager::loadStyleFile") << "Failed to open style file: " << filePath;
		return false;
	}

	nlohmann::json root;
	try
	{
		root= nlohmann::json::parse(file);
	}
	catch (const nlohmann::json::parse_error& e)
	{
		MIKAN_LOG_ERROR("MkGuiStyleManager::loadStyleFile") << "JSON parse error in " << filePath << ": " << e.what();
		return false;
	}

	bool bSuccess= true;
	for (auto& [styleName, styleJson] : root.items())
	{
		auto style= std::make_shared<MkGuiStyle>();
		style->name()= styleName;

		// Optional font
		if (styleJson.contains("font") && styleJson["font"].is_string())
		{
			const std::string fontName= styleJson["font"].get<std::string>();

			// Both names resolve to the one UI font (icons are merged into it);
			// "big_icon" differs only by push size
			if (fontName == "normal_icon")
			{
				style->font()= m_impl->guiContext->getNormalIconFont();
			}
			else if (fontName == "big_icon")
			{
				style->font()= m_impl->guiContext->getBigIconFont();
				style->fontSize()= 22.f;
			}
		}

		// Optional explicit font size (overrides the font-name default)
		if (styleJson.contains("fontSize") && styleJson["fontSize"].is_number())
		{
			style->fontSize()= styleJson["fontSize"].get<float>();
		}

		// Optional label widths
		if (styleJson.contains("labelWidth") && styleJson["labelWidth"].is_number())
		{
			style->labelWidth()= styleJson["labelWidth"].get<int>();
		}

		// Optional value widths
		if (styleJson.contains("valueWidth") && styleJson["valueWidth"].is_number())
		{
			style->valueWidth()= styleJson["valueWidth"].get<int>();
		}

		// Style vars
		if (styleJson.contains("vars") && styleJson["vars"].is_array())
		{
			for (const auto& varJson : styleJson["vars"])
			{
				if (!varJson.contains("name") || !varJson.contains("value"))
					continue;

				const std::string varName= varJson["name"].get<std::string>();

				auto floatStyleIt= k_styleFloatTable.find(varName);
				auto vec2StyleIt= k_styleVec2Table.find(varName);

				if (floatStyleIt != k_styleFloatTable.end())
				{
					MkGuiStyleFloatEntry entry;
					entry.var= floatStyleIt->second.var;
					entry.bScalesWithDpi= floatStyleIt->second.bScalesWithDpi;

					if (varJson["value"].is_number())
					{
						entry.floatVal= varJson["value"].get<float>();
						style->floatVars().push_back(entry);
					}
					else
					{
						MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile")
							<< "Mismatched value type for var: " << varName;
					}
				}
				else if (vec2StyleIt != k_styleVec2Table.end())
				{
					MkGuiStyleVec2Entry entry;
					entry.var= vec2StyleIt->second.var;
					entry.bScalesWithDpi= vec2StyleIt->second.bScalesWithDpi;

					if (varJson["value"].is_array() && varJson["value"].size() == 2)
					{
						entry.vec2Val= {varJson["value"][0].get<float>(), varJson["value"][1].get<float>()};
						style->vec2Vars().push_back(entry);
					}
					else
					{
						MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile")
							<< "Mismatched value type for var: " << varName;
						continue;
					}
				}
				else
				{
					MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile") << "Unknown style var: " << varName;
				}
			}
		}

		// Style colors
		if (styleJson.contains("colors") && styleJson["colors"].is_array())
		{
			for (const auto& colorJson : styleJson["colors"])
			{
				if (!colorJson.contains("name") || !colorJson.contains("value"))
					continue;

				const std::string colorName= colorJson["name"].get<std::string>();
				auto it= k_styleColorTable.find(colorName);
				if (it == k_styleColorTable.end())
				{
					MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile") << "Unknown style color: " << colorName;
					continue;
				}

				if (!colorJson["value"].is_array() || colorJson["value"].size() != 4)
				{
					MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile")
						<< "Color value must be [r,g,b,a] array for: " << colorName;
					continue;
				}

				MkGuiStyleColorEntry entry;
				entry.col= it->second;
				entry.value= {colorJson["value"][0].get<float>(), colorJson["value"][1].get<float>(),
							  colorJson["value"][2].get<float>(), colorJson["value"][3].get<float>()};
				style->colors().push_back(entry);
			}
		}

		// Style textures
		if (styleJson.contains("textures") && styleJson["textures"].is_array())
		{
			IMkTextureCache* textureCache= m_impl->guiContext->getTextureCache();
			for (const auto& texJson : styleJson["textures"])
			{
				if (!texJson.contains("name") || !texJson.contains("path"))
					continue;

				const std::string texName= texJson["name"].get<std::string>();
				// Paths in JSON are relative to the resources directory
				const std::filesystem::path texPath=
					PathUtils::getResourceDirectory() / texJson["path"].get<std::string>();

				MkGuiStyleTextureEntry entry;
				entry.x= texJson.value("width", 16.f);
				entry.y= texJson.value("height", 16.f);
				entry.texture= textureCache->loadTexturePath(texPath);

				if (entry.texture)
				{
					style->textures()[texName]= entry;
				}
				else
				{
					MIKAN_LOG_WARNING("MkGuiStyleManager::loadStyleFile") << "Failed to load texture: " << texPath;
				}
			}
		}

		m_impl->styles[styleName]= style;
	}

	return bSuccess;
}
