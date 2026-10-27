#include "MikanGStreamerVideoModule.h"
#include "MikanGStreamerRtpVideoSender.h"
#include "MikanGStreamerVideoDeviceManager.h"
#include "INetworkVideoDeviceModule.h"

#include <gst/gst.h>
#include <gst/gstparse.h>

#include "Logger.h"

class MikanGStreamerVideoModule : public INetworkVideoDeviceModule
{
public:
	MikanGStreamerVideoModule()
		: m_bIsInitialized(false)
	{
	}

	virtual ~MikanGStreamerVideoModule() { shutdown(); }

	bool startup() override
	{
		MIKAN_LOG_INFO("MikanNetworkVideoModule") << "Initializing MikanNetworkVideoModule";

		// Initialize GStreamer
		// Intentionally don't pass in the command line arguments
		GError* error= nullptr;
		if (gst_init_check(nullptr, nullptr, &error))
		{
			m_bIsInitialized= true;
		}
		else
		{
			MIKAN_LOG_ERROR("MikanGStreamerModule")
				<< "Failed to init GStreamer: " << (error != nullptr ? error->message : "unknown error");
			g_clear_error(&error);
		}

		return m_bIsInitialized;
	}

	void shutdown() override
	{
		// Deliberately no gst_deinit() here. GStreamer's init state is process-global
		// and shared with every other loaded plugin that uses it, and the library
		// hard-errors if it is deinitialized a second time. Its resources are
		// reclaimed at process exit, so no plugin tears it down.
		m_bIsInitialized= false;
	}

	virtual INetworkVideoDeviceManagerPtr createNetworkVideoDeviceManager() override
	{
		if (m_bIsInitialized)
		{
			return std::make_shared<MikanGStreamerVideoDeviceManager>();
		}

		return INetworkVideoDeviceManagerPtr();
	}

	virtual IRtpVideoSenderPtr createRtpVideoSender() override
	{
		if (m_bIsInitialized)
		{
			return std::make_shared<MikanGStreamerRtpVideoSender>();
		}

		return IRtpVideoSenderPtr();
	}

private:
	bool m_bIsInitialized;
};

// C-API
INetworkVideoDeviceModule* AllocatePluginModule() { return new MikanGStreamerVideoModule(); }

void FreePluginModule(INetworkVideoDeviceModule* module) { delete module; }