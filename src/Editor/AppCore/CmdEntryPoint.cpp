//-- includes -----
#include "CmdApp.h"
#include "DiscreteGpuPreference.h"
#include "ThreadUtils.h"

MIKAN_REQUEST_DISCRETE_GPU()

//-- entry point -----
int main(int argc, char* argv[])
{
	ThreadUtils::initMainThreadId();

	CmdApp app;

	return app.exec(argc, argv);
}
