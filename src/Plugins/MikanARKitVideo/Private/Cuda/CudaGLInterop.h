#pragma once

#include <cuda.h>

#include "MkRendererFwd.h"

// Finds the CUDA device the current GL context runs on, since CUDA can only share textures
// with a GL context on its own device. Returns false when there is none, which on a
// hybrid-GPU laptop means GL is on the integrated GPU. Needs a current GL context and
// cuInit() already called.
bool findCudaDeviceForCurrentGLContext(CUdevice& outDevice);

// Zero-copy CUDA-GL interop for the decoded ARKit NV12 video frame (ticket
// "Phase 6"): owns two IMkTextures - luma (MK_RED, 8-bit) at full resolution and
// chroma (MK_RG, 8-bit) at half resolution, matching NV12's 4:2:0 plane layout -
// and registers both backing GL textures with CUDA, so the decoded Y/UV device
// memory can be copied directly into them with no host round-trip and no CUDA
// kernel (the actual NV12->RGBA color conversion now happens as a GLSL shader
// pass on the Editor side - see ARKitVideoSourceComponent - since a plain
// device-to-array copy is all this interop layer needs to do).
//
// THREADING: every method on this class must be called on the thread that owns
// the current GL context. Mikan is single-threaded for GL - there is no separate
// render thread; App::tickWindows() runs each window's update()/render() on the
// main thread, which is the only thread with a GL context ever current (see
// VideoFrameDistortionView::processVideoFrame() for the established pattern of
// marshaling GL work onto that thread from a producer/decode thread - this class
// does not do that marshaling itself, its caller must). This applies to
// init()/resize()/shutdown() (GL texture creation) and copyFromDevice() (CUDA-GL
// registration/mapping) alike - calling any of these off that thread is a
// documented source of silent driver-level failures. A CUDA context must also be
// current on that thread for the CUDA-side calls.
class CudaGLColorTexture
{
public:
	CudaGLColorTexture();
	~CudaGLColorTexture();

	CudaGLColorTexture(const CudaGLColorTexture&)= delete;
	CudaGLColorTexture& operator=(const CudaGLColorTexture&)= delete;

	// width/height are the luma (full-resolution) plane's dimensions - the chroma
	// plane is always half that in each dimension, per NV12's 4:2:0 layout.
	bool init(int width, int height);
	void shutdown();
	bool isInitialized() const;

	int getWidth() const;
	int getHeight() const;
	IMkTexturePtr getLumaTexture() const;
	IMkTexturePtr getChromaTexture() const;

	bool resize(int width, int height);

	// Maps both planes' registered GL textures as CUDA arrays, copies the given
	// device-memory NV12 Y/UV planes into them via cuMemcpy2D (honoring the
	// caller's own stride, since decoder output can be padded), then unmaps.
	// Synchronous - blocks until both copies complete before returning, so the
	// textures are safe to read via GL immediately after a successful call.
	bool copyFromDevice(CUdeviceptr yPlane, int yStrideBytes, CUdeviceptr uvPlane, int uvStrideBytes);

private:
	bool registerWithCuda();
	void unregisterFromCuda();

	IMkTexturePtr m_lumaTexture;
	IMkTexturePtr m_chromaTexture;
	CUgraphicsResource m_lumaGraphicsResource= nullptr;
	CUgraphicsResource m_chromaGraphicsResource= nullptr;
	int m_width= 0;
	int m_height= 0;
};
