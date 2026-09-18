#pragma once

#include "MkGuiExport.h"
#include "MkCanvasScopedPin.h"

#include "imgui.h"

// Node-canvas widget vocabulary shared by every graph editor: pin icons drawn
// straight into the draw list, keyed by an owned icon enum rather than any
// third-party style type.
namespace MkCanvas
{
// The canvas library reserves object id 0 as its invalid/background sentinel,
// while graph ids start at 0 (the first node of every existing graph file).
// Every id crossing the canvas boundary shifts by one, in both directions.
constexpr int k_canvasIdOffset= 1;
inline int toCanvasId(int graphId) { return graphId + k_canvasIdOffset; }
inline int fromCanvasId(int canvasId) { return canvasId - k_canvasIdOffset; }

// Node positions and comment box sizes persist as 96 DPI logical units, so a
// graph laid out on one display opens the same on another. The canvas itself
// works in the scaled pixels the rest of the UI draws in, since its contents
// are measured ImGui items. Every position and size crossing the canvas
// boundary converts here, in both directions. Positions convert like sizes
// because canvas space shares its origin with the logical space.
MIKAN_GUI_FUNC(ImVec2) toCanvasSpace(const ImVec2& logical);
MIKAN_GUI_FUNC(ImVec2) fromCanvasSpace(const ImVec2& canvas);

enum class PinIcon : int
{
	Flow,
	Circle,
	Square,
	Grid,
	RoundSquare,
	Diamond,
};

// Draws a pin icon as an inline ImGui item of the given size, and anchors
// the pin's link endpoint to a fixed point on the icon's connection edge
// (right for outputs, left for inputs). Must be called between the pin's
// begin/end scope. Filled = connected; unfilled outlines read as open sockets.
MIKAN_GUI_FUNC(void)
drawPinIcon(const ImVec2& size, PinIcon icon, bool bFilled, MkCanvasPinDirection direction, const ImVec4& color,
			const ImVec4& innerColor= ImVec4(0.f, 0.f, 0.f, 0.f));

// Draws a comment box as a canvas group node: a title band in the color over
// a translucent, resizable region that drags the nodes inside it along, plus a
// floating title hint once the canvas is zoomed out too far to read the band.
// inoutGroupSize seeds the region on the first draw and comes back holding the
// size the user has resized it to, in logical units both ways; bApplyGroupSize
// pushes it to the canvas again (after a load or an edit), since the canvas
// otherwise owns the size.
MIKAN_GUI_FUNC(void)
drawCommentNode(int nodeId, const char* title, const ImVec4& color, ImVec2& inoutGroupSize, bool bApplyGroupSize);
} // namespace MkCanvas
