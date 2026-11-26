#pragma once

#include "ComponentFwd.h"
#include "MkRendererFwd.h"
#include "ObjectSystemFwd.h"

#include <functional>

// The project's outputs as one set, across the object system each kind lives in
namespace OutputQueries
{
void visitOutputs(ProjectManagerPtr projectManager, const std::function<void(OutputComponentPtr)>& visitFunc);

// Hands one composited frame to every enabled output
void writeFrameToOutputs(ProjectManagerPtr projectManager, IMkTexturePtr compositedTexture);

// Releases every output's sender, for when no compositor publishes any more
void closeOutputs(ProjectManagerPtr projectManager);
} // namespace OutputQueries
