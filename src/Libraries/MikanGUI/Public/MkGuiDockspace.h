#pragma once

#include "MkGuiExport.h"

#include "imgui.h"

// Dockspace hosting. The DockBuilder half of this is ImGui internal API, which
// stays behind this wrapper so the editor never includes imgui_internal.h.
namespace MkGui
{
// Opens a full-viewport, chrome-free host window carrying a menu bar and a
// dockspace whose central node is left transparent, so whatever the app drew
// into the back buffer (the 3d scene) shows through it.
//
// Returns the dockspace id. outNeedsDefaultLayout is true on a run with no
// saved layout, or when bResetLayout asks for the saved one to be discarded,
// which is when the caller should build one and call dockBuilderFinish. Always
// pair with endDockspaceHost.
MIKAN_GUI_FUNC(ImGuiID)
beginDockspaceHost(const char* hostWindowId, const char* dockspaceId, bool& outNeedsDefaultLayout,
				   bool bResetLayout= false);
MIKAN_GUI_FUNC(void) endDockspaceHost();

// Splits a node, returning the new child on the given side. The remainder of
// the original node is written back to inOutRemainingNodeId.
MIKAN_GUI_FUNC(ImGuiID) dockBuilderSplit(ImGuiID nodeId, ImGuiDir direction, float sizeRatio,
										 ImGuiID& inOutRemainingNodeId);
MIKAN_GUI_FUNC(void) dockBuilderDockWindow(const char* windowName, ImGuiID nodeId);
MIKAN_GUI_FUNC(void) dockBuilderFinish(ImGuiID dockspaceId);

// Screen-space rect of the central (empty) node, which is the area left for the
// 3d scene. Returns false when the dockspace has no central node yet.
//
// Takes the same string id passed to beginDockspaceHost and must be called while
// that host window is still current (from the stage's gui pass), because the id
// resolves against the current window's id stack.
MIKAN_GUI_FUNC(bool) getDockspaceCentralRect(const char* dockspaceId, ImVec2& outPos, ImVec2& outSize);

// The dock layout is measured in the same scaled pixels its panels draw in, so
// a layout arranged at one UI scale is wrong at another. These carry a reference
// scale with the layout the way ImGui carries one with a table's column widths.
//
// installDockLayoutSettings registers the ini entry carrying the reference
// scale, read into and written from the float the caller owns, which must
// outlive the ImGui context. Call it on a context before its settings load.
// The value stays 0 when the ini carries no entry, which means a layout with
// no reference yet rather than one of scale zero.
MIKAN_GUI_FUNC(void) installDockLayoutSettings(float* refScaleStorage);

// Multiplies every dock node's size by the factor, which re-proportions the
// splits against content that just changed size. Call after ImGui::NewFrame, so
// the nodes a settings load creates are there to walk.
MIKAN_GUI_FUNC(void) scaleDockLayout(float factor);

// Sets ImGui's font rasterizer density (an imgui_internal state), returning
// the previous value. Glyphs bake at density x resolution while keeping
// logical metrics, so geometry scaled up by a canvas transform samples a
// matching-density bitmap and stays crisp. Used by MkCanvasScopedEditor.
MIKAN_GUI_FUNC(float) setFontRasterizerDensity(float density);
} // namespace MkGui
