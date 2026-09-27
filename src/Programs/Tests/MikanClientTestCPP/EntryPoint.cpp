#include "TestApp.h"
#include "TestFrameDump.h"
#include "TestSpoutProbe.h"
#include "Logger.h"
#include <SDL.h>

#include <cstring>
#include <string>
#include <vector>

// -probe <sender> <png>: read any Spout sender by name through the editor's receiver path and
// write it as a PNG, then exit. No window, no client connection: a drive uses it to see what
// another client (a game engine) is actually publishing, independent of any compositor graph.
static int probeSpoutSender(const char* senderName, const char* pngPath)
{
	LoggerSettings settings= {};
	settings.min_log_level= LogSeverityLevel::info;
	settings.enable_console= true;
	log_init(settings);

	std::vector<uint8_t> pixels;
	int width= 0;
	int height= 0;
	int exitCode= 0;
	if (readSpoutSenderRgba(senderName, pixels, width, height) && writeRgbaPng(pngPath, pixels, width, height))
	{
		MIKAN_LOG_INFO("probe") << "Wrote Spout sender " << senderName << " (" << width << "x" << height << ") to "
								<< pngPath;
	}
	else
	{
		MIKAN_LOG_ERROR("probe") << "Failed to read back Spout sender " << senderName;
		exitCode= 1;
	}

	log_dispose();
	return exitCode;
}

int main(int argc, char* argv[])
{
	if (argc == 4 && std::strcmp(argv[1], "-probe") == 0)
	{
		return probeSpoutSender(argv[2], argv[3]);
	}

	TestApp app;

	return app.exec(argc, argv);
}
