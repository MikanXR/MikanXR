#include "OutputQueries.h"
#include "OutputComponent.h"
#include "ProjectManager.h"
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
}

void writeFrameToOutputs(ProjectManagerPtr projectManager, IMkTexturePtr compositedTexture)
{
	if (!compositedTexture)
		return;

	visitOutputs(projectManager,
				 [&compositedTexture](OutputComponentPtr output)
				 {
					 if (output->getOutputDefinition()->getIsEnabled())
					 {
						 output->writeFrame(compositedTexture);
					 }
				 });
}

void closeOutputs(ProjectManagerPtr projectManager)
{
	visitOutputs(projectManager, [](OutputComponentPtr output) { output->closeOutput(); });
}
} // namespace OutputQueries
