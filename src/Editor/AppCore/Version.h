#ifndef VERSION_H
#define VERSION_H

// Release version, calendar style: the date the release was cut, zero padded.
// The parts are string literals because a padded month or day ("09") is not a
// valid C++ integer literal. REVISION is empty for the day's first release and
// ".1", ".2", ... for a same-day re-cut.
// The release workflow refuses a tag that does not read "v" + this string.
#define MIKAN_RELEASE_VERSION_YEAR "2026"
#define MIKAN_RELEASE_VERSION_MONTH "09"
#define MIKAN_RELEASE_VERSION_DAY "14"
#define MIKAN_RELEASE_VERSION_REVISION ""

/// "YYYY.MM.DD" or "YYYY.MM.DD.N"
#if !defined(MIKAN_RELEASE_VERSION_STRING)
#define MIKAN_RELEASE_VERSION_STRING                                                                                   \
	MIKAN_RELEASE_VERSION_YEAR "." MIKAN_RELEASE_VERSION_MONTH                                                         \
							   "." MIKAN_RELEASE_VERSION_DAY MIKAN_RELEASE_VERSION_REVISION
#endif

// Latest Mikan API Protocol Version used by the server
// Increment this value when the server API changes
#define MIKAN_SERVER_API_VERSION 0

// Oldest Mikan API Protocol Version allowed by the server
// Increment this value when deprecating old client API versions
#define MIKAN_MIN_ALLOWED_CLIENT_API_VERSION 0

#endif // VERSION_H
