//-- includes -----
#include <stdio.h>
#include <stdlib.h>
#include "unit_test.h"

// Same compile-time guard as before - this file needs CUDA (only available
// when MIKAN_WITH_ARKIT_VIDEO is ON, which is GStreamer plus an installed CUDA
// Toolkit - see UnitTests/CMakeLists.txt, which defines
// MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE for exactly this).
#if defined(MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE)

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <assert.h>
#include <cmath>
#include <cstdint>
#include <vector>

#include <GL/glew.h>

#include <cuda.h>

#include "CudaGLInterop.h"
#include "IMkTexture.h"

// Full end-to-end coverage for ticket D4/E3/"Phase 6"'s CudaGLColorTexture - the
// one piece of this CUDA-GL interop pipeline that genuinely needs a live OpenGL
// context, which this otherwise-headless console test executable doesn't
// normally have. This file creates a minimal, hidden, off-screen WGL context
// purely for this one test's own use (no SDL/MikanWindow dependency -
// self-contained Win32+WGL, in the same spirit as MikanARKitVideoDevice.cpp's
// own direct <windows.h> usage). Feeds constant-value Y/UV planes through
// CudaGLColorTexture::copyFromDevice() and confirms both planes are visible via
// a glGetTexImage readback - the same round-trip
// MikanARKitVideoDevice::updateColorTexture() performs live per frame. As of
// "Phase 6" there's no CUDA kernel/color-conversion math to verify here anymore
// (that moved to a GLSL shader on the Editor side) - this test only proves the
// raw NV12 bytes survive the device-to-array copy unmodified. Still gated at
// runtime (skips gracefully, doesn't fail) if window/GL/CUDA-GL-interop context
// creation fails, since a headless CI runner with no display adapter is a real
// possibility this repo has no other precedent for handling.
namespace
{
bool cudaGlInteropTestCreateHiddenGLContext(HWND& outWindow, HDC& outDC, HGLRC& outGLRC)
{
	outWindow= nullptr;
	outDC= nullptr;
	outGLRC= nullptr;

	static const wchar_t* kClassName= L"MikanARKitVideoCudaGLInteropTestWindow";
	static bool classRegistered= false;
	if (!classRegistered)
	{
		WNDCLASSW windowClass= {};
		windowClass.lpfnWndProc= DefWindowProcW;
		windowClass.hInstance= GetModuleHandleW(nullptr);
		windowClass.lpszClassName= kClassName;
		if (RegisterClassW(&windowClass) == 0)
			return false;
		classRegistered= true;
	}

	HWND window= CreateWindowExW(0, kClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
								 GetModuleHandleW(nullptr), nullptr);
	if (window == nullptr)
		return false;

	HDC dc= GetDC(window);
	if (dc == nullptr)
	{
		DestroyWindow(window);
		return false;
	}

	PIXELFORMATDESCRIPTOR pfd= {};
	pfd.nSize= sizeof(pfd);
	pfd.nVersion= 1;
	pfd.dwFlags= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType= PFD_TYPE_RGBA;
	pfd.cColorBits= 32;
	pfd.cDepthBits= 24;

	const int pixelFormat= ChoosePixelFormat(dc, &pfd);
	if (pixelFormat == 0 || !SetPixelFormat(dc, pixelFormat, &pfd))
	{
		ReleaseDC(window, dc);
		DestroyWindow(window);
		return false;
	}

	HGLRC glrc= wglCreateContext(dc);
	if (glrc == nullptr || !wglMakeCurrent(dc, glrc))
	{
		if (glrc != nullptr)
			wglDeleteContext(glrc);
		ReleaseDC(window, dc);
		DestroyWindow(window);
		return false;
	}

	static bool glewInitialized= false;
	if (!glewInitialized)
	{
		glewExperimental= GL_TRUE;
		if (glewInit() != GLEW_OK)
		{
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(glrc);
			ReleaseDC(window, dc);
			DestroyWindow(window);
			return false;
		}
		glewInitialized= true;
	}

	outWindow= window;
	outDC= dc;
	outGLRC= glrc;
	return true;
}

void cudaGlInteropTestDestroyHiddenGLContext(HWND window, HDC dc, HGLRC glrc)
{
	wglMakeCurrent(nullptr, nullptr);
	if (glrc != nullptr)
		wglDeleteContext(glrc);
	if (window != nullptr && dc != nullptr)
		ReleaseDC(window, dc);
	if (window != nullptr)
		DestroyWindow(window);
}

bool cudaGlInteropTestCudaAvailable()
{
	if (cuInit(0) != CUDA_SUCCESS)
		return false;

	int deviceCount= 0;
	return cuDeviceGetCount(&deviceCount) == CUDA_SUCCESS && deviceCount > 0;
}
} // namespace

//-- private functions -----
static bool arkit_cuda_gl_interop_test_writeback_visible_via_gl_readback()
{
	UNIT_TEST_BEGIN(
		"CudaGLColorTexture: a copyFromDevice() plane write is visible when the texture is read back through GL")

	HWND window= nullptr;
	HDC dc= nullptr;
	HGLRC glrc= nullptr;
	if (!cudaGlInteropTestCreateHiddenGLContext(window, dc, glrc))
	{
		// Off-screen GL context creation isn't guaranteed to work in every
		// environment this suite runs in (e.g. no display adapter) - not a
		// failure of this test, nothing further to verify.
		UNIT_TEST_COMPLETE()
	}

	if (!cudaGlInteropTestCudaAvailable())
	{
		cudaGlInteropTestDestroyHiddenGLContext(window, dc, glrc);
		UNIT_TEST_COMPLETE()
	}

	// The same device choice the plugin makes. A GL context on a GPU with no CUDA device
	// behind it (an integrated GPU on a hybrid laptop) cannot share textures with CUDA at
	// all, which is an environment limit rather than a failure of this test.
	CUdevice device;
	if (!findCudaDeviceForCurrentGLContext(device))
	{
		fprintf(stdout, "      skipped - OpenGL is running on a GPU with no CUDA device\n");
		cudaGlInteropTestDestroyHiddenGLContext(window, dc, glrc);
		UNIT_TEST_COMPLETE()
	}

	CUcontext context= nullptr;
	success= (cuCtxCreate(&context, nullptr, 0, device) == CUDA_SUCCESS);
	assert(success);

	CudaGLColorTexture colorTexture;
	success= success && colorTexture.init(64, 48);
	assert(success);

	const int lumaW= colorTexture.getWidth();
	const int lumaH= colorTexture.getHeight();
	const int chromaW= lumaW / 2;
	const int chromaH= lumaH / 2;

	// Arbitrary constant Y/UV values - this test only checks the bytes survive
	// the device-to-array copy unmodified, not any color-conversion math (that
	// now lives in a GLSL shader on the Editor side, not in this plugin).
	const uint8_t yValue= 200;
	const uint8_t uvValue= 40;

	const int yStrideBytes= lumaW;
	const int uvStrideBytes= lumaW; // interleaved UV, half-height, same row stride as Y (bytes)
	std::vector<uint8_t> yPlane(static_cast<size_t>(yStrideBytes) * lumaH, yValue);
	std::vector<uint8_t> uvPlane(static_cast<size_t>(uvStrideBytes) * chromaH, uvValue);

	CUdeviceptr d_yPlane= 0, d_uvPlane= 0;
	success= success && (cuMemAlloc(&d_yPlane, yPlane.size()) == CUDA_SUCCESS);
	success= success && (cuMemAlloc(&d_uvPlane, uvPlane.size()) == CUDA_SUCCESS);
	assert(success);

	success= success && (cuMemcpyHtoD(d_yPlane, yPlane.data(), yPlane.size()) == CUDA_SUCCESS);
	success= success && (cuMemcpyHtoD(d_uvPlane, uvPlane.data(), uvPlane.size()) == CUDA_SUCCESS);
	assert(success);

	success= success && colorTexture.copyFromDevice(d_yPlane, yStrideBytes, d_uvPlane, uvStrideBytes);
	assert(success);

	std::vector<uint8_t> lumaReadback(static_cast<size_t>(lumaW) * lumaH, 0);
	if (success)
	{
		glBindTexture(GL_TEXTURE_2D, colorTexture.getLumaTexture()->getGlTextureId());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, lumaReadback.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		success= success && (glGetError() == GL_NO_ERROR);
		assert(success);
	}

	std::vector<uint8_t> chromaReadback(static_cast<size_t>(chromaW) * chromaH * 2, 0);
	if (success)
	{
		glBindTexture(GL_TEXTURE_2D, colorTexture.getChromaTexture()->getGlTextureId());
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_UNSIGNED_BYTE, chromaReadback.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		success= success && (glGetError() == GL_NO_ERROR);
		assert(success);
	}

	bool lumaMatches= true;
	for (size_t i= 0; i < lumaReadback.size() && lumaMatches; ++i)
		lumaMatches= (lumaReadback[i] == yValue);
	success= success && lumaMatches;
	assert(success);

	bool chromaMatches= true;
	for (size_t i= 0; i < chromaReadback.size() && chromaMatches; ++i)
		chromaMatches= (chromaReadback[i] == uvValue);
	success= success && chromaMatches;
	assert(success);

	if (d_yPlane != 0)
		cuMemFree(d_yPlane);
	if (d_uvPlane != 0)
		cuMemFree(d_uvPlane);
	colorTexture.shutdown();
	if (context != nullptr)
		cuCtxDestroy(context);
	cudaGlInteropTestDestroyHiddenGLContext(window, dc, glrc);

	UNIT_TEST_COMPLETE()
}

//-- public interface -----
bool run_arkit_cuda_gl_interop_unit_tests()
{
	UNIT_TEST_MODULE_BEGIN("arkit_cuda_gl_interop")
	UNIT_TEST_MODULE_CALL_TEST(arkit_cuda_gl_interop_test_writeback_visible_via_gl_readback);
	UNIT_TEST_MODULE_END()
}

#else // !defined(MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE)

bool run_arkit_cuda_gl_interop_unit_tests()
{
	fprintf(stdout, "[arkit_cuda_gl_interop]\n");
	fprintf(stdout, "  skipped - built without the CUDA Toolkit\n");
	return true;
}

#endif // defined(MIKAN_ARKIT_CUDA_GL_INTEROP_AVAILABLE)
