#pragma once

#include "ObjectSystemFwd.h"
#include "ProjectPipelineModel.h"

#include <string>

class ClientSourceManager;

// Reads the current scene's chain out of the object systems into a snapshot the
// pipeline model rules run on. The six stages always come first, in
// ePipelineStage order, so the top row has a box for each even with no current
// scene. The client source manager may be null (MikanCmd), and then no client
// texture source reads as connected.
PipelineSnapshot gatherProjectPipeline(ProjectManagerPtr projectManager, ClientSourceManager* clientSourceManager);

// The display string of a pipeline text: the name with no key, the key's
// translation, or for a "Fmt" key its format expanded with the text's args
std::string formatPipelineText(const PipelineText& text);
