// -- includes -----
#include "CudaDriverUtils.h"

#if defined WIN32 || defined _WIN32 || defined WINCE
#include <windows.h>
#endif

// -- public methods -----
namespace CudaDriverUtils
{
int getInstalledCudaDriverVersion()
{
#if defined WIN32 || defined _WIN32 || defined WINCE
	// Already loaded by any process with an NVIDIA driver in use, otherwise loaded here.
	// The module stays loaded: the CUDA Driver API loads the same DLL on first use anyway.
	HMODULE driverModule= LoadLibraryW(L"nvcuda.dll");
	if (driverModule == nullptr)
		return 0;

	// CUresult cuDriverGetVersion(int*), with CUDA_SUCCESS == 0
	typedef int(__stdcall * DriverGetVersionFunc)(int*);
	DriverGetVersionFunc driverGetVersion=
		reinterpret_cast<DriverGetVersionFunc>(GetProcAddress(driverModule, "cuDriverGetVersion"));

	int driverVersion= 0;
	if (driverGetVersion == nullptr || driverGetVersion(&driverVersion) != 0)
		return 0;

	return driverVersion;
#else
	return 0;
#endif
}
}; // namespace CudaDriverUtils
