#pragma once

#include "MikanUtilityExport.h"

//-- utility methods -----
namespace CudaDriverUtils
{
/// The CUDA version (e.g. 12080) the installed NVIDIA driver supports, read from
/// nvcuda.dll's cuDriverGetVersion export without initializing CUDA. Code built against
/// a newer CUDA major release can fault inside the driver rather than fail, so this is
/// the check made before any other CUDA call. 0 when no NVIDIA driver is installed.
MIKAN_UTILITY_FUNC(int) getInstalledCudaDriverVersion();
}; // namespace CudaDriverUtils
