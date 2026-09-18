#include "MkGuiScopedUpdate.h"
#include "MkGuiContext.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl3.h"

MkGuiScopedUpdate::MkGuiScopedUpdate(MkGuiContext& context)
	: MkGuiScopedContext(context)
{
	// Pick up a monitor or user scale change before anything is submitted this frame
	context.refreshUiScale();

	// Tell ImGui to prepare for a new frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// After NewFrame, which is where a settings load builds the dock nodes and
	// window settings this re-proportions
	context.refreshLayoutScale();
}

MkGuiScopedUpdate::~MkGuiScopedUpdate()
{
	// Finalize the ImGui draw lists
	ImGui::Render();
}
