#include "MkCanvasWidgets.h"
#include "MkCanvasScopedNode.h"
#include "MkGuiDrawUtils.h"

// IMGUI_DEFINE_MATH_OPERATORS comes from the MikanGUI target definitions
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_node_editor.h"

// clang-format off
namespace MkCanvas
{
// Icon geometry adapted from imgui-node-editor's blueprints example
// (examples/blueprints-example/utilities/drawing.cpp, public domain,
// written by Michal Cichon), retyped onto the owned PinIcon enum.
static void drawIconGeometry(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, PinIcon type,
							 bool filled, ImU32 color, ImU32 innerColor)
{
          auto rect           = ImRect(a, b);
          auto rect_x         = rect.Min.x;
          auto rect_y         = rect.Min.y;
          auto rect_w         = rect.Max.x - rect.Min.x;
          auto rect_h         = rect.Max.y - rect.Min.y;
          auto rect_center_x  = (rect.Min.x + rect.Max.x) * 0.5f;
          auto rect_center_y  = (rect.Min.y + rect.Max.y) * 0.5f;
          auto rect_center    = ImVec2(rect_center_x, rect_center_y);
    const auto outline_scale  = rect_w / 24.0f;
    const auto extra_segments = static_cast<int>(2 * outline_scale); // for full circle

    if (type == PinIcon::Flow)
    {
        const auto origin_scale = rect_w / 24.0f;

        const auto offset_x  = 1.0f * origin_scale;
        const auto offset_y  = 0.0f * origin_scale;
        const auto margin     = (filled ? 2.0f : 2.0f) * origin_scale;
        const auto rounding   = 0.1f * origin_scale;
        const auto tip_round  = 0.7f; // percentage of triangle edge (for tip)
        //const auto edge_round = 0.7f; // percentage of triangle edge (for corner)
        const auto canvas = ImRect(
            rect.Min.x + margin + offset_x,
            rect.Min.y + margin + offset_y,
            rect.Max.x - margin + offset_x,
            rect.Max.y - margin + offset_y);
        const auto canvas_x = canvas.Min.x;
        const auto canvas_y = canvas.Min.y;
        const auto canvas_w = canvas.Max.x - canvas.Min.x;
        const auto canvas_h = canvas.Max.y - canvas.Min.y;

        const auto left   = canvas_x + canvas_w            * 0.5f * 0.3f;
        const auto right  = canvas_x + canvas_w - canvas_w * 0.5f * 0.3f;
        const auto top    = canvas_y + canvas_h            * 0.5f * 0.2f;
        const auto bottom = canvas_y + canvas_h - canvas_h * 0.5f * 0.2f;
        const auto center_y = (top + bottom) * 0.5f;
        //const auto angle = AX_PI * 0.5f * 0.5f * 0.5f;

        const auto tip_top    = ImVec2(canvas_x + canvas_w * 0.5f, top);
        const auto tip_right  = ImVec2(right, center_y);
        const auto tip_bottom = ImVec2(canvas_x + canvas_w * 0.5f, bottom);

        drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
        drawList->PathBezierCubicCurveTo(
            ImVec2(left, top),
            ImVec2(left, top),
            ImVec2(left, top) + ImVec2(rounding, 0));
        drawList->PathLineTo(tip_top);
        drawList->PathLineTo(tip_top + (tip_right - tip_top) * tip_round);
        drawList->PathBezierCubicCurveTo(
            tip_right,
            tip_right,
            tip_bottom + (tip_right - tip_bottom) * tip_round);
        drawList->PathLineTo(tip_bottom);
        drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
        drawList->PathBezierCubicCurveTo(
            ImVec2(left, bottom),
            ImVec2(left, bottom),
            ImVec2(left, bottom) - ImVec2(0, rounding));

        if (!filled)
        {
            if (innerColor & 0xFF000000)
                drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

            drawList->PathStroke(color, 2.0f * outline_scale, ImDrawFlags_Closed);
        }
        else
            drawList->PathFillConvex(color);
    }
    else
    {
        auto triangleStart = rect_center_x + 0.32f * rect_w;

        auto rect_offset = -static_cast<int>(rect_w * 0.25f * 0.25f);

        rect.Min.x    += rect_offset;
        rect.Max.x    += rect_offset;
        rect_x        += rect_offset;
        rect_center_x += rect_offset * 0.5f;
        rect_center.x += rect_offset * 0.5f;

        if (type == PinIcon::Circle)
        {
            const auto c = rect_center;

            if (!filled)
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;

                if (innerColor & 0xFF000000)
                    drawList->AddCircleFilled(c, r, innerColor, 12 + extra_segments);
                drawList->AddCircle(c, r, color, 12 + extra_segments, 2.0f * outline_scale);
            }
            else
            {
                drawList->AddCircleFilled(c, 0.5f * rect_w / 2.0f, color, 12 + extra_segments);
            }
        }

        if (type == PinIcon::Square)
        {
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                if (innerColor & 0xFF000000)
                {
                    drawList->AddRectFilled(p0, p1, innerColor, 0, ImDrawFlags_RoundCornersAll);
                }

                drawList->AddRect(p0, p1, color, 0, 2.0f * outline_scale, ImDrawFlags_RoundCornersAll);
            }
        }

        if (type == PinIcon::Grid)
        {
            const auto r = 0.5f * rect_w / 2.0f;
            const auto w = ceilf(r / 3.0f);

            const auto baseTl = ImVec2(floorf(rect_center_x - w * 2.5f), floorf(rect_center_y - w * 2.5f));
            const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));

            auto tl = baseTl;
            auto br = baseBr;
            for (int i = 0; i < 3; ++i)
            {
                tl.x = baseTl.x;
                br.x = baseBr.x;
                drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                if (i != 1 || filled)
                    drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                drawList->AddRectFilled(tl, br, color);

                tl.y += w * 2;
                br.y += w * 2;
            }

            triangleStart = br.x + w + 1.0f / 24.0f * rect_w;
        }

        if (type == PinIcon::RoundSquare)
        {
            if (filled)
            {
                const auto r  = 0.5f * rect_w / 2.0f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                drawList->AddRectFilled(p0, p1, color, cr, ImDrawFlags_RoundCornersAll);
            }
            else
            {
                const auto r = 0.5f * rect_w / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = rect_center - ImVec2(r, r);
                const auto p1 = rect_center + ImVec2(r, r);

                if (innerColor & 0xFF000000)
                {
                    drawList->AddRectFilled(p0, p1, innerColor, cr, ImDrawFlags_RoundCornersAll);
                }

                drawList->AddRect(p0, p1, color, cr, 2.0f * outline_scale, ImDrawFlags_RoundCornersAll);
            }
        }
        else if (type == PinIcon::Diamond)
        {
            if (filled)
            {
                const auto r = 0.607f * rect_w / 2.0f;
                const auto c = rect_center;

                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));
                drawList->PathFillConvex(color);
            }
            else
            {
                const auto r = 0.607f * rect_w / 2.0f - 0.5f;
                const auto c = rect_center;

                drawList->PathLineTo(c + ImVec2( 0, -r));
                drawList->PathLineTo(c + ImVec2( r,  0));
                drawList->PathLineTo(c + ImVec2( 0,  r));
                drawList->PathLineTo(c + ImVec2(-r,  0));

                if (innerColor & 0xFF000000)
                    drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

                drawList->PathStroke(color, 2.0f * outline_scale, ImDrawFlags_Closed);
            }
        }
        else
        {
            const auto triangleTip = triangleStart + rect_w * (0.45f - 0.32f);

            drawList->AddTriangleFilled(
                ImVec2(ceilf(triangleTip), rect_y + rect_h * 0.5f),
                ImVec2(triangleStart, rect_center_y + 0.15f * rect_h),
                ImVec2(triangleStart, rect_center_y - 0.15f * rect_h),
                color);
        }
    }
}

// clang-format on

ImVec2 toCanvasSpace(const ImVec2& logical)
{
	const float uiScale= MkGui::getUiScale();

	return ImVec2(logical.x * uiScale, logical.y * uiScale);
}

ImVec2 fromCanvasSpace(const ImVec2& canvas)
{
	const float uiScale= MkGui::getUiScale();

	return uiScale > 0.f ? ImVec2(canvas.x / uiScale, canvas.y / uiScale) : canvas;
}

void drawPinIcon(const ImVec2& size, PinIcon icon, bool bFilled, MkCanvasPinDirection direction, const ImVec4& color,
				 const ImVec4& innerColor)
{
	const ImVec2 cursorPos= ImGui::GetCursorScreenPos();

	if (ImGui::IsRectVisible(size))
	{
		ImDrawList* drawList= ImGui::GetWindowDrawList();

		drawIconGeometry(drawList, cursorPos, cursorPos + size, icon, bFilled, ImColor(color), ImColor(innerColor));
	}

	ImGui::Dummy(size);

	// Anchor the link endpoint to a fixed point on the icon's connection edge.
	// A degenerate pivot rect keeps the endpoint from sliding along the icon
	// boundary as link angles change; the pin's full item rect would land
	// endpoints out beside the label text.
	const float pivotX= (direction == MkCanvasPinDirection::Output) ? cursorPos.x + size.x : cursorPos.x;
	const ImVec2 pivotPoint(pivotX, cursorPos.y + size.y * 0.5f);
	ax::NodeEditor::PinPivotRect(pivotPoint, pivotPoint);
}

void drawCommentNode(int nodeId, const char* title, const ImVec4& color, ImVec2& inoutGroupSize, bool bApplyGroupSize)
{
	const int canvasId= toCanvasId(nodeId);
	const ImVec2 canvasGroupSize= toCanvasSpace(inoutGroupSize);
	if (bApplyGroupSize)
	{
		ed::SetGroupSize(canvasId, canvasGroupSize);
	}

	// The region stays see-through so the nodes inside keep their contrast
	ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(color.x, color.y, color.z, 0.18f));
	ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(color.x, color.y, color.z, 0.6f));
	{
		MkCanvasScopedNode node(nodeId, ImVec4(color.x, color.y, color.z, 0.85f));
		node.beginHeader();
		ImGui::TextUnformatted(title);
		node.endHeader();

		// Once the node is a group the canvas ignores the seed and reports its own size
		ed::Group(canvasGroupSize);
		inoutGroupSize= fromCanvasSpace(ImGui::GetItemRectSize());
	}
	ed::PopStyleColor(2);

	// Zoomed out, the title band shrinks past legibility; the hint floats above the box at screen size
	if (ed::BeginGroupHint(canvasId))
	{
		const float uiScale= MkGui::getUiScale();
		const float hintPadX= 8.f * uiScale;
		const float hintPadY= 4.f * uiScale;
		const float hintRounding= 4.f * uiScale;

		const ImVec2 groupMin= ed::GetGroupMin();
		ImGui::SetCursorScreenPos(
			ImVec2(groupMin.x + hintPadX, groupMin.y - ImGui::GetTextLineHeightWithSpacing() - hintPadY));
		ImGui::BeginGroup();
		ImGui::TextUnformatted(title);
		ImGui::EndGroup();

		const ImVec2 hintMin(ImGui::GetItemRectMin().x - hintPadX, ImGui::GetItemRectMin().y - hintPadY);
		const ImVec2 hintMax(ImGui::GetItemRectMax().x + hintPadX, ImGui::GetItemRectMax().y + hintPadY);
		ImDrawList* drawList= ed::GetHintBackgroundDrawList();
		drawList->AddRectFilled(hintMin, hintMax, ImColor(color.x, color.y, color.z, 0.5f), hintRounding);
		drawList->AddRect(hintMin, hintMax, ImColor(color.x, color.y, color.z, 0.9f), hintRounding);
	}
	ed::EndGroupHint();
}
} // namespace MkCanvas
