#include "CudaDriverVersion.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cuda.h>

bool isCudaDriverCompatible(int& outDriverVersion)
{
	outDriverVersion= 0;

	// Already loaded by any process with an NVIDIA driver in use, otherwise loaded here.
	// The module stays loaded: the CUDA Driver API loads the same DLL on first use anyway.
	HMODULE driverModule= LoadLibraryW(L"nvcuda.dll");
	if (driverModule == nullptr)
		return false;

	typedef CUresult(CUDAAPI * DriverGetVersionFunc)(int*);
	DriverGetVersionFunc driverGetVersion=
		reinterpret_cast<DriverGetVersionFunc>(GetProcAddress(driverModule, "cuDriverGetVersion"));
	if (driverGetVersion == nullptr || driverGetVersion(&outDriverVersion) != CUDA_SUCCESS)
	{
		outDriverVersion= 0;
		return false;
	}

	// A driver runs code from any Toolkit of its own CUDA major version or older
	const int requiredDriverVersion= (CUDA_VERSION / 1000) * 1000;
	return outDriverVersion >= requiredDriverVersion;
}

int getCompiledCudaVersion() { return CUDA_VERSION; }
