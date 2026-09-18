#include "MkGuiScopedStyle.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiScopedFont.h"
#include "MkGuiScopedStyleVar.h"
#include "MkGuiScopedStyleColor.h"

#include <memory>

struct MkGuiScopedStyle::Impl
{
	// Destruction order is reverse of declaration order (LIFO):
	// m_colors pops first, then m_vars, then m_font
	MkGuiScopedStyleColor colors;
	MkGuiScopedStyleVar vars;
	std::unique_ptr<MkGuiScopedFont> font;
};

MkGuiScopedStyle::MkGuiScopedStyle(MkGuiStyleConstPtr style)
{
	if (!style)
		return;

	m_impl= new Impl();

	// Push font first (popped last). The size needs no scaling of its own: ImGui
	// applies the UI scale to every font size, including the ones pushed here.
	if (style->getFont())
	{
		m_impl->font= std::make_unique<MkGuiScopedFont>(style->getFont(), style->getFontSize());
	}

	// The vars are authored in pixels and are pushed over the already scaled
	// base style, so the sizes among them take the scale here
	const float uiScale= MkGui::getUiScale();

	// Push style floats
	for (int i= 0; i < style->getFloatVarCount(); ++i)
	{
		const MkGuiStyleFloatEntry& e= style->getFloatVar(i);
		const float scale= e.bScalesWithDpi ? uiScale : 1.f;
		m_impl->vars.push(e.var, e.floatVal * scale);
	}

	// Push style vecs
	for (int i= 0; i < style->getVec2VarCount(); ++i)
	{
		const MkGuiStyleVec2Entry& e= style->getVec2Var(i);
		const float scale= e.bScalesWithDpi ? uiScale : 1.f;
		m_impl->vars.push(e.var, ImVec2(e.vec2Val.x * scale, e.vec2Val.y * scale));
	}

	// Push colors (popped first)
	for (int i= 0; i < style->getColorCount(); ++i)
	{
		const MkGuiStyleColorEntry& e= style->getColor(i);
		m_impl->colors.push(e.col, e.value);
	}
}

MkGuiScopedStyle::~MkGuiScopedStyle() { delete m_impl; }
