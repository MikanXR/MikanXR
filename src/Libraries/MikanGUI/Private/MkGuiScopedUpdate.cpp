#include "MkGuiScopedUpdate.h"
#include "MkGuiContext.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"
#include "ImGuizmo.h"

MkGuiScopedUpdate::MkGuiScopedUpdate(MkGuiContext& context)
	: MkGuiScopedContext(context)
{
	// Pick up a monitor or user scale change before anything is submitted this frame
	context.refreshUiScale();

	// Tell ImGui to prepare for a new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// ImGuizmo keeps one global context, so this resets its per-frame hover flags
	// and binds its fallback draw list in whichever ImGui context is current,
	// which lets any window draw a gizmo
	ImGuizmo::BeginFrame();

	// After NewFrame, which is where a settings load builds the dock nodes and
	// window settings this re-proportions
	context.refreshLayoutScale();

	// Before any window begins this frame, so a moved window is placed (and given
	// its own viewport) by the Begin that follows
	context.applyPendingFloatRequests();
}

MkGuiScopedUpdate::~MkGuiScopedUpdate()
{
	// Finalize the ImGui draw lists
	ImGui::Render();
}
