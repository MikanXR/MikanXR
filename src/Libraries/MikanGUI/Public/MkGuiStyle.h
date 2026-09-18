#pragma once

#include "imgui.h"
#include "MkRendererFwd.h"

#include <memory>

// Style var values are authored in pixels, so most of them take the UI scale when
// they are pushed. The ones that are ratios rather than sizes (alpha, text
// alignment) carry the flag cleared.
struct MkGuiStyleFloatEntry
{
	ImGuiStyleVar var;
	float floatVal= 0.f;
	bool bScalesWithDpi= true;
};

struct MkGuiStyleVec2Entry
{
	ImGuiStyleVar var;
	ImVec2 vec2Val= {0.f, 0.f};
	bool bScalesWithDpi= true;
};

struct MkGuiStyleColorEntry
{
	ImGuiCol col;
	ImVec4 value;
};

struct MkGuiStyleTextureEntry
{
	float x= 0.f; // display width
	float y= 0.f; // display height
	IMkTextureConstPtr texture;
};
