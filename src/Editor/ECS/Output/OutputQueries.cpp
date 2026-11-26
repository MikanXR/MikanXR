#include "OutputQueries.h"
#include "OutputComponent.h"
#include "ProjectManager.h"
#include "RTPOutputSystem.h"
#include "SpoutOutputSystem.h"

namespace OutputQueries
{
void visitOutputs(ProjectManagerPtr projectManager, const std::function<void(OutputComponentPtr)>& visitFunc)
{
	if (!projectManager)
		return;

	if (auto spoutOutputSystem= projectManager->getSystemOfType<SpoutOutputSystem>())
	{
		spoutOutputSystem->visitComponents([&visitFunc](SpoutOutputComponentPtr output) { visitFunc(output); });
	}

	if (auto rtpOutputSystem= projectManager->getSystemOfType<RTPOutputSystem>())
	{
		rtpOutputSystem->visitComponents([&visitFunc](RTPOutputComponentPtr output) { visitFunc(output); });
	}
}

void writeFrameToOutputs(ProjectManagerPtr projectManager, const OutputFrame& frame)
{
	if (!projectManager || !frame.texture)
		return;

	visitOutputs(projectManager,
				 [&frame](OutputComponentPtr output)
				 {
					 if (output->getOutputDefinition()->getIsEnabled())
					 {
						 output->writeFrame(frame);
					 }
				 });

	// The feeds clients asked for ride the same frame through a stream of their own
	if (auto rtpOutputSystem= projectManager->getSystemOfType<RTPOutputSystem>())
	{
		rtpOutputSystem->writeReturnFeedFrame(frame);
	}
}

void closeOutputs(ProjectManagerPtr projectManager)
{
	if (!projectManager)
		return;

	visitOutputs(projectManager, [](OutputComponentPtr output) { output->closeOutput(); });

	if (auto rtpOutputSystem= projectManager->getSystemOfType<RTPOutputSystem>())
	{
		rtpOutputSystem->closeReturnFeed();
	}
}
} // namespace OutputQueries
