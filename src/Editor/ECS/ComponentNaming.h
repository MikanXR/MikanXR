#pragma once

#include <string>
#include <vector>

// A component class the settings UI offers a name prefix for, with the
// prefix it starts out with. An empty default means the class name itself
// is the prefix until the user types one.
struct ComponentNamePrefixEntry
{
	std::string componentClassName;
	std::string defaultPrefix;
};

// Every creatable component class, in the order the settings UI lists them
const std::vector<ComponentNamePrefixEntry>& getComponentNamePrefixEntries();

// The built-in prefix for a class, empty when the class has none
std::string getDefaultComponentNamePrefix(const std::string& componentClassName);

// "<prefix>_<id>", or "<className>_<id>" when the prefix is empty
std::string makeDefaultComponentName(const std::string& prefix, const std::string& componentClassName, int componentId);

// The name a freshly created component gets: the app settings prefix when an
// App is running, the built-in default otherwise (MikanCmd has no App)
std::string resolveDefaultComponentName(const std::string& componentClassName, int componentId);
