#include "CudaDriverVersion.h"
#include "CudaDriverUtils.h"

#include <cuda.h>

bool isCudaDriverCompatible(int& outDriverVersion)
{
	outDriverVersion= CudaDriverUtils::getInstalledCudaDriverVersion();
	if (outDriverVersion == 0)
		return false;

	// A driver runs code from any Toolkit of its own CUDA major version or older
	const int requiredDriverVersion= (CUDA_VERSION / 1000) * 1000;
	return outDriverVersion >= requiredDriverVersion;
}

int getCompiledCudaVersion() { return CUDA_VERSION; }
