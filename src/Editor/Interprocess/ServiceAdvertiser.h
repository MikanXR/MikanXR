#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

/// Advertises the editor on the LAN over DNS-SD (mDNS), so the MikanARStreamer
/// phone app can find the editor's address, its HTTP server port, and the base
/// port of each ARKit video source without the user typing them in.
///
/// The service is `_mikanxr._tcp` on the HTTP server's port, with the ARKit
/// base ports in the TXT record. It is advertised only while the HTTP server
/// accepts connections from other machines, since a loopback-only server is of
/// no use to a phone.
///
/// Uses the DNS-SD API built into Windows (windns.h, Windows 10 1903 and later),
/// which needs no Bonjour install. Registration completes on a system thread
/// pool callback, so each registration is a heap object that outlives the call
/// that started it until its completion has fired.
struct DnsSdRegistration;

class ServiceAdvertiser
{
public:
	static const char* k_serviceType;
	/// TXT record version, bumped when a key changes meaning
	static const int k_txtVersion= 1;
	/// How often the desired advertisement is rebuilt and compared
	static constexpr float k_refreshIntervalSeconds= 1.f;

	using TxtRecord= std::vector<std::pair<std::string, std::string>>;

	/// The TXT record: `txtvers`, then `arkit` holding the ARKit base ports sorted
	/// ascending and comma separated, empty when the project has no ARKit source,
	/// then `ws` holding the client API websocket port.
	static TxtRecord buildTxtRecord(const std::vector<int>& arkitBasePorts, int websocketPort);

	/// The DNS-SD instance name shown to browsers, e.g. `MikanXR (DESKTOP-1)`
	static std::string buildInstanceLabel(const std::string& computerName);

	ServiceAdvertiser();
	~ServiceAdvertiser();

	// Non-copyable
	ServiceAdvertiser(const ServiceAdvertiser&)= delete;
	ServiceAdvertiser& operator=(const ServiceAdvertiser&)= delete;

	void startup(class MainWindow* ownerWindow);

	/// Re-registers when the HTTP port, the remote access setting, or the set of
	/// ARKit base ports changed, and reaps registrations whose withdrawal has
	/// completed. Called once per frame on the main thread.
	void poll(float deltaSeconds);

	/// Withdraws the advertisement, waiting briefly for the system to confirm
	/// so no completion callback is left in flight at exit.
	void shutdown();

	bool isAdvertising() const;

private:
	struct AdvertisedState
	{
		bool bEnabled= false;
		int httpPort= 0;
		int websocketPort= 0;
		std::vector<int> arkitBasePorts;

		bool operator==(const AdvertisedState& other) const
		{
			return bEnabled == other.bEnabled && httpPort == other.httpPort && websocketPort == other.websocketPort
				   && arkitBasePorts == other.arkitBasePorts;
		}
		bool operator!=(const AdvertisedState& other) const { return !(*this == other); }
	};

	AdvertisedState gatherDesiredState() const;
	void registerService(const AdvertisedState& state);
	void retireActiveRegistration();
	void serviceRetiringRegistrations();
	void logRegistrationResult();

	class MainWindow* m_ownerWindow= nullptr;
	std::string m_computerName;
	AdvertisedState m_advertisedState;
	float m_secondsUntilRefresh= 0.f;
	bool m_bRegistrationPending= false;

	DnsSdRegistration* m_activeRegistration= nullptr;
	std::vector<DnsSdRegistration*> m_retiringRegistrations;
};
