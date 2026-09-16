#include "TakeNaming.h"
#include "JsonlSessionLogWriter.h"

namespace
{
constexpr const char* k_takePrefix= "take_";
constexpr const char* k_timestampFormat= "%Y%m%d_%H%M%S";
} // namespace

std::string TakeNaming::makeTakeName(const std::tm& localTime)
{
	char timestamp[32];
	std::strftime(timestamp, sizeof(timestamp), k_timestampFormat, &localTime);
	return std::string(k_takePrefix) + timestamp;
}

std::string TakeNaming::makeTakeNameNow()
{
	return std::string(k_takePrefix) + JsonlSessionLogWriter::makeLocalTimestampString(k_timestampFormat);
}

std::string TakeNaming::makeMarkerName(const std::string& takeName) { return takeName + k_markerSuffix; }

bool TakeNaming::isMarkerName(const std::string& takeName)
{
	const std::string suffix= k_markerSuffix;
	return takeName.size() > suffix.size()
		   && takeName.compare(takeName.size() - suffix.size(), suffix.size(), suffix) == 0;
}
