#pragma once

// -- includes -----
#include "CommonConfig.h"

#include <filesystem>
#include <map>
#include <string>

// -- definitions -----
class AppSettingsConfig : public CommonConfig
{
public:
	AppSettingsConfig(const std::string& fnamebase= "AppSettingsConfig");

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;

	static const std::string k_lastProjectPathPropertyId;
	inline bool hasLastProjectPath() const { return !m_lastProjectPath.empty(); }
	inline const std::filesystem::path& getLastProjectPath() const { return m_lastProjectPath; }
	void setLastProjectPath(const std::filesystem::path& projectPath);

	static const std::string k_appLanguagePropertyId;
	inline const std::string& getAppLanguage() const { return m_appLanguage; }
	void setAppLanguage(const std::string& appLanguage);

	static const std::string k_scriptEditorCommandPropertyId;
	inline const std::string& getScriptEditorCommand() const { return m_scriptEditorCommand; }
	void setScriptEditorCommand(const std::string& command);

	static const std::string k_httpServerPortPropertyId;
	inline int getHttpServerPort() const { return m_httpServerPort; }
	void setHttpServerPort(int port);

	static const std::string k_automationServerPortPropertyId;
	inline int getAutomationServerPort() const { return m_automationServerPort; }
	void setAutomationServerPort(int port);

	static const std::string k_spoutLogEnabledPropertyId;
	inline bool getSpoutLogEnabled() const { return m_bSpoutLogEnabled; }
	void setSpoutLogEnabled(bool bEnabled);

	static const std::string k_editBundledResourcesPropertyId;
	inline bool getEditBundledResources() const { return m_bEditBundledResources; }
	void setEditBundledResources(bool bEditable);

	static const std::string k_arkitDebugChannelEnabledPropertyId;
	inline bool getARKitDebugChannelEnabled() const { return m_bARKitDebugChannelEnabled; }
	void setARKitDebugChannelEnabled(bool bEnabled);

	static const std::string k_arkitDebugChannelPortPropertyId;
	inline int getARKitDebugChannelPort() const { return m_arkitDebugChannelPort; }
	void setARKitDebugChannelPort(int port);

	// The prefix a new component of the class is named with ("CAM" gives
	// "CAM_1096"). An empty prefix names it by its class instead.
	static const std::string k_componentNamePrefixesPropertyId;
	std::string getComponentNamePrefix(const std::string& componentClassName) const;
	void setComponentNamePrefix(const std::string& componentClassName, const std::string& prefix);
	void resetComponentNamePrefixes();

protected:
	std::filesystem::path m_lastProjectPath;
	std::string m_appLanguage;
	std::string m_scriptEditorCommand= "code --reuse-window";
	int m_httpServerPort= 8090;        // mirrors HTTP_SERVER_PORT in HttpInterprocessMessageServer.h
	int m_automationServerPort= 21120; // loopback automation command channel
	bool m_bSpoutLogEnabled= false;    // relays Spout's own logs into the editor log
	// Developer switch: the bundled resources are read-only assets behind every
	// project until this is on, when the editors save into them in place
	bool m_bEditBundledResources= false;
	// The ARKit debug channel binds every interface, unlike the loopback-only
	// automation channel, so it stays off until asked for
	bool m_bARKitDebugChannelEnabled= false;
	int m_arkitDebugChannelPort= 21121;
	// Only the prefixes that differ from the built-in defaults, keyed by
	// component class name. An entry holding "" is a deliberate class-name
	// fallback, which is not the same as no entry.
	std::map<std::string, std::string> m_componentNamePrefixes;
};