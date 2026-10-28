#pragma once

// Code built against a CUDA 13 Toolkit resolves its Driver API entry points from the
// installed driver at runtime, and a driver from an older major release can fault inside
// nvcuda.dll instead of returning an error. This compares the driver's version
// (CudaDriverUtils::getInstalledCudaDriverVersion, read without going through that
// resolution) with the Toolkit, so callers can refuse to touch CUDA on an older driver.
// outDriverVersion receives the driver's CUDA version (e.g. 12080), or 0 when no NVIDIA
// driver is installed. Returns true when it is the same CUDA major version as the Toolkit
// this code was compiled against, or newer.
bool isCudaDriverCompatible(int& outDriverVersion);

// The CUDA version (e.g. 13010) of the Toolkit headers this code was compiled against
int getCompiledCudaVersion();
