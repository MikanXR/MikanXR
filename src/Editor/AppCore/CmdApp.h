#pragma once

#include <map>
#include <set>
#include <string>

// Stripped-down command-line application used by the MikanCmd executable.
//
// Unlike the full GUI App (App.h), CmdApp does NOT create any window, graphics
// context, CEF instance, or module manager. It initializes the logger, parses
// the command line, and dispatches to a command handler.
//
// Commands are selected by flags (e.g. "-runTests"). Add new commands by adding
// a handler method and dispatching to it from exec().
class CmdApp
{
public:
	int exec(int argc, char** argv);

private:
	void parseCommandLine(int argc, char** argv);
	bool hasCommandLineFlag(const std::string& flag) const;
	std::string getCommandLineStringArg(const std::string& key, const std::string& defaultValue= "") const;

	void printUsage() const;

	// Command handlers (return a process exit code)
	int runTests() const;
	int triggerCrash() const;
	int estimateLighting() const;
	int generateDepthMesh() const;
	int compileMaterial() const;
	int fetchModels() const;

	// Command line arguments parsed at startup
	std::map<std::string, std::string> m_commandLineParams;
	std::set<std::string> m_commandLineFlags;
};
