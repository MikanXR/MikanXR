#pragma once

#include <ctime>
#include <string>

// The file stem convention shared with the MikanARStreamer app: a take is
// "take_YYYYMMDD_HHMMSS" in local time, and the marker reference shot that pairs
// with it carries the take's name plus "_marker", so the align flow can find
// one from the other by name alone.
namespace TakeNaming
{
inline constexpr const char* k_markerSuffix= "_marker";

std::string makeTakeName(const std::tm& localTime);
std::string makeTakeNameNow();
std::string makeMarkerName(const std::string& takeName);
bool isMarkerName(const std::string& takeName);
} // namespace TakeNaming
