#include "ServiceAdvertiser.h"
#include "App.h"
#include "AppSettingsConfig.h"
#include "ARKitVideoSourceSystem.h"
#include "Logger.h"
#include "MainWindow.h"
#include "ProjectManager.h"
#include "StringUtils.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <sstream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windns.h>

namespace
{
enum class eRegistrationPhase : int
{
	registering,
	registered,
	failed,
	withdrawn
};

// How long shutdown waits for the system to confirm the withdrawal
const auto k_shutdownTimeout= std::chrono::seconds(2);
// A retired registration whose completion never arrives is abandoned, not freed,
// since a late callback would still touch it
const auto k_retireTimeout= std::chrono::seconds(5);
} // namespace

/// One DNS-SD registration and the request it was made with. The system holds
/// the request and calls back with it as context on a thread pool thread, so
/// the object lives until its final completion has fired.
struct DnsSdRegistration
{
	std::string label;
	int port= 0;
	PDNS_SERVICE_INSTANCE instance= nullptr;
	// What the system actually registered, which carries a "(n)" suffix when the
	// name collided. Deregistering by the requested name would miss a renamed
	// registration and leave it answering after the process exits.
	PDNS_SERVICE_INSTANCE registeredInstance= nullptr;
	DNS_SERVICE_REGISTER_REQUEST registerRequest= {};
	DNS_SERVICE_REGISTER_REQUEST deregisterRequest= {};
	DNS_SERVICE_CANCEL cancelHandle= {};

	// Written by the completion callback, read on the main thread
	std::atomic<eRegistrationPhase> phase{eRegistrationPhase::registering};
	std::atomic<DWORD> status{ERROR_SUCCESS};

	// Main thread only
	bool bCancelRequested= false;
	bool bDeregisterRequested= false;
	bool bResultLogged= false;
	std::chrono::steady_clock::time_point retiredAt;

	~DnsSdRegistration()
	{
		if (registeredInstance != nullptr)
			DnsServiceFreeInstance(registeredInstance);
		if (instance != nullptr)
			DnsServiceFreeInstance(instance);
	}
};

static VOID WINAPI onDnsSdRequestComplete(DWORD status, PVOID context, PDNS_SERVICE_INSTANCE completedInstance)
{
	DnsSdRegistration* registration= static_cast<DnsSdRegistration*>(context);
	registration->status= status;

	// The first completion answers the registration (or its cancellation). Any
	// later one answers the deregistration. The main thread only writes the
	// phase once it reads registered, so this read cannot race it.
	if (registration->phase == eRegistrationPhase::registering && status == ERROR_SUCCESS)
	{
		// Kept for the deregistration, published before the phase that reads it
		registration->registeredInstance= completedInstance;
		registration->phase= eRegistrationPhase::registered;
		return;
	}

	if (completedInstance != nullptr)
		DnsServiceFreeInstance(completedInstance);

	registration->phase= registration->phase == eRegistrationPhase::registering ? eRegistrationPhase::failed
																				: eRegistrationPhase::withdrawn;
}

static std::string getLocalComputerName()
{
	WCHAR buffer[256];
	DWORD size= ARRAYSIZE(buffer);
	if (GetComputerNameExW(ComputerNameDnsHostname, buffer, &size))
	{
		return StringUtils::convertWStringToUTF8String(std::wstring(buffer, size));
	}

	return "MikanXR";
}

// ---- ServiceAdvertiser -----------------------------------------------------

const char* ServiceAdvertiser::k_serviceType= "_mikanxr._tcp";

ServiceAdvertiser::TxtRecord ServiceAdvertiser::buildTxtRecord(const std::vector<int>& arkitBasePorts)
{
	std::vector<int> sortedPorts= arkitBasePorts;
	std::sort(sortedPorts.begin(), sortedPorts.end());
	sortedPorts.erase(std::unique(sortedPorts.begin(), sortedPorts.end()), sortedPorts.end());

	std::ostringstream portList;
	for (size_t index= 0; index < sortedPorts.size(); ++index)
	{
		if (index > 0)
			portList << ",";
		portList << sortedPorts[index];
	}

	TxtRecord record;
	record.emplace_back("txtvers", std::to_string(k_txtVersion));
	record.emplace_back("arkit", portList.str());
	return record;
}

std::string ServiceAdvertiser::buildInstanceLabel(const std::string& computerName)
{
	return "MikanXR (" + computerName + ")";
}

ServiceAdvertiser::ServiceAdvertiser()= default;

ServiceAdvertiser::~ServiceAdvertiser() { shutdown(); }

void ServiceAdvertiser::startup(MainWindow* ownerWindow)
{
	m_ownerWindow= ownerWindow;
	m_computerName= getLocalComputerName();
	m_secondsUntilRefresh= 0.f;
}

void ServiceAdvertiser::poll(float deltaSeconds)
{
	if (m_ownerWindow == nullptr)
		return;

	logRegistrationResult();
	serviceRetiringRegistrations();

	m_secondsUntilRefresh-= deltaSeconds;
	if (m_secondsUntilRefresh <= 0.f)
	{
		m_secondsUntilRefresh= k_refreshIntervalSeconds;

		const AdvertisedState desiredState= gatherDesiredState();
		if (desiredState != m_advertisedState)
		{
			retireActiveRegistration();
			m_advertisedState= desiredState;
			m_bRegistrationPending= desiredState.bEnabled;

			if (!desiredState.bEnabled)
			{
				MIKAN_LOG_INFO("ServiceAdvertiser") << "Network advertisement off (remote connections not allowed)";
			}
		}
	}

	// The replacement waits for the withdrawal to complete. Registered alongside
	// the old one it would collide on the instance name, and Windows would rename
	// it "MikanXR (<computer>)(1)".
	if (m_bRegistrationPending && m_retiringRegistrations.empty())
	{
		m_bRegistrationPending= false;
		registerService(m_advertisedState);
	}
}

void ServiceAdvertiser::shutdown()
{
	if (m_ownerWindow == nullptr)
		return;

	retireActiveRegistration();
	m_advertisedState= AdvertisedState();
	m_bRegistrationPending= false;

	const auto deadline= std::chrono::steady_clock::now() + k_shutdownTimeout;
	serviceRetiringRegistrations();
	while (!m_retiringRegistrations.empty() && std::chrono::steady_clock::now() < deadline)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		serviceRetiringRegistrations();
	}

	if (!m_retiringRegistrations.empty())
	{
		MIKAN_LOG_WARNING("ServiceAdvertiser") << "Network advertisement withdrawal did not complete before exit";
		// Abandoned rather than freed, since a late callback would still touch them
		m_retiringRegistrations.clear();
	}

	m_ownerWindow= nullptr;
}

bool ServiceAdvertiser::isAdvertising() const
{
	return m_activeRegistration != nullptr && m_activeRegistration->phase == eRegistrationPhase::registered;
}

ServiceAdvertiser::AdvertisedState ServiceAdvertiser::gatherDesiredState() const
{
	AdvertisedState state;

	AppSettingsConfigPtr appSettings= m_ownerWindow->getOwnerApp()->getAppSettings();
	state.bEnabled= appSettings->getHttpServerAllowRemote();
	if (!state.bEnabled)
		return state;

	state.httpPort= appSettings->getHttpServerPort();

	ProjectManagerPtr projectManager= m_ownerWindow->getProjectManager();
	ARKitVideoSourceSystemPtr arkitSystem=
		projectManager ? projectManager->getSystemOfType<ARKitVideoSourceSystem>() : nullptr;
	if (arkitSystem)
	{
		for (const auto& entry : arkitSystem->getComponentMap())
		{
			ARKitVideoSourceComponentPtr source= entry.second.lock();
			if (source)
				state.arkitBasePorts.push_back(source->getARKitVideoSourceDefinition()->getBasePort());
		}
	}
	std::sort(state.arkitBasePorts.begin(), state.arkitBasePorts.end());

	return state;
}

void ServiceAdvertiser::registerService(const AdvertisedState& state)
{
	DnsSdRegistration* registration= new DnsSdRegistration();
	registration->label= buildInstanceLabel(m_computerName);
	registration->port= state.httpPort;

	const std::wstring instanceName=
		StringUtils::convertUTF8StringToWString(registration->label + "." + k_serviceType + ".local");
	const std::wstring hostName= StringUtils::convertUTF8StringToWString(m_computerName + ".local");

	std::vector<std::wstring> txtKeys;
	std::vector<std::wstring> txtValues;
	for (const auto& entry : buildTxtRecord(state.arkitBasePorts))
	{
		txtKeys.push_back(StringUtils::convertUTF8StringToWString(entry.first));
		txtValues.push_back(StringUtils::convertUTF8StringToWString(entry.second));
	}
	std::vector<PCWSTR> txtKeyPtrs;
	std::vector<PCWSTR> txtValuePtrs;
	for (size_t index= 0; index < txtKeys.size(); ++index)
	{
		txtKeyPtrs.push_back(txtKeys[index].c_str());
		txtValuePtrs.push_back(txtValues[index].c_str());
	}

	// No addresses given: the system answers for this host's own interfaces
	registration->instance=
		DnsServiceConstructInstance(instanceName.c_str(), hostName.c_str(), nullptr, nullptr, (WORD)state.httpPort, 0,
									0, (DWORD)txtKeyPtrs.size(), txtKeyPtrs.data(), txtValuePtrs.data());
	if (registration->instance == nullptr)
	{
		MIKAN_LOG_WARNING("ServiceAdvertiser") << "Failed to build the network advertisement";
		delete registration;
		return;
	}

	registration->registerRequest.Version= DNS_QUERY_REQUEST_VERSION1;
	registration->registerRequest.InterfaceIndex= 0;
	registration->registerRequest.pServiceInstance= registration->instance;
	registration->registerRequest.pRegisterCompletionCallback= onDnsSdRequestComplete;
	registration->registerRequest.pQueryContext= registration;
	registration->registerRequest.hCredentials= nullptr;
	registration->registerRequest.unicastEnabled= FALSE;

	const DWORD result= DnsServiceRegister(&registration->registerRequest, &registration->cancelHandle);
	if (result != DNS_REQUEST_PENDING)
	{
		MIKAN_LOG_WARNING("ServiceAdvertiser") << "Failed to start the network advertisement (error " << result << ")";
		delete registration;
		return;
	}

	m_activeRegistration= registration;
}

void ServiceAdvertiser::retireActiveRegistration()
{
	if (m_activeRegistration == nullptr)
		return;

	m_activeRegistration->retiredAt= std::chrono::steady_clock::now();
	m_retiringRegistrations.push_back(m_activeRegistration);
	m_activeRegistration= nullptr;
}

void ServiceAdvertiser::serviceRetiringRegistrations()
{
	const auto now= std::chrono::steady_clock::now();

	for (auto it= m_retiringRegistrations.begin(); it != m_retiringRegistrations.end();)
	{
		DnsSdRegistration* registration= *it;
		const eRegistrationPhase phase= registration->phase;

		if (phase == eRegistrationPhase::registering && !registration->bCancelRequested)
		{
			// Still probing: the cancellation completes through the same callback
			DnsServiceRegisterCancel(&registration->cancelHandle);
			registration->bCancelRequested= true;
		}
		else if (phase == eRegistrationPhase::registered && !registration->bDeregisterRequested)
		{
			// Sends the goodbye so browsers drop the service at once
			registration->deregisterRequest= registration->registerRequest;
			if (registration->registeredInstance != nullptr)
				registration->deregisterRequest.pServiceInstance= registration->registeredInstance;
			registration->bDeregisterRequested= true;
			if (DnsServiceDeRegister(&registration->deregisterRequest, nullptr) != DNS_REQUEST_PENDING)
			{
				registration->phase= eRegistrationPhase::withdrawn;
			}
		}

		const eRegistrationPhase settledPhase= registration->phase;
		if (settledPhase == eRegistrationPhase::failed || settledPhase == eRegistrationPhase::withdrawn)
		{
			delete registration;
			it= m_retiringRegistrations.erase(it);
		}
		else if (now - registration->retiredAt > k_retireTimeout)
		{
			MIKAN_LOG_WARNING("ServiceAdvertiser") << "Abandoning a network advertisement that never completed";
			it= m_retiringRegistrations.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void ServiceAdvertiser::logRegistrationResult()
{
	DnsSdRegistration* registration= m_activeRegistration;
	if (registration == nullptr || registration->bResultLogged)
		return;

	const eRegistrationPhase phase= registration->phase;
	if (phase == eRegistrationPhase::registered)
	{
		std::ostringstream txtText;
		for (const auto& entry : buildTxtRecord(m_advertisedState.arkitBasePorts))
		{
			txtText << " " << entry.first << "=" << entry.second;
		}
		std::string registeredName= registration->label;
		if (registration->registeredInstance != nullptr && registration->registeredInstance->pszInstanceName != nullptr)
		{
			registeredName= StringUtils::convertWStringToUTF8String(registration->registeredInstance->pszInstanceName);
		}
		MIKAN_LOG_INFO("ServiceAdvertiser")
			<< "Advertising \"" << registeredName << "\" on port " << registration->port << "," << txtText.str();
		registration->bResultLogged= true;
	}
	else if (phase == eRegistrationPhase::failed)
	{
		MIKAN_LOG_WARNING("ServiceAdvertiser")
			<< "Network advertisement failed (error " << registration->status.load() << ")";
		registration->bResultLogged= true;
	}
}
