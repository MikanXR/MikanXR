#pragma once

#include "ComponentFwd.h"
#include "ObjectSystemFwd.h"

#include <functional>

// The project's outputs as one set, across the object system each kind lives in
namespace OutputQueries
{
void visitOutputs(ProjectManagerPtr projectManager, const std::function<void(OutputComponentPtr)>& visitFunc);

// Hands one composited frame to every enabled output, and to the client return feed
void writeFrameToOutputs(ProjectManagerPtr projectManager, const struct OutputFrame& frame);

// Releases every output's sender and the return feed's, for when no compositor
// publishes any more
void closeOutputs(ProjectManagerPtr projectManager);
} // namespace OutputQueries
