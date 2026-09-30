//-- includes -----
#include "FatalStartupError.h"

//-- statics -----
namespace
{
// Startup runs on the main thread, so the record needs no lock
FatalStartupErrorInfo g_fatalStartupError;
} // namespace

//-- utility methods -----
namespace FatalStartupError
{
void report(eFatalStartupErrorType type, const std::string& subject, const std::string& detail)
{
	if (g_fatalStartupError.type != eFatalStartupErrorType::none)
		return;

	g_fatalStartupError.type= type;
	g_fatalStartupError.subject= subject;
	g_fatalStartupError.detail= detail;
}

const FatalStartupErrorInfo& get() { return g_fatalStartupError; }
}; // namespace FatalStartupError
