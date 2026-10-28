#pragma once

#include "MikanUtilityExport.h"

#include <filesystem>

//-- utility methods -----
namespace GStreamerUtils
{
/// Locate the installed GStreamer runtime and make its DLLs loadable by a plugin that
/// links it. On Windows the root comes from GSTREAMER_1_0_ROOT_MINGW_X86_64, then from
/// the MSI's HKLM\SOFTWARE\GStreamer1.0\x86_64 InstallDir, and <root>/bin is prepended
/// to this process's PATH once. Elsewhere the system loader finds GStreamer and this
/// returns true. outRuntimeDir receives the bin directory used (empty when not found).
MIKAN_UTILITY_FUNC(bool) ensureRuntimeLoadable(std::filesystem::path& outRuntimeDir);
}; // namespace GStreamerUtils
