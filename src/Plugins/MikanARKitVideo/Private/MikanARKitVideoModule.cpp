#include "MikanARKitVideoModule.h"
#include "MikanARKitVideoDeviceManager.h"
#include "IARKitVideoDeviceModule.h"
#include "Cuda/CudaDriverVersion.h"

#include <gst/gst.h>
#include <gst/gstparse.h>

#include "Logger.h"

class MikanARKitVideoModule : public IARKitVideoDeviceModule
{
public:
	MikanARKitVideoModule()
		: m_bIsInitialized(false)
	{
	}

	virtual ~MikanARKitVideoModule() { shutdown(); }

	bool startup() override
	{
		MIKAN_LOG_INFO("MikanARKitVideoModule") << "Initializing MikanARKitVideoModule";

		// Every video frame goes through CUDA-GL interop, and an older driver faults in
		// the CUDA calls rather than failing them, so refuse to start before any are made
		int driverVersion= 0;
		if (!isCudaDriverCompatible(driverVersion))
		{
			const int compiledVersion= getCompiledCudaVersion();
			if (driverVersion == 0)
			{
				MIKAN_LOG_ERROR("MikanARKitVideoModule")
					<< "No NVIDIA driver found. ARKit video needs an NVIDIA GPU with a driver supporting CUDA "
					<< compiledVersion / 1000 << ".0 or newer.";
			}
			else
			{
				MIKAN_LOG_ERROR("MikanARKitVideoModule")
					<< "The NVIDIA driver supports CUDA " << driverVersion / 1000 << "." << (driverVersion % 1000) / 10
					<< ", and ARKit video needs CUDA " << compiledVersion / 1000
					<< ".0 or newer. Update the NVIDIA driver to use ARKit video.";
			}
			return false;
		}

		// Initialize GStreamer (used for the RTP video receive path - see Track C2).
		// Intentionally don't pass in the command line arguments.
		GError* error= nullptr;
		if (gst_init_check(nullptr, nullptr, &error))
		{
			m_bIsInitialized= true;
		}
		else
		{
			MIKAN_LOG_ERROR("MikanARKitVideoModule")
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

	virtual IARKitVideoDeviceManagerPtr createARKitVideoDeviceManager() override
	{
		if (m_bIsInitialized)
		{
			return std::make_shared<MikanARKitVideoDeviceManager>();
		}

		return IARKitVideoDeviceManagerPtr();
	}

private:
	bool m_bIsInitialized;
};

// C-API
IARKitVideoDeviceModule* AllocatePluginModule() { return new MikanARKitVideoModule(); }

void FreePluginModule(IARKitVideoDeviceModule* module) { delete module; }
