#ifndef VERSION_H
#define VERSION_H

/// Conventional string-ification macro.
// From: http://stackoverflow.com/questions/5256313/c-c-macro-string-concatenation
#if !defined(MIKAN_STRINGIZE)
#define MIKAN_STRINGIZEIMPL(x) #x
#define MIKAN_STRINGIZE(x) MIKAN_STRINGIZEIMPL(x)
#endif

// Current version of this release
#define MIKAN_RELEASE_VERSION_PRODUCT 1
#define MIKAN_RELEASE_VERSION_MAJOR 0
#define MIKAN_RELEASE_VERSION_MINOR 0
#define MIKAN_RELEASE_VERSION_RELEASE 0

/// "Product.Major.Minor.Release"
#if !defined(MIKAN_RELEASE_VERSION_STRING)
// Each part is stringized on its own: stringizing the dotted expression as a whole
// keeps the whitespace of the line continuation ("1.0.0 .0")
#define MIKAN_RELEASE_VERSION_STRING                                                                                   \
	MIKAN_STRINGIZE(MIKAN_RELEASE_VERSION_PRODUCT)                                                                     \
	"." MIKAN_STRINGIZE(MIKAN_RELEASE_VERSION_MAJOR) "." MIKAN_STRINGIZE(                                              \
		MIKAN_RELEASE_VERSION_MINOR) "." MIKAN_STRINGIZE(MIKAN_RELEASE_VERSION_RELEASE)
#endif

// Latest Mikan API Protocol Version used by the server
// Increment this value when the server API changes
#define MIKAN_SERVER_API_VERSION 0

// Oldest Mikan API Protocol Version allowed by the server
// Increment this value when deprecating old client API versions
#define MIKAN_MIN_ALLOWED_CLIENT_API_VERSION 0

#endif // VERSION_H
