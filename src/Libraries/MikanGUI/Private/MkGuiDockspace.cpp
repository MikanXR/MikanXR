#include "MkGuiDockspace.h"

// IMGUI_DEFINE_MATH_OPERATORS comes from the MikanGUI target definitions
#include "imgui_internal.h"

#include <cstdio>
#include <cstring>

namespace MkGui
{
ImGuiID beginDockspaceHost(const char* hostWindowId, const char* dockspaceId, bool& outNeedsDefaultLayout,
						   bool bResetLayout)
{
	const ImGuiViewport* viewport= ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	// NoBackground is what lets the 3d scene show through the central node.
	// NoBringToFrontOnFocus keeps the host behind the panels docked into it.
	const ImGuiWindowFlags hostFlags=
		ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::Begin(hostWindowId, nullptr, hostFlags);
	ImGui::PopStyleVar();

	const ImGuiID dockspace= ImGui::GetID(dockspaceId);

	outNeedsDefaultLayout= bResetLayout || ImGui::DockBuilderGetNode(dockspace) == nullptr;
	if (outNeedsDefaultLayout)
	{
		// Removing the node also undocks its windows, so a reset re-docks every one
		ImGui::DockBuilderRemoveNode(dockspace);
		ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);
	}

	ImGui::DockSpace(dockspace, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);

	return dockspace;
}

void endDockspaceHost() { ImGui::End(); }

ImGuiID dockBuilderSplit(ImGuiID nodeId, ImGuiDir direction, float sizeRatio, ImGuiID& inOutRemainingNodeId)
{
	return ImGui::DockBuilderSplitNode(nodeId, direction, sizeRatio, nullptr, &inOutRemainingNodeId);
}

void dockBuilderDockWindow(const char* windowName, ImGuiID nodeId) { ImGui::DockBuilderDockWindow(windowName, nodeId); }

void dockBuilderFinish(ImGuiID dockspaceId) { ImGui::DockBuilderFinish(dockspaceId); }

bool getDockspaceCentralRect(const char* dockspaceId, ImVec2& outPos, ImVec2& outSize)
{
	const ImGuiDockNode* centralNode= ImGui::DockBuilderGetCentralNode(ImGui::GetID(dockspaceId));
	if (centralNode == nullptr)
		return false;

	outPos= centralNode->Pos;
	outSize= centralNode->Size;
	return true;
}

// -- Dock layout reference scale ----
// The ini entry is written and read through ImGui's own settings machinery, so
// the scale a layout was arranged at stays with that layout in the same file.
static void* dockLayoutSettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
	// One entry, so any name opens the same storage
	return strcmp(name, "Dock") == 0 ? handler->UserData : nullptr;
}

static void dockLayoutSettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
{
	float refScale= 0.f;
	if (entry != nullptr && sscanf(line, "RefScale=%f", &refScale) == 1)
	{
		*(float*)entry= refScale;
	}
}

static void dockLayoutSettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* outBuffer)
{
	const float* refScale= (const float*)handler->UserData;
	if (refScale == nullptr || *refScale <= 0.f)
		return;

	outBuffer->appendf("[%s][Dock]\n", handler->TypeName);
	outBuffer->appendf("RefScale=%g\n", *refScale);
	outBuffer->append("\n");
}

void installDockLayoutSettings(float* refScaleStorage)
{
	ImGuiSettingsHandler handler;
	handler.TypeName= "MikanLayout";
	handler.TypeHash= ImHashStr("MikanLayout");
	handler.ReadOpenFn= dockLayoutSettingsReadOpen;
	handler.ReadLineFn= dockLayoutSettingsReadLine;
	handler.WriteAllFn= dockLayoutSettingsWriteAll;
	handler.UserData= refScaleStorage;

	ImGui::AddSettingsHandler(&handler);
}

void scaleDockLayout(float factor)
{
	ImGuiContext* context= ImGui::GetCurrentContext();
	if (context == nullptr || factor <= 0.f)
		return;

	// Every node, rather than the tree under one dockspace id, because that id
	// only resolves while its host window is current
	ImGuiDockContext* dockContext= &context->DockContext;
	for (int nodeIndex= 0; nodeIndex < dockContext->Nodes.Data.Size; nodeIndex++)
	{
		if (ImGuiDockNode* node= (ImGuiDockNode*)dockContext->Nodes.Data[nodeIndex].val_p)
		{
			node->Size= ImTrunc(node->Size * factor);
			node->SizeRef= ImTrunc(node->SizeRef * factor);
		}
	}
}

float setFontRasterizerDensity(float density)
{
	const float previousDensity= ImGui::GetFontRasterizerDensity();
	ImGui::SetFontRasterizerDensity(density);
	return previousDensity;
}
} // namespace MkGui
