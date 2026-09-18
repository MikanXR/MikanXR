#include "MkGuiStyle.h"
#include "MkGuiDrawUtils.h"

int MkGuiStyle::getLabelWidth() const { return (int)(m_labelWidth * MkGui::getUiScale()); }

int MkGuiStyle::getValueWidth() const { return (int)(m_valueWidth * MkGui::getUiScale()); }

const MkGuiStyleTextureEntry* MkGuiStyle::findTexture(const std::string& name) const
{
	auto it= m_textures.find(name);
	return (it != m_textures.end()) ? &it->second : nullptr;
}
