#include "AutomationServer.h"
#include "App.h"
#include "AppStage.h"
#include "AutomationLogBuffer.h"
#include "AutomationProtocol.h"
#include "AutomationSocket.h"
#include "AutomationVariantText.h"
#include "CrashHandler.h"
#include "ProjectAssetCatalog.h"
#include "ProjectScriptContext.h"
#include "CompositorObjectSystem.h"
#include "EditorWindow.h"
#include "FunctionDatabaseEnumerator.h"
#include "IMkTexture.h"
#include "IMkWindowContext.h"
#include "MkWindowEvent.h"
#include "Logger.h"
#include "MainWindow.h"
#include "MikanComponent.h"
#include "MikanFunctionDatabase.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MikanPropertyDatabase.h"
#include "MikanServer.h"
#include "MikanShaderConfig.h"
#include "PathUtils.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "PropertyDatabaseEnumerator.h"
#include "ScriptComponent.h"
#include "ScriptObjectSystem.h"
#include "ScriptRequestHandler.h"
#include "StringUtils.h"
#include "TransactionHistory.h"

#include "Graphs/MaterialNodeGraph.h"
#include "Graphs/NodeGraph.h"
#include "MaterialCompiler/GlslShaderWriter.h"
#include "MaterialCompiler/MaterialCompiler.h"
#include "MaterialCompiler/MaterialDomain.h"
#include "Nodes/Node.h"
#include "Pins/NodeLink.h"
#include "Pins/NodePin.h"
#include "Properties/GraphProperty.h"
#include "Windows/CompositorNodeEditorWindow.h"
#include "Windows/MaterialNodeEditorWindow.h"
#include "Windows/NodeEditorWindow.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>

namespace
{
bool parseComponentId(const std::string& token, int& outComponentId)
{
	if (token.empty())
		return false;

	char* end= nullptr;
	const long parsed= strtol(token.c_str(), &end, 10);
	if (end == nullptr || *end != '\0')
		return false;

	outComponentId= (int)parsed;
	return true;
}

// Named keys the automation channel can press. A single printable character is its own key, so
// this covers only the keys that have no character form and the ones a drive actually reaches for.
bool parseKeyName(const std::string& name, MkKeySym& outKeySym)
{
	static const std::map<std::string, MkKeySym> k_namedKeys= {
		{"return", MkKey::RETURN},
		{"enter", MkKey::RETURN},
		{"tab", MkKey::TAB},
		{"escape", MkKey::ESCAPE},
		{"backspace", MkKey::BACKSPACE},
		{"delete", MkKey::DELETE_KEYCODE},
		{"space", MkKey::SPACE},
		{"up", MkKey::UP},
		{"down", MkKey::DOWN},
		{"left", MkKey::LEFT},
		{"right", MkKey::RIGHT},
		{"home", MkKey::HOME},
		{"end", MkKey::END},
		{"pageup", MkKey::PAGEUP},
		{"pagedown", MkKey::PAGEDOWN},
		{"insert", MkKey::INSERT},
		{"f1", MkKey::F1},
		{"f2", MkKey::F2},
		{"f3", MkKey::F3},
		{"f4", MkKey::F4},
		{"f5", MkKey::F5},
		{"f6", MkKey::F6},
		{"f7", MkKey::F7},
		{"f8", MkKey::F8},
		{"f9", MkKey::F9},
		{"f10", MkKey::F10},
		{"f11", MkKey::F11},
		{"f12", MkKey::F12},
		{"ctrl", MkKey::LEFT_CTRL},
		{"shift", MkKey::LEFT_SHIFT},
		{"alt", MkKey::LEFT_ALT},
	};

	std::string lowered= name;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });

	const auto it= k_namedKeys.find(lowered);
	if (it != k_namedKeys.end())
	{
		outKeySym= it->second;
		return true;
	}

	// A single printable character presses the key that produces it unshifted
	if (lowered.size() == 1 && lowered[0] > 0x20 && lowered[0] < 0x7F)
	{
		outKeySym= (MkKeySym)lowered[0];
		return true;
	}

	return false;
}

bool parseKeyModifier(const std::string& name, uint16_t& outKeyMod)
{
	if (name == "shift")
		outKeyMod= MkKeyMod::SHIFT;
	else if (name == "ctrl")
		outKeyMod= MkKeyMod::CTRL;
	else if (name == "alt")
		outKeyMod= MkKeyMod::ALT;
	else if (name == "gui")
		outKeyMod= MkKeyMod::GUI;
	else
		return false;

	return true;
}

bool parseMouseButtonName(const std::string& name, int& outMkMouseButton)
{
	if (name == "left")
		outMkMouseButton= MkMouseButton::LEFT;
	else if (name == "middle")
		outMkMouseButton= MkMouseButton::MIDDLE;
	else if (name == "right")
		outMkMouseButton= MkMouseButton::RIGHT;
	else
		return false;

	return true;
}

const char* referrerKindName(ProjectAssetReferrer::Kind kind)
{
	switch (kind)
	{
	case ProjectAssetReferrer::Kind::graph:
		return "graph";
	case ProjectAssetReferrer::Kind::material:
		return "material";
	case ProjectAssetReferrer::Kind::component:
		return "component";
	default:
		return "unknown";
	}
}

// Resolve a (system name, component id) command target.
// A component id of -1 targets the system itself (outComponent stays null).
bool resolveCommandTarget(MainWindow* mainWindow, const std::string& systemName, const std::string& componentIdToken,
						  MikanObjectSystemPtr& outSystem, MikanComponentPtr& outComponent, std::string& outError)
{
	outSystem= mainWindow->getProjectManager()->getSystemByName(systemName);
	if (!outSystem)
	{
		outError= "unknown system '" + systemName + "'";
		return false;
	}

	int componentId= -1;
	if (!parseComponentId(componentIdToken, componentId))
	{
		outError= "invalid component id '" + componentIdToken + "'";
		return false;
	}

	if (componentId != -1)
	{
		outComponent= outSystem->getComponentById(componentId);
		if (!outComponent)
		{
			outError= "no component with id " + std::to_string(componentId) + " in system '" + systemName + "'";
			return false;
		}
	}

	return true;
}

// Resolve a path argument the way the other path-taking commands do: a relative
// path against the project (then the app resources), otherwise against the
// working directory. A path that exists nowhere still resolves, so the caller's
// loader reports the miss.
std::filesystem::path resolvePathArgument(const std::string& pathText)
{
	const std::filesystem::path path(pathText);

	std::filesystem::path resolved= PathUtils::resolveProjectResource(path);
	if (resolved.empty())
	{
		resolved= std::filesystem::absolute(path);
	}

	return resolved;
}

// One `error <nodeId> <message>` line per material compile error
void appendCompileErrorLines(const std::vector<NodeEvaluationError>& errors, std::vector<std::string>& outLines)
{
	for (const NodeEvaluationError& error : errors)
	{
		outLines.push_back("error " + std::to_string(error.errorNodeId) + " " + error.errorMessage);
	}
}

// nodegraph open material <graphPath> | new [compositor|shape]
bool openMaterialGraphEditor(const std::vector<std::string>& args, std::string& outError)
{
	if (args.size() < 3)
	{
		outError= "usage: nodegraph open material <graphPath>|new [compositor|shape]";
		return false;
	}

	const bool bNewGraph= args[2] == "new";
	eMaterialDomain domain= eMaterialDomain::compositor;
	std::filesystem::path graphPath;
	if (bNewGraph)
	{
		if (args.size() >= 4)
		{
			domain= MaterialDomainUtils::domainFromString(args[3]);
			if (domain == eMaterialDomain::INVALID)
			{
				outError= "unknown material domain '" + args[3] + "' (compositor|shape)";
				return false;
			}
		}
	}
	else
	{
		graphPath= resolvePathArgument(args[2]);
	}

	// One material editor at a time: an open window is reused and raised, a new
	// one gets its graph installed right after creation the way the compositor
	// window's bind does (the window's startup creates no graph of its own)
	App* app= App::getInstance();
	MaterialNodeEditorWindow* window= app->getWindowOfType<MaterialNodeEditorWindow>();
	if (window == nullptr)
	{
		window= app->createAppWindow<MaterialNodeEditorWindow>();
		if (window == nullptr)
		{
			outError= "failed to create the material editor window";
			return false;
		}
	}
	else
	{
		window->getMkWindowContext()->raiseWindow();
	}

	if (bNewGraph)
	{
		window->newMaterialGraph(domain);
	}
	else if (!window->openMaterialGraph(graphPath))
	{
		outError= "failed to open material graph '" + graphPath.string() + "'";
		return false;
	}

	return true;
}
} // namespace

AutomationServer::AutomationServer()= default;

AutomationServer::~AutomationServer() { shutdown(); }

void AutomationServer::initialize(MainWindow* mainWindow)
{
	m_mainWindow= mainWindow;

	registerCoreNamespaces();
}

bool AutomationServer::startListener(uint16_t port)
{
	m_socket= std::make_unique<AutomationSocket>(port);
	m_socket->onLineReceived= [this](const std::string& line) { handleCommandLine(line); };

	if (!m_socket->isListening())
	{
		// A failed bind is tolerated: the editor runs fine without the
		// automation channel, it just cannot be driven externally
		m_socket= nullptr;
		return false;
	}

	m_listenPort= port;

	return true;
}

void AutomationServer::stopListener() { m_socket= nullptr; }

void AutomationServer::poll()
{
	if (m_socket != nullptr)
	{
		m_socket->poll();
	}
}

void AutomationServer::shutdown()
{
	m_socket= nullptr;
	m_commandProviders.clear();
	m_mainWindow= nullptr;
}

void AutomationServer::registerCommandNamespace(const std::string& namespaceName,
												const std::vector<std::string>& helpLines, CommandHandler handler)
{
	m_commandProviders[namespaceName]= {helpLines, handler};
}

// ---- Dispatch ----------------------------------------------------------------

void AutomationServer::handleCommandLine(const std::string& line)
{
	std::vector<std::string> tokens;
	std::string parseError;
	if (!AutomationProtocol::tokenizeCommandLine(line, tokens, parseError))
	{
		sendErrorReply("parse: " + parseError);
		return;
	}

	// An empty line is a no-op that still answers, so a client stays in sync
	if (tokens.empty())
	{
		sendReply({});
		return;
	}

	const std::string namespaceName= tokens[0];
	auto providerIter= m_commandProviders.find(namespaceName);
	if (providerIter == m_commandProviders.end())
	{
		std::string validNames;
		for (const auto& [name, provider] : m_commandProviders)
		{
			if (!validNames.empty())
				validNames+= ", ";
			validNames+= name;
		}
		sendErrorReply("unknown command '" + namespaceName + "'; valid namespaces: " + validNames);
		return;
	}

	const std::vector<std::string> args(tokens.begin() + 1, tokens.end());
	std::vector<std::string> outLines;
	std::string outError;
	m_currentCommandLine= line;
	m_bReplyDeferred= false;
	if (providerIter->second.handler(args, outLines, outError))
	{
		if (!m_bReplyDeferred)
			sendReply(outLines);
	}
	else
	{
		std::string errorLine= namespaceName;
		if (!args.empty())
			errorLine+= " " + args[0];
		errorLine+= ": " + (outError.empty() ? std::string("failed") : outError);
		sendErrorReply(errorLine);
	}
}

void AutomationServer::sendReply(const std::vector<std::string>& contentLines)
{
	if (m_socket != nullptr)
	{
		m_socket->sendText(AutomationProtocol::frameReply(contentLines));
	}
}

void AutomationServer::sendErrorReply(const std::string& errorLine) { sendReply({errorLine}); }

void AutomationServer::sendDeferredReply(const std::vector<std::string>& contentLines, bool bIsError)
{
	if (bIsError)
		sendErrorReply(contentLines.empty() ? std::string("failed") : contentLines.front());
	else
		sendReply(contentLines);
}

// ---- Built-in command namespaces -----------------------------------------------

void AutomationServer::registerCoreNamespaces()
{
	using namespace std::placeholders;

	registerCommandNamespace("help", {"help"}, std::bind(&AutomationServer::handleHelpCommand, this, _1, _2, _3));

	registerCommandNamespace("app",
							 {"app info", "app push <stageName>", "app pop", "app open <projectPath>",
							  "app new <projectPath>", "app resume", "app quit", "app crash <kind>"},
							 std::bind(&AutomationServer::handleAppCommand, this, _1, _2, _3));

	registerCommandNamespace("stage", {"stage <command> [parameters...]"},
							 std::bind(&AutomationServer::handleStageCommand, this, _1, _2, _3));

	registerCommandNamespace("system", {"system list"},
							 std::bind(&AutomationServer::handleSystemCommand, this, _1, _2, _3));

	registerCommandNamespace("component",
							 {"component list <system> [componentClass]", "component create <system> <componentClass>",
							  "component destroy <system> <componentId>"},
							 std::bind(&AutomationServer::handleComponentCommand, this, _1, _2, _3));

	registerCommandNamespace("property",
							 {"property list [system] [componentClass]", "property get <system> <componentId> <name>",
							  "property set <system> <componentId> <name> <value...>"},
							 std::bind(&AutomationServer::handlePropertyCommand, this, _1, _2, _3));

	registerCommandNamespace(
		"function", {"function list [system] [componentClass]", "function invoke <system> <componentId> <name>"},
		std::bind(&AutomationServer::handleFunctionCommand, this, _1, _2, _3));

	registerCommandNamespace("screenshot",
							 {"screenshot compositor [componentId] [path]", "screenshot window [windowIndex] [path]"},
							 std::bind(&AutomationServer::handleScreenshotCommand, this, _1, _2, _3));

	registerCommandNamespace("window", {"window list", "window focus <windowIndex>"},
							 std::bind(&AutomationServer::handleWindowCommand, this, _1, _2, _3));

	registerCommandNamespace(
		"input",
		{"input move <windowIndex> <x> <y>", "input click <windowIndex> <x> <y> [left|middle|right] [clickCount]",
		 "input press|release <windowIndex> <x> <y> [left|middle|right]",
		 "input wheel <windowIndex> <x> <y> <scrollY> [scrollX]",
		 "input key|keydown|keyup <windowIndex> <keyName> [modifiers...]", "input text <windowIndex> <text...>"},
		std::bind(&AutomationServer::handleInputCommand, this, _1, _2, _3));

	registerCommandNamespace("script",
							 {"script list", "script eval <lua-code>",
							  "script trigger <triggerName> [script=<scriptName>] [key=value ...]", "script reload",
							  "script routes"},
							 std::bind(&AutomationServer::handleScriptCommand, this, _1, _2, _3));

	registerCommandNamespace("log", {"log tail <lineCount> [trace|debug|info|warning|error|fatal]"},
							 std::bind(&AutomationServer::handleLogCommand, this, _1, _2, _3));

	registerCommandNamespace("nodegraph",
							 {"nodegraph open [compositorComponentId]", "nodegraph open material <graphPath>",
							  "nodegraph open material new [compositor|shape]", "nodegraph close", "nodegraph info",
							  "nodegraph list nodes|pins|links|properties|pages", "nodegraph page [pageId]",
							  "nodegraph createpage <pageClassName>", "nodegraph deletepage <pageId>",
							  "nodegraph createnode <nodeClassName> [x y]", "nodegraph deletenode <nodeId>",
							  "nodegraph createlink <startPinId> <endPinId>", "nodegraph deletelink <linkId>",
							  "nodegraph undo [n]", "nodegraph redo [n]", "nodegraph run on|off", "nodegraph compile",
							  "nodegraph renamevar <propertyId> <name...>",
							  "nodegraph reordervar <movedPropertyId> <targetPropertyId>"},
							 std::bind(&AutomationServer::handleNodeGraphCommand, this, _1, _2, _3));

	registerCommandNamespace("material", {"material info <matPath>", "material compile <graphPath>"},
							 std::bind(&AutomationServer::handleMaterialCommand, this, _1, _2, _3));

	registerCommandNamespace("assets",
							 {"assets folders", "assets list [folderId]", "assets refresh",
							  "assets import <folderId> <sourcePath>", "assets refs <storedPath>",
							  "assets delete <folderId> <storedPath>", "assets select <folderId> <storedPath>",
							  "assets open <folderId> <storedPath>", "assets selected", "assets folder"},
							 std::bind(&AutomationServer::handleAssetsCommand, this, _1, _2, _3));

	// The history namespace is registered by TransactionHistory after startup
}

bool AutomationServer::handleHelpCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										 std::string& outError)
{
	for (const auto& [name, provider] : m_commandProviders)
	{
		for (const std::string& helpLine : provider.helpLines)
		{
			outLines.push_back(helpLine);
		}
	}

	return true;
}

bool AutomationServer::handleAppCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: app info|push|pop|open|new|resume|quit|crash";
		return false;
	}

	const std::string& verb= args[0];
	AppStage* currentAppStage= m_mainWindow->getCurrentAppStage();

	if (verb == "info")
	{
		AppStage* parentAppStage= m_mainWindow->getParentAppStage();
		ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();

		outLines.push_back("stage " + (currentAppStage != nullptr ? currentAppStage->getAppStageName() : "none"));
		outLines.push_back("parent " + (parentAppStage != nullptr ? parentAppStage->getAppStageName() : "none"));
		outLines.push_back("project "
						   + (projectManager->hasLoadedProject()
								  ? projectManager->getProjectConfig()->getLoadedConfigPath().string()
								  : "none"));
		return true;
	}
	else if (verb == "push")
	{
		if (args.size() < 2)
		{
			outError= "usage: app push <stageName>";
			return false;
		}

		const std::string& desiredStageName= args[1];
		if (currentAppStage != nullptr && currentAppStage->getAppStageName() == desiredStageName)
			return true;

		if (m_mainWindow->pushAppStage(desiredStageName) == nullptr)
		{
			outError= "unknown app stage '" + desiredStageName + "'";
			return false;
		}

		// The stage transition lands on the next frame (pending app stage ops)
		return true;
	}
	else if (verb == "pop")
	{
		if (m_mainWindow->getParentAppStage() == nullptr)
		{
			outError= "no parent app stage to pop to";
			return false;
		}

		m_mainWindow->popAppState();
		return true;
	}
	else if (verb == "open" || verb == "new" || verb == "resume")
	{
		// These route to the current stage's remote control commands,
		// which exist on the main menu stage
		static const std::map<std::string, std::string> k_stageCommands= {
			{"open", "open_project"}, {"new", "new_project"}, {"resume", "resume_project"}};

		std::vector<std::string> parameters(args.begin() + 1, args.end());
		std::vector<std::string> stageArgs= {k_stageCommands.at(verb)};
		stageArgs.insert(stageArgs.end(), parameters.begin(), parameters.end());

		return handleStageCommand(stageArgs, outLines, outError);
	}
	else if (verb == "quit")
	{
		App::getInstance()->requestShutdown();
		return true;
	}
	else if (verb == "crash")
	{
		// The process dies before any reply, so the client sees the socket close
		if (args.size() < 2)
		{
			outError= std::string("usage: app crash <kind>, kinds: ") + CrashHandler::getTestCrashKinds();
			return false;
		}

		if (!CrashHandler::triggerTestCrash(args[1]))
		{
			outError= "unknown crash kind '" + args[1] + "', kinds: " + CrashHandler::getTestCrashKinds();
			return false;
		}

		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleStageCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										  std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: stage <command> [parameters...]";
		return false;
	}

	AppStage* currentAppStage= m_mainWindow->getCurrentAppStage();
	if (currentAppStage == nullptr)
	{
		outError= "no current app stage";
		return false;
	}

	const std::string& command= args[0];
	const std::vector<std::string> parameters(args.begin() + 1, args.end());

	std::vector<std::string> results;
	if (!currentAppStage->handleRemoteControlCommand(command, parameters, results))
	{
		outError= "command '" + command + "' not handled by stage '" + currentAppStage->getAppStageName() + "'";
		if (!results.empty())
			outError+= " (" + results[0] + ")";
		return false;
	}

	outLines= results;
	return true;
}

bool AutomationServer::handleSystemCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										   std::string& outError)
{
	if (args.empty() || args[0] != "list")
	{
		outError= "usage: system list";
		return false;
	}

	for (const MikanObjectSystemPtr& system : m_mainWindow->getProjectManager()->getSystems())
	{
		outLines.push_back(system->getObjectSystemClassName());
	}

	return true;
}

bool AutomationServer::handleComponentCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											  std::string& outError)
{
	if (args.size() < 2)
	{
		outError= "usage: component list|create|destroy <system> ...";
		return false;
	}

	const std::string& verb= args[0];
	const std::string& systemName= args[1];
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	MikanObjectSystemPtr objectSystem= projectManager->getSystemByName(systemName);
	if (!objectSystem)
	{
		outError= "unknown system '" + systemName + "'";
		return false;
	}

	if (verb == "create")
	{
		if (args.size() < 3)
		{
			outError= "usage: component create <system> <componentClass>";
			return false;
		}

		MikanComponentPtr component= objectSystem->addNewObjectWithDefaultDefinition(args[2]);
		if (!component)
		{
			outError= "system '" + systemName + "' cannot create a '" + args[2] + "'";
			return false;
		}

		outLines.push_back(std::to_string(component->getComponentId()));
		return true;
	}
	else if (verb == "destroy")
	{
		if (args.size() < 3)
		{
			outError= "usage: component destroy <system> <componentId>";
			return false;
		}

		int componentId= -1;
		if (!parseComponentId(args[2], componentId) || componentId == -1)
		{
			outError= "invalid component id '" + args[2] + "'";
			return false;
		}

		MikanComponentPtr component= objectSystem->getComponentById(componentId);
		if (!component)
		{
			outError= "no component with id " + std::to_string(componentId) + " in system '" + systemName + "'";
			return false;
		}

		if (!component->destroyOwnerObject())
		{
			outError= "failed to destroy component " + std::to_string(componentId);
			return false;
		}

		return true;
	}
	else if (verb != "list")
	{
		outError= "unknown verb '" + verb + "'";
		return false;
	}

	// Component classes come from the property database, which registers
	// every component class each system owns
	std::vector<std::string> componentClassNames;
	if (args.size() >= 3)
	{
		componentClassNames.push_back(args[2]);
	}
	else
	{
		for (const MikanPropertyEntry& entry : projectManager->getPropertyDatabaseConst()->getAllProperties())
		{
			if (entry.systemName != systemName || entry.componentClassName.empty())
				continue;

			if (std::find(componentClassNames.begin(), componentClassNames.end(), entry.componentClassName)
				== componentClassNames.end())
			{
				componentClassNames.push_back(entry.componentClassName);
			}
		}
	}

	for (const std::string& componentClassName : componentClassNames)
	{
		std::vector<int> componentIds;
		if (!objectSystem->getComponentIdList(componentClassName, componentIds))
			continue;

		for (int componentId : componentIds)
		{
			outLines.push_back(std::to_string(componentId) + " " + componentClassName);
		}
	}

	return true;
}

bool AutomationServer::handlePropertyCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											 std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: property list|get|set ...";
		return false;
	}

	const std::string& verb= args[0];
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();
	MikanPropertyDatabaseConstPtr propertyDatabase= projectManager->getPropertyDatabaseConst();

	if (verb == "list")
	{
		const std::string systemFilter= args.size() >= 2 ? args[1] : "";
		const std::string componentFilter= args.size() >= 3 ? args[2] : "";

		PropertyDatabaseEnumerator enumerator(propertyDatabase, systemFilter, componentFilter, "");
		while (enumerator.isValid())
		{
			const MikanPropertyEntry* entry= propertyDatabase->getPropertyByIndex(enumerator.getCurrentPropertyIndex());
			const std::string componentClassName= !entry->componentClassName.empty() ? entry->componentClassName : "-";

			outLines.push_back(entry->systemName + " " + componentClassName + " " + entry->descriptor->getName() + " "
							   + mikanVariantTypeToString(entry->descriptor->getDataType()) + " "
							   + (entry->descriptor->isReadOnly() ? "ro" : "rw"));

			enumerator.next();
		}

		return true;
	}
	else if (verb == "get" || verb == "set")
	{
		if (args.size() < 4)
		{
			outError=
				"usage: property " + verb + " <system> <componentId> <name>" + (verb == "set" ? " <value...>" : "");
			return false;
		}

		MikanObjectSystemPtr objectSystem;
		MikanComponentPtr component;
		if (!resolveCommandTarget(m_mainWindow, args[1], args[2], objectSystem, component, outError))
			return false;

		const std::string& propertyName= args[3];
		IPropertyInterface* propertyInterface= component != nullptr
												   ? static_cast<IPropertyInterface*>(component.get())
												   : static_cast<IPropertyInterface*>(objectSystem.get());

		if (verb == "get")
		{
			MikanVariant value;
			if (!propertyInterface->getPropertyValue(propertyName, value))
			{
				outError= "unknown property '" + propertyName + "'";
				return false;
			}

			outLines.push_back(AutomationVariantText::variantToText(value));
			return true;
		}
		else
		{
			if (args.size() < 5)
			{
				outError= "usage: property set <system> <componentId> <name> <value...>";
				return false;
			}

			// The descriptor supplies the target type for text coercion
			const std::string componentClassName= component != nullptr ? component->getComponentClassName() : "";
			PropertyDescriptorConstPtr descriptor=
				propertyDatabase->findPropertyDescriptor(args[1], componentClassName, propertyName);
			if (!descriptor)
			{
				outError= "unknown property '" + propertyName + "'";
				return false;
			}

			if (descriptor->isReadOnly())
			{
				outError= "property '" + propertyName + "' is read only";
				return false;
			}

			const std::vector<std::string> valueTokens(args.begin() + 4, args.end());
			MikanVariant value;
			if (!AutomationVariantText::textToVariant(descriptor->getDataType(), valueTokens, value, outError))
				return false;

			if (!propertyInterface->setPropertyValue(propertyName, value))
			{
				outError= "failed to set property '" + propertyName + "'";
				return false;
			}

			return true;
		}
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleScreenshotCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											   std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: screenshot compositor|window [path]";
		return false;
	}

	const std::string& verb= args[0];

	if (verb == "compositor")
	{
		// Optional compositor component id, then optional output path
		size_t nextArg= 1;
		int compositorId= -1;
		if (args.size() > nextArg && parseComponentId(args[nextArg], compositorId))
			++nextArg;
		const std::string path= args.size() > nextArg ? args[nextArg] : "mikan_compositor.png";

		auto compositorSystem= m_mainWindow->getProjectManager()->getSystemOfType<CompositorObjectSystem>();
		if (!compositorSystem)
		{
			outError= "no compositor system";
			return false;
		}

		if (compositorId == -1)
		{
			// Default to the project's single compositor; several need an explicit id
			std::vector<int> compositorIds;
			compositorSystem->getComponentIdList(CompositorComponent::k_componentClassName, compositorIds);
			if (compositorIds.size() != 1)
			{
				outError= "give a compositor component id (project has " + std::to_string(compositorIds.size())
						  + " compositors)";
				return false;
			}
			compositorId= compositorIds[0];
		}

		CompositorComponentPtr compositor= compositorSystem->getCompositorById(compositorId);
		if (!compositor)
		{
			outError= "no compositor with id " + std::to_string(compositorId);
			return false;
		}

		IMkTexturePtr frameTexture= compositor->getCompositedFrameTextureMutable();
		if (!frameTexture)
		{
			outError= "no composited frame available";
			return false;
		}

		if (!saveMkTextureToPNG(frameTexture, path.c_str()))
		{
			outError= "failed to write '" + path + "'";
			return false;
		}

		outLines.push_back(std::filesystem::absolute(path).string());
		return true;
	}
	else if (verb == "window")
	{
		if (m_bWindowCapturePending)
		{
			outError= "a window capture is already pending";
			return false;
		}

		// Optional window index, then optional output path. Bare `screenshot window` keeps meaning
		// the main window, so drives written before other windows were capturable still work.
		size_t nextArg= 1;
		EditorWindow* targetWindow= nullptr;
		int windowIndex= -1;
		if (args.size() > nextArg && parseComponentId(args[nextArg], windowIndex))
		{
			targetWindow= resolveWindowIndex(args[nextArg], outError);
			if (!targetWindow)
				return false;
			++nextArg;
		}
		else
		{
			targetWindow= m_mainWindow;
		}

		// Park the capture until the target window's render completes; the reply is
		// sent from servicePendingWindowCapture
		m_windowCapturePath= args.size() > nextArg ? args[nextArg] : "mikan_window.png";
		m_windowCaptureTarget= targetWindow;
		m_bWindowCapturePending= true;
		m_bReplyDeferred= true;
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleNodeGraphCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											  std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: nodegraph open|close|info|list|page|createpage|deletepage|createnode|deletenode|"
				  "createlink|deletelink|undo|redo|run|compile|renamevar|reordervar";
		return false;
	}

	const std::string& verb= args[0];

	if (verb == "open")
	{
		// `open material ...` targets the material editor, which binds to no component
		if (args.size() >= 2 && args[1] == "material")
		{
			return openMaterialGraphEditor(args, outError);
		}

		// Optional compositor component id; default to the project's single compositor
		int compositorId= -1;
		if (args.size() >= 2 && !parseComponentId(args[1], compositorId))
		{
			outError= "invalid compositor component id '" + args[1] + "'";
			return false;
		}

		auto compositorSystem= m_mainWindow->getProjectManager()->getSystemOfType<CompositorObjectSystem>();
		if (!compositorSystem)
		{
			outError= "no compositor system";
			return false;
		}

		if (compositorId == -1)
		{
			std::vector<int> compositorIds;
			compositorSystem->getComponentIdList(CompositorComponent::k_componentClassName, compositorIds);
			if (compositorIds.size() != 1)
			{
				outError= "give a compositor component id (project has " + std::to_string(compositorIds.size())
						  + " compositors)";
				return false;
			}
			compositorId= compositorIds[0];
		}

		CompositorComponentPtr compositor= compositorSystem->getCompositorById(compositorId);
		if (!compositor)
		{
			outError= "no compositor with id " + std::to_string(compositorId);
			return false;
		}

		// Opens the compositor node editor window if one is not already open
		compositor->editCompositorGraph();
		return true;
	}

	// Every other verb targets the open node editor window
	NodeEditorWindow* window= App::getInstance()->getWindowOfType<NodeEditorWindow>();
	if (window == nullptr)
	{
		outError= "no node editor window open";
		return false;
	}

	if (verb == "close")
	{
		// Torn down by the app at the end of this frame
		window->requestClose();
		return true;
	}

	NodeGraphPtr nodeGraph= window->getNodeGraph();
	if (!nodeGraph)
	{
		outError= "no graph loaded";
		return false;
	}

	if (verb == "info")
	{
		const std::string& path= window->getNodeGraphPath().string();

		outLines.push_back("class " + nodeGraph->getClassName());
		outLines.push_back("path " + (path.empty() ? std::string("none") : path));
		outLines.push_back("nodes " + std::to_string(nodeGraph->getNodesMap().size()));
		outLines.push_back("pins " + std::to_string(nodeGraph->getPinsMap().size()));
		outLines.push_back("links " + std::to_string(nodeGraph->getLinksMap().size()));
		outLines.push_back("properties " + std::to_string(nodeGraph->getPropertyMap().size()));
		outLines.push_back("page " + std::to_string(window->getCurrentPageId()));
		// The implicit root page is not counted
		outLines.push_back("pages " + std::to_string(nodeGraph->getPages().size()));
		outLines.push_back(std::string("can_undo ") + (window->canUndo() ? "true" : "false"));
		outLines.push_back(std::string("can_redo ") + (window->canRedo() ? "true" : "false"));
		outLines.push_back("history_depth " + std::to_string(window->getHistory().getDepth()));
		outLines.push_back("history_cursor " + std::to_string(window->getHistory().getCursor()));

		const std::string logPath= window->getGraphLogFilePath().string();
		outLines.push_back("log " + (logPath.empty() ? std::string("none") : logPath));

		auto* compositorWindow= dynamic_cast<CompositorNodeEditorWindow*>(window);
		if (compositorWindow != nullptr)
		{
			outLines.push_back(std::string("running ") + (compositorWindow->isCompositorRunning() ? "true" : "false"));
		}

		auto* materialWindow= dynamic_cast<MaterialNodeEditorWindow*>(window);
		if (materialWindow != nullptr)
		{
			MaterialNodeGraphPtr materialGraph= materialWindow->getMaterialNodeGraph();
			if (materialGraph)
			{
				outLines.push_back("domain " + MaterialDomainUtils::domainToString(materialGraph->getDomain()));
				outLines.push_back("vertex_preset "
								   + MaterialDomainUtils::presetToString(materialGraph->getVertexPreset()));
				outLines.push_back("compile_errors " + std::to_string(materialGraph->getLastCompileErrors().size()));
			}
		}
		return true;
	}
	else if (verb == "list")
	{
		const std::string kind= args.size() >= 2 ? args[1] : "";

		if (kind == "nodes")
		{
			for (const auto& [nodeId, node] : nodeGraph->getNodesMap())
			{
				// The title may hold spaces, so the page id goes last
				outLines.push_back(std::to_string(nodeId) + " " + node->getClassName() + " " + node->editorGetTitle()
								   + " " + std::to_string(node->getPageId()));
			}
		}
		else if (kind == "pages")
		{
			// The implicit root first, then the created pages in id order
			outLines.push_back(std::to_string(NodeGraph::k_rootPageId) + " root Main");
			for (const auto& [pageId, page] : nodeGraph->getPages())
			{
				outLines.push_back(std::to_string(pageId) + " " + page->getClassName() + " " + page->getName());
			}
		}
		else if (kind == "pins")
		{
			for (const auto& [pinId, pin] : nodeGraph->getPinsMap())
			{
				NodePtr ownerNode= pin->getOwnerNode();
				const std::string direction= pin->getDirection() == eNodePinDirection::INPUT ? "in" : "out";

				outLines.push_back(std::to_string(pinId) + " " + pin->getClassName() + " "
								   + std::to_string(ownerNode ? ownerNode->getId() : -1) + " " + direction + " "
								   + pin->getName());
			}
		}
		else if (kind == "links")
		{
			for (const auto& [linkId, link] : nodeGraph->getLinksMap())
			{
				NodePinPtr startPin= link->getStartPin();
				NodePinPtr endPin= link->getEndPin();

				outLines.push_back(std::to_string(linkId) + " " + std::to_string(startPin ? startPin->getId() : -1)
								   + " " + std::to_string(endPin ? endPin->getId() : -1));
			}
		}
		else if (kind == "properties")
		{
			// Listed in variable-list order so a reorder is observable here
			for (GraphPropertyPtr property : nodeGraph->getPropertiesInSortOrder())
			{
				outLines.push_back(std::to_string(property->getId()) + " " + property->getClassName() + " "
								   + property->getName());
			}
		}
		else
		{
			outError= "usage: nodegraph list nodes|pins|links|properties|pages";
			return false;
		}

		return true;
	}
	else if (verb == "createnode")
	{
		if (args.size() < 2)
		{
			outError= "usage: nodegraph createnode <nodeClassName> [x y]";
			return false;
		}

		const std::string nodeClassName= args[1];
		glm::vec2 gridPos(0.f, 0.f);
		if (args.size() >= 4)
		{
			gridPos.x= (float)atof(args[2].c_str());
			gridPos.y= (float)atof(args[3].c_str());
		}

		// Node creation must run inside the window's update, where its GL and
		// gui contexts can be made current; the reply is sent from the task
		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, nodeClassName, gridPos]()
			{
				t_node_id newNodeId= -1;
				std::string error;
				if (window->automationCreateNode(nodeClassName, gridPos, newNodeId, error))
				{
					sendReply({std::to_string(newNodeId)});
				}
				else
				{
					sendErrorReply("nodegraph createnode: " + error);
				}
			});
		return true;
	}
	else if (verb == "deletenode")
	{
		int nodeId= -1;
		if (args.size() < 2 || !parseComponentId(args[1], nodeId))
		{
			outError= "usage: nodegraph deletenode <nodeId>";
			return false;
		}

		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, nodeId]()
			{
				std::string error;
				if (window->automationDeleteNode(nodeId, error))
				{
					sendReply({});
				}
				else
				{
					sendErrorReply("nodegraph deletenode: " + error);
				}
			});
		return true;
	}
	else if (verb == "createlink")
	{
		int startPinId= -1;
		int endPinId= -1;
		if (args.size() < 3 || !parseComponentId(args[1], startPinId) || !parseComponentId(args[2], endPinId))
		{
			outError= "usage: nodegraph createlink <startPinId> <endPinId>";
			return false;
		}

		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, startPinId, endPinId]()
			{
				t_node_link_id newLinkId= -1;
				std::string error;
				if (window->automationCreateLink(startPinId, endPinId, newLinkId, error))
				{
					sendReply({std::to_string(newLinkId)});
				}
				else
				{
					sendErrorReply("nodegraph createlink: " + error);
				}
			});
		return true;
	}
	else if (verb == "deletelink")
	{
		int linkId= -1;
		if (args.size() < 2 || !parseComponentId(args[1], linkId))
		{
			outError= "usage: nodegraph deletelink <linkId>";
			return false;
		}

		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, linkId]()
			{
				std::string error;
				if (window->automationDeleteLink(linkId, error))
				{
					sendReply({});
				}
				else
				{
					sendErrorReply("nodegraph deletelink: " + error);
				}
			});
		return true;
	}
	else if (verb == "page")
	{
		if (args.size() < 2)
		{
			outLines.push_back(std::to_string(window->getCurrentPageId()));
			return true;
		}

		int pageId= -1;
		if (!parseComponentId(args[1], pageId))
		{
			outError= "usage: nodegraph page [pageId]";
			return false;
		}

		// The canvas rebinds inside the window's update; replies the resulting page id
		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, pageId]()
			{
				std::string error;
				if (window->automationSetCurrentPage(pageId, error))
				{
					sendReply({std::to_string(window->getCurrentPageId())});
				}
				else
				{
					sendErrorReply("nodegraph page: " + error);
				}
			});
		return true;
	}
	else if (verb == "createpage")
	{
		if (args.size() < 2)
		{
			outError= "usage: nodegraph createpage <pageClassName>";
			return false;
		}

		const std::string pageClassName= args[1];

		// A new page may seed nodes, which needs the window's GL and gui contexts current
		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, pageClassName]()
			{
				t_graph_page_id newPageId= -1;
				std::string error;
				if (window->automationCreatePage(pageClassName, newPageId, error))
				{
					sendReply({std::to_string(newPageId)});
				}
				else
				{
					sendErrorReply("nodegraph createpage: " + error);
				}
			});
		return true;
	}
	else if (verb == "deletepage")
	{
		int pageId= -1;
		if (args.size() < 2 || !parseComponentId(args[1], pageId))
		{
			outError= "usage: nodegraph deletepage <pageId>";
			return false;
		}

		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, pageId]()
			{
				std::string error;
				if (window->automationDeletePage(pageId, error))
				{
					sendReply({});
				}
				else
				{
					sendErrorReply("nodegraph deletepage: " + error);
				}
			});
		return true;
	}
	else if (verb == "undo" || verb == "redo")
	{
		int requestedSteps= 1;
		if (args.size() >= 2 && (!parseComponentId(args[1], requestedSteps) || requestedSteps <= 0))
		{
			outError= "usage: nodegraph " + verb + " [n]";
			return false;
		}

		const int steps= (verb == "undo") ? -requestedSteps : requestedSteps;

		// Applied inside the window's update; replies the resulting cursor
		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, window, steps]()
			{
				window->stepHistory(steps);
				sendReply({std::to_string(window->getHistory().getCursor())});
			});
		return true;
	}
	else if (verb == "renamevar")
	{
		int propertyId= -1;
		if (args.size() < 3 || !parseComponentId(args[1], propertyId))
		{
			outError= "usage: nodegraph renamevar <propertyId> <name...>";
			return false;
		}

		GraphPropertyPtr property= nodeGraph->getPropertyById(propertyId);
		if (!property)
		{
			outError= "no property with id " + std::to_string(propertyId);
			return false;
		}

		// The name is the rest of the line, rejoined so spaces survive
		std::string newName= args[2];
		for (size_t argIndex= 3; argIndex < args.size(); ++argIndex)
		{
			newName+= " " + args[argIndex];
		}

		nodeGraph->renameProperty(propertyId, newName);
		return true;
	}
	else if (verb == "reordervar")
	{
		int movedId= -1;
		int targetId= -1;
		if (args.size() < 3 || !parseComponentId(args[1], movedId) || !parseComponentId(args[2], targetId))
		{
			outError= "usage: nodegraph reordervar <movedPropertyId> <targetPropertyId>";
			return false;
		}

		if (!nodeGraph->reorderPropertyBefore(movedId, targetId))
		{
			outError= "could not move property " + std::to_string(movedId) + " before " + std::to_string(targetId);
			return false;
		}

		return true;
	}
	else if (verb == "run")
	{
		const std::string state= args.size() >= 2 ? args[1] : "";
		if (state != "on" && state != "off")
		{
			outError= "usage: nodegraph run on|off";
			return false;
		}

		auto* compositorWindow= dynamic_cast<CompositorNodeEditorWindow*>(window);
		if (compositorWindow == nullptr)
		{
			outError= "the open node editor is not a compositor graph editor";
			return false;
		}

		if (!compositorWindow->setCompositorRunning(state == "on"))
		{
			outError= "no compositor component bound";
			return false;
		}

		outLines.push_back(std::string("running ") + (compositorWindow->isCompositorRunning() ? "true" : "false"));
		return true;
	}
	else if (verb == "compile")
	{
		auto* materialWindow= dynamic_cast<MaterialNodeEditorWindow*>(window);
		if (materialWindow == nullptr)
		{
			outError= "the open node editor is not a material graph editor";
			return false;
		}

		// The compile refreshes the window's error overlay and writes beside the
		// graph file, so it runs inside the window's update like the other
		// mutations; the reply is sent from the task
		m_bReplyDeferred= true;
		window->enqueueAutomationTask(
			[this, materialWindow]()
			{
				if (materialWindow->compileAndWriteOutputs())
				{
					sendReply({"compiled"});
					return;
				}

				std::vector<std::string> errorLines;
				MaterialNodeGraphPtr materialGraph= materialWindow->getMaterialNodeGraph();
				if (materialGraph)
				{
					appendCompileErrorLines(materialGraph->getLastCompileErrors(), errorLines);
				}

				if (!errorLines.empty())
				{
					sendReply(errorLines);
				}
				else if (PathUtils::resolveProjectResource(materialWindow->getNodeGraphPath()).empty())
				{
					sendErrorReply("nodegraph compile: the graph has no file to write beside (save it first)");
				}
				else
				{
					sendErrorReply("nodegraph compile: failed to write the material outputs (see log tail)");
				}
			});
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleMaterialCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											 std::string& outError)
{
	if (args.size() < 2)
	{
		outError= "usage: material info <matPath> | material compile <graphPath>";
		return false;
	}

	const std::string& verb= args[0];
	const std::filesystem::path path= resolvePathArgument(args[1]);

	if (verb == "info")
	{
		MikanShaderConfig config;
		if (!config.load(path))
		{
			outError= "failed to load material '" + path.string() + "'";
			return false;
		}

		// A hand-authored .mat carries no domain, preset, or source graph
		outLines.push_back("name " + config.materialName);
		outLines.push_back("domain " + (config.domain.empty() ? std::string("none") : config.domain));
		outLines.push_back("vertex_preset "
						   + (config.vertexPreset.empty() ? std::string("none") : config.vertexPreset));
		outLines.push_back(
			"source_graph "
			+ (config.sourceGraphPath.empty() ? std::string("none") : config.sourceGraphPath.generic_string()));
		for (const auto& [name, semantic] : config.uniformSemanticMap)
		{
			outLines.push_back("uniform " + name + " " + semantic);
		}

		return true;
	}
	else if (verb == "compile")
	{
		// Headless: with no owner window the graph allocates no GL resources, and
		// the compile touches nothing an editor window owns
		NodeGraphPtr nodeGraph= NodeGraphFactory::loadNodeGraph(nullptr, path);
		MaterialNodeGraphPtr materialGraph= std::dynamic_pointer_cast<MaterialNodeGraph>(nodeGraph);
		if (!materialGraph)
		{
			outError= nodeGraph ? "'" + path.string() + "' is not a material graph"
								: "failed to load graph '" + path.string() + "'";
			return false;
		}

		GlslShaderWriter writer;
		MaterialCompileResult result= materialGraph->compile(writer);
		if (result.hasErrors())
		{
			appendCompileErrorLines(result.errors, outLines);
			return true;
		}

		if (!MaterialCompiler::writeOutputs(result, path, outError))
		{
			return false;
		}

		outLines.push_back(MaterialCompiler::getVertexShaderPathForGraph(path).string());
		outLines.push_back(MaterialCompiler::getFragmentShaderPathForGraph(path).string());
		outLines.push_back(MaterialCompiler::getMaterialPathForGraph(path, materialGraph->getDomain()).string());
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

EditorWindow* AutomationServer::resolveWindowIndex(const std::string& indexText, std::string& outError) const
{
	int windowIndex= -1;
	if (!parseComponentId(indexText, windowIndex))
	{
		outError= "invalid window index '" + indexText + "'";
		return nullptr;
	}

	const std::vector<EditorWindow*>& appWindows= m_mainWindow->getOwnerApp()->getAppWindows();
	if (windowIndex < 0 || windowIndex >= (int)appWindows.size())
	{
		outError= "no window at index " + std::to_string(windowIndex)
				  + " (open windows: " + std::to_string(appWindows.size()) + ")";
		return nullptr;
	}

	return appWindows[windowIndex];
}

bool AutomationServer::handleWindowCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										   std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: window list|focus ...";
		return false;
	}

	const std::string& verb= args[0];

	if (verb == "list")
	{
		const std::vector<EditorWindow*>& appWindows= m_mainWindow->getOwnerApp()->getAppWindows();

		for (size_t windowIndex= 0; windowIndex < appWindows.size(); ++windowIndex)
		{
			EditorWindow* window= appWindows[windowIndex];

			outLines.push_back(std::to_string(windowIndex) + " " + std::to_string((int)window->getWidth()) + "x"
							   + std::to_string((int)window->getHeight()) + " " + window->getTitle());
		}

		return true;
	}
	else if (verb == "focus")
	{
		if (args.size() < 2)
		{
			outError= "usage: window focus <windowIndex>";
			return false;
		}

		EditorWindow* window= resolveWindowIndex(args[1], outError);
		if (!window)
			return false;

		window->getMkWindowContext()->raiseWindow();
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleInputCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										  std::string& outError)
{
	if (args.size() < 2)
	{
		outError= "usage: input move|click|wheel|key|text <windowIndex> ...";
		return false;
	}

	const std::string& verb= args[0];

	EditorWindow* window= resolveWindowIndex(args[1], outError);
	if (!window)
		return false;

	IMkWindowContextPtr windowContext= window->getMkWindowContext();

	// Injected input only reaches ImGui and the window's listener if the window is actually the one
	// receiving OS input, so every verb raises it first.
	windowContext->raiseWindow();

	if (verb == "move" || verb == "click" || verb == "press" || verb == "release" || verb == "wheel")
	{
		if (args.size() < 4)
		{
			outError= "usage: input " + verb + " <windowIndex> <x> <y> ...";
			return false;
		}

		int windowX= 0;
		int windowY= 0;
		if (!parseComponentId(args[2], windowX) || !parseComponentId(args[3], windowY))
		{
			outError= "invalid position '" + args[2] + " " + args[3] + "'";
			return false;
		}

		windowContext->warpMouseToWindowPosition(windowX, windowY);

		if (verb == "move")
			return true;

		if (verb == "wheel")
		{
			int scrollY= 0;
			int scrollX= 0;
			if (args.size() < 5 || !parseComponentId(args[4], scrollY))
			{
				outError= "usage: input wheel <windowIndex> <x> <y> <scrollY> [scrollX]";
				return false;
			}
			if (args.size() >= 6 && !parseComponentId(args[5], scrollX))
			{
				outError= "invalid scrollX '" + args[5] + "'";
				return false;
			}

			windowContext->injectMouseWheel(windowX, windowY, scrollX, scrollY);
			return true;
		}

		int mkMouseButton= MkMouseButton::LEFT;
		if (args.size() >= 5 && !parseMouseButtonName(args[4], mkMouseButton))
		{
			outError= "unknown mouse button '" + args[4] + "' (left|middle|right)";
			return false;
		}

		int clickCount= 1;
		if (args.size() >= 6 && !parseComponentId(args[5], clickCount))
		{
			outError= "invalid click count '" + args[5] + "'";
			return false;
		}

		// press and release are the halves of click, for drags and for widgets whose popup only
		// survives while the button is held
		if (verb != "release")
			windowContext->injectMouseButton(mkMouseButton, true, windowX, windowY, clickCount);
		if (verb != "press")
			windowContext->injectMouseButton(mkMouseButton, false, windowX, windowY, clickCount);

		return true;
	}
	else if (verb == "key" || verb == "keydown" || verb == "keyup")
	{
		if (args.size() < 3)
		{
			outError= "usage: input " + verb + " <windowIndex> <keyName> [modifiers...]";
			return false;
		}

		MkKeySym keySym= MkKey::UNKNOWN;
		if (!parseKeyName(args[2], keySym))
		{
			outError= "unknown key '" + args[2] + "'";
			return false;
		}

		uint16_t keyMod= MkKeyMod::NONE;
		for (size_t argIndex= 3; argIndex < args.size(); ++argIndex)
		{
			uint16_t parsedMod= MkKeyMod::NONE;
			if (!parseKeyModifier(args[argIndex], parsedMod))
			{
				outError= "unknown modifier '" + args[argIndex] + "' (shift|ctrl|alt|gui)";
				return false;
			}
			keyMod|= parsedMod;
		}

		// keydown and keyup are the halves of key, so a drive can hold a
		// modifier (keydown ctrl ctrl) across a mouse press, move, and release
		if (verb != "keyup")
			windowContext->injectKey(keySym, keyMod, true);
		if (verb != "keydown")
			windowContext->injectKey(keySym, keyMod, false);
		return true;
	}
	else if (verb == "text")
	{
		// Free text: take the remainder of the raw line so spacing and quoting survive tokenization
		const std::string text=
			AutomationProtocol::remainderAfterTokens(getCurrentCommandLine(), 3); // input text <windowIndex>
		if (text.empty())
		{
			outError= "usage: input text <windowIndex> <text...>";
			return false;
		}

		windowContext->injectText(text);
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

void AutomationServer::servicePendingWindowCapture(EditorWindow* window, int windowWidth, int windowHeight)
{
	if (!m_bWindowCapturePending || window != m_windowCaptureTarget)
		return;

	m_bWindowCapturePending= false;
	m_windowCaptureTarget= nullptr;

	if (saveDefaultFramebufferToPNG(windowWidth, windowHeight, m_windowCapturePath.c_str()))
	{
		sendReply({std::filesystem::absolute(m_windowCapturePath).string()});
	}
	else
	{
		sendErrorReply("screenshot window: failed to write '" + m_windowCapturePath + "'");
	}
}

bool AutomationServer::handleScriptCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										   std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: script list|eval|trigger|reload|routes ...";
		return false;
	}

	const std::string& verb= args[0];

	ScriptObjectSystemPtr scriptSystem= m_mainWindow->getProjectManager()->getSystemOfType<ScriptObjectSystem>();
	if (!scriptSystem)
	{
		outError= "no script system";
		return false;
	}

	if (verb == "list")
	{
		// One line per project script, in the order the scripts run
		for (ScriptDefinitionPtr definition : scriptSystem->getTypedDefinitionConst()->getAllDefinitions())
		{
			ScriptComponentPtr script= scriptSystem->getTypedComponentById(definition->getScriptId());
			if (!script)
				continue;

			std::vector<std::string> triggerNames;
			script->getTriggerNames(triggerNames);
			std::vector<std::string> httpTriggerNames;
			script->getHttpTriggerNames(httpTriggerNames);
			std::vector<std::string> parameterNames;
			script->getScriptVariableNames(parameterNames);

			const std::string path= definition->getScriptPath().generic_string();
			outLines.push_back(
				std::to_string(script->getComponentId()) + " " + (path.empty() ? "-" : path) + " "
				+ (script->isScriptLoaded() ? "loaded" : "not_loaded")
				+ (triggerNames.empty() ? "" : " " + StringUtils::joinString(triggerNames, ','))
				+ (httpTriggerNames.empty() ? "" : " http=" + StringUtils::joinString(httpTriggerNames, ','))
				+ (parameterNames.empty() ? "" : " params=" + StringUtils::joinString(parameterNames, ',')));
		}

		return true;
	}
	else if (verb == "reload")
	{
		scriptSystem->reloadAllScripts();
		return true;
	}
	else if (verb == "routes")
	{
		// One line per HTTP route in table order
		for (const ScriptHttpRoute& route : scriptSystem->getTypedDefinitionConst()->getHttpRoutes().getRoutes())
		{
			outLines.push_back(route.route + " " + std::to_string(route.scriptId) + " "
							   + (route.functionName.empty() ? "-" : route.functionName) + " "
							   + (scriptSystem->isHttpRouteResolved(route) ? "resolved" : "unresolved"));
		}
		return true;
	}
	else if (verb == "eval" || verb == "trigger")
	{
		if (args.size() < 2)
		{
			outError= "usage: script " + verb + " "
					  + (verb == "eval" ? "<lua-code>" : "<triggerName> [script=<scriptName>] [key=value ...]");
			return false;
		}

		CommonScriptContextPtr scriptContext= scriptSystem->getScriptContext();
		if (!scriptContext)
		{
			outError= "no project script state loaded";
			return false;
		}

		if (verb == "eval")
		{
			// Take the code as the raw untokenized tail of the command line,
			// so Lua quotes and spacing arrive verbatim
			const std::string code= AutomationProtocol::remainderAfterTokens(m_currentCommandLine, 2);

			std::string result;
			if (!scriptContext->evalString(code, result))
			{
				outError= result;
				return false;
			}

			if (!result.empty())
				outLines.push_back(result);
			return true;
		}
		else
		{
			// Trailing "key=value" tokens become the trigger's argument table, the
			// same table an HTTP route builds from its query string. A "script="
			// token names the one script component to fire instead; without it
			// every component that has the trigger fires.
			const std::string& triggerName= args[1];
			MikanScriptID targetScriptId= INVALID_MIKAN_ID;
			std::map<std::string, std::string> triggerArgs;
			for (size_t argIndex= 2; argIndex < args.size(); ++argIndex)
			{
				const std::string& token= args[argIndex];
				const size_t equalsPos= token.find('=');
				if (equalsPos == std::string::npos || equalsPos == 0)
				{
					outError= "expected key=value, got '" + token + "'";
					return false;
				}

				const std::string key= token.substr(0, equalsPos);
				const std::string value= token.substr(equalsPos + 1);
				if (key == "script")
				{
					ScriptComponentPtr script= scriptSystem->getTypedComponentByName(value);
					if (!script)
					{
						outError= "unknown script '" + value + "'";
						return false;
					}
					targetScriptId= script->getComponentId();
					continue;
				}

				triggerArgs[key]= value;
			}

			// Bracketed like the panel button, so a trigger's property writes
			// coalesce into one transaction
			TransactionHistory* transactionHistory= m_mainWindow->getTransactionHistory();
			if (transactionHistory != nullptr)
				transactionHistory->beginGesture("script:" + triggerName);
			const bool bSuccess= scriptContext->invokeScriptTrigger(triggerName, triggerArgs, targetScriptId);
			if (transactionHistory != nullptr)
				transactionHistory->endGesture();

			if (!bSuccess)
			{
				outError= "trigger '" + triggerName + "' failed or does not exist";
				return false;
			}

			return true;
		}
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleLogCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										std::string& outError)
{
	if (args.size() < 2 || args[0] != "tail")
	{
		outError= "usage: log tail <lineCount> [trace|debug|info|warning|error|fatal]";
		return false;
	}

	int lineCount= 0;
	if (!parseComponentId(args[1], lineCount) || lineCount <= 0)
	{
		outError= "invalid line count '" + args[1] + "'";
		return false;
	}

	int minLevel= 0;
	if (args.size() >= 3)
	{
		static const std::vector<std::string> k_levelNames= {"trace", "debug", "info", "warning", "error", "fatal"};
		const auto levelIter= std::find(k_levelNames.begin(), k_levelNames.end(), args[2]);
		if (levelIter == k_levelNames.end())
		{
			outError= "unknown log level '" + args[2] + "'";
			return false;
		}
		minLevel= (int)(levelIter - k_levelNames.begin());
	}

	AutomationLogBuffer::getTail(lineCount, minLevel, outLines);
	return true;
}

bool AutomationServer::handleFunctionCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
											 std::string& outError)
{
	if (args.empty())
	{
		outError= "usage: function list|invoke ...";
		return false;
	}

	const std::string& verb= args[0];
	ProjectManagerPtr projectManager= m_mainWindow->getProjectManager();

	if (verb == "list")
	{
		const std::string systemFilter= args.size() >= 2 ? args[1] : "";
		const std::string componentFilter= args.size() >= 3 ? args[2] : "";
		MikanFunctionDatabaseConstPtr functionDatabase= projectManager->getFunctionDatabaseConst();

		FunctionDatabaseEnumerator enumerator(functionDatabase, systemFilter, componentFilter, "");
		while (enumerator.isValid())
		{
			const MikanFunctionEntry* entry= functionDatabase->getFunctionByIndex(enumerator.getCurrentFunctionIndex());
			const std::string componentClassName= !entry->componentClassName.empty() ? entry->componentClassName : "-";

			outLines.push_back(entry->systemName + " " + componentClassName + " " + entry->descriptor->getFunctionName()
							   + " " + entry->descriptor->getDisplayName());

			enumerator.next();
		}

		return true;
	}
	else if (verb == "invoke")
	{
		if (args.size() < 4)
		{
			outError= "usage: function invoke <system> <componentId> <name>";
			return false;
		}

		MikanObjectSystemPtr objectSystem;
		MikanComponentPtr component;
		if (!resolveCommandTarget(m_mainWindow, args[1], args[2], objectSystem, component, outError))
			return false;

		const std::string& functionName= args[3];
		const bool bInvoked=
			component != nullptr ? component->invokeFunction(functionName) : objectSystem->invokeFunction(functionName);
		if (!bInvoked)
		{
			outError= "unknown function '" + functionName + "'";
			return false;
		}

		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}

bool AutomationServer::handleAssetsCommand(const std::vector<std::string>& args, std::vector<std::string>& outLines,
										   std::string& outError)
{
	ProjectAssetCatalog* catalog= m_mainWindow->getAssetCatalog();
	if (catalog == nullptr)
	{
		outError= "no asset catalog";
		return false;
	}

	if (args.empty())
	{
		outError= "usage: assets folders|list|refresh|import|refs|delete|select|selected|folder ...";
		return false;
	}

	const std::string& verb= args[0];

	if (verb == "folders")
	{
		for (const ProjectAssetFolderDesc& desc : ProjectAssetCatalog::getFolderDescs())
		{
			const std::string projectSubfolder=
				!desc.projectSubfolder.empty() ? desc.projectSubfolder.generic_string() : "-";
			const std::string bundledSubfolder=
				!desc.bundledSubfolder.empty() ? desc.bundledSubfolder.generic_string() : "-";

			outLines.push_back(desc.id + " project:" + projectSubfolder + " bundled:" + bundledSubfolder + " "
							   + (desc.bReadOnly ? "readonly" : "writable"));
		}

		return true;
	}
	else if (verb == "list")
	{
		if (args.size() >= 2)
		{
			for (const ProjectAssetEntry& entry : catalog->getEntries(args[1]))
			{
				outLines.push_back(entry.folderId + " " + entry.className + " " + entry.storedPath
								   + (entry.bReadOnly ? " readonly" : ""));
			}
		}
		else
		{
			for (const ProjectAssetFolderDesc& desc : ProjectAssetCatalog::getFolderDescs())
			{
				for (const ProjectAssetEntry& entry : catalog->getEntries(desc.id))
				{
					outLines.push_back(entry.folderId + " " + entry.className + " " + entry.storedPath
									   + (entry.bReadOnly ? " readonly" : ""));
				}
			}
		}

		return true;
	}
	else if (verb == "refresh")
	{
		catalog->refresh();
		outLines.push_back("refreshed");
		return true;
	}
	else if (verb == "import")
	{
		if (args.size() < 3)
		{
			outError= "usage: assets import <folderId> <sourcePath>";
			return false;
		}

		std::string sourceText= args[2];
		for (size_t i= 3; i < args.size(); ++i)
		{
			sourceText+= " " + args[i];
		}

		std::string storedPath;
		if (!catalog->importAsset(args[1], std::filesystem::path(sourceText), storedPath, outError))
		{
			return false;
		}

		outLines.push_back(storedPath);
		return true;
	}
	else if (verb == "refs")
	{
		if (args.size() < 2)
		{
			outError= "usage: assets refs <storedPath>";
			return false;
		}

		std::string storedPath= args[1];
		for (size_t i= 2; i < args.size(); ++i)
		{
			storedPath+= " " + args[i];
		}

		for (const ProjectAssetReferrer& referrer : catalog->findReferences(storedPath))
		{
			outLines.push_back(std::string(referrerKindName(referrer.kind)) + " " + referrer.name + " "
							   + referrer.detail);
		}

		return true;
	}
	else if (verb == "delete")
	{
		if (args.size() < 3)
		{
			outError= "usage: assets delete <folderId> <storedPath>";
			return false;
		}

		const ProjectAssetEntry* entry= catalog->findEntry(args[1], args[2]);
		if (entry == nullptr)
		{
			outError= "asset not found";
			return false;
		}

		const std::vector<ProjectAssetReferrer> referrers= catalog->findReferences(entry->storedPath);
		if (!referrers.empty())
		{
			std::string joined;
			for (const ProjectAssetReferrer& referrer : referrers)
			{
				if (!joined.empty())
					joined+= ", ";
				joined+= std::string(referrerKindName(referrer.kind)) + " " + referrer.name;
			}

			outError= "referenced by: " + joined;
			return false;
		}

		if (!catalog->deleteAsset(*entry, outError))
		{
			return false;
		}

		outLines.push_back("deleted");
		return true;
	}
	else if (verb == "select")
	{
		if (args.size() < 3)
		{
			outError= "usage: assets select <folderId> <storedPath>";
			return false;
		}

		AppStage* currentAppStage= m_mainWindow->getCurrentAppStage();
		std::vector<std::string> results;
		if (currentAppStage == nullptr
			|| !currentAppStage->handleRemoteControlCommand("select_asset", {args[1], args[2]}, results))
		{
			outError= "asset not found";
			return false;
		}

		outLines.push_back("selected");
		return true;
	}
	else if (verb == "open")
	{
		if (args.size() < 3)
		{
			outError= "usage: assets open <folderId> <storedPath>";
			return false;
		}

		// The same open a double click on the entry performs
		const ProjectAssetEntry* entry= catalog->findEntry(args[1], args[2]);
		if (entry == nullptr)
		{
			outError= "asset not found";
			return false;
		}

		AssetReferencePtr assetRef= catalog->getAssetReference(*entry);
		if (!assetRef || !assetRef->editorCanOpen())
		{
			outError= "asset has no editor";
			return false;
		}

		assetRef->editorOpen();
		outLines.push_back("opened");
		return true;
	}
	else if (verb == "selected")
	{
		AppStage* currentAppStage= m_mainWindow->getCurrentAppStage();
		std::vector<std::string> results;
		if (currentAppStage == nullptr || !currentAppStage->handleRemoteControlCommand("selected_asset", {}, results))
		{
			outError= "no current project stage";
			return false;
		}

		if (results.size() >= 2)
			outLines.push_back(results[0] + " " + results[1]);
		else
			outLines.push_back("none");

		return true;
	}
	else if (verb == "folder")
	{
		AppStage* currentAppStage= m_mainWindow->getCurrentAppStage();
		std::vector<std::string> results;
		if (currentAppStage == nullptr
			|| !currentAppStage->handleRemoteControlCommand("current_asset_folder", {}, results) || results.empty())
		{
			outError= "no current project stage";
			return false;
		}

		outLines.push_back(results[0]);
		return true;
	}

	outError= "unknown verb '" + verb + "'";
	return false;
}
