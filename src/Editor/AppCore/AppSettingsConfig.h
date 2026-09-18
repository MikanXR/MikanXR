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

	// A multiplier the user applies on top of the display's own content scale, for
	// when the scale Windows reports is not the size they want to work at.
	static const std::string k_uiScalePropertyId;
	static constexpr float k_minUiScale= 0.5f;
	static constexpr float k_maxUiScale= 3.0f;
	inline float getUiScale() const { return m_uiScale; }
	void setUiScale(float scale);

	static const std::string k_scriptEditorCommandPropertyId;
	inline const std::string& getScriptEditorCommand() const { return m_scriptEditorCommand; }
	void setScriptEditorCommand(const std::string& command);

	// The current default, carrying {project}/{file}/{line} placeholders, and
	// the old default that predates them (a stored copy of the legacy default
	// upgrades to the new one on load).
	static const std::string k_defaultScriptEditorCommand;
	static const std::string k_legacyScriptEditorCommand;
	// True when the command carries {file}, the one placeholder that makes it
	// self-contained rather than needing paths appended after it.
	static bool scriptEditorCommandHasPlaceholders(const std::string& command);

	static const std::string k_httpServerPortPropertyId;
	inline int getHttpServerPort() const { return m_httpServerPort; }
	void setHttpServerPort(int port);

	static const std::string k_httpServerAllowRemotePropertyId;
	inline bool getHttpServerAllowRemote() const { return m_bHttpServerAllowRemote; }
	void setHttpServerAllowRemote(bool bAllow);

	static const std::string k_automationServerEnabledPropertyId;
	inline bool getAutomationServerEnabled() const { return m_bAutomationServerEnabled; }
	void setAutomationServerEnabled(bool bEnabled);

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
	float m_uiScale= 1.f;
	std::string m_scriptEditorCommand= k_defaultScriptEditorCommand;
	int m_httpServerPort= 8090; // mirrors HTTP_SERVER_PORT in HttpInterprocessMessageServer.h
	// The HTTP server binds loopback only until this is on, which is what a phone
	// uploading captures over the LAN needs. Off by default, since the script
	// trigger routes and the upload endpoint are then reachable by anything on the network.
	bool m_bHttpServerAllowRemote= false;
	// The automation channel drives and scripts the editor, so any local process
	// reaching it owns the session. It stays off until asked for.
	bool m_bAutomationServerEnabled= false;
	int m_automationServerPort= 21120; // loopback automation command channel
	bool m_bSpoutLogEnabled= false;    // relays Spout's own logs into the editor log
	// Developer switch: the bundled resources are read-only assets behind every
	// project until this is on, when the editors save into them in place
	bool m_bEditBundledResources= false;
	// The ARKit debug channel binds every interface rather than loopback only,
	// so it stays off until asked for
	bool m_bARKitDebugChannelEnabled= false;
	int m_arkitDebugChannelPort= 21121;
	// Only the prefixes that differ from the built-in defaults, keyed by
	// component class name. An entry holding "" is a deliberate class-name
	// fallback, which is not the same as no entry.
	std::map<std::string, std::string> m_componentNamePrefixes;
};