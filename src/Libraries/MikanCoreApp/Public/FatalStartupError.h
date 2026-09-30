#pragma once

#include "MikanCoreAppExport.h"

#include <string>

//-- definitions -----
enum class eFatalStartupErrorType
{
	none,

	// A built-in shader failed to compile. subject is the shader name, detail the driver's compile log.
	internalShaderCompile,
};

// Why startup cannot continue, recorded by whichever layer hit the failure so the
// application can explain it to the user after the failure has unwound to it.
// The fields are data, not display text: the application owns the wording.
struct FatalStartupErrorInfo
{
	eFatalStartupErrorType type= eFatalStartupErrorType::none;
	std::string subject;
	std::string detail;
};

//-- utility methods -----
namespace FatalStartupError
{
/// Records a fatal startup error. Only the first report is kept, since later
/// failures during the same startup are usually consequences of it.
MIKAN_COREAPP_FUNC(void) report(eFatalStartupErrorType type, const std::string& subject, const std::string& detail);

/// The first reported error, or one of type none when nothing was reported
MIKAN_COREAPP_FUNC(const FatalStartupErrorInfo&) get();
}; // namespace FatalStartupError
