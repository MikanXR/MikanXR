#include "CudaGLInterop.h"
#include "CudaErrorHandling.h"
#include "Logger.h"

// GLEW must be included before any header that pulls in a system GL header
// (cudaGL.h does, transitively) - see MikanARKitVideoDevice.cpp's equivalent note
// for the windows.h/GL.h ordering issue this avoids. GLEW itself is already
// initialized process-wide by the time any video-source update() tick runs (see
// GlGraphicsContext::startup()), so this translation unit doesn't call
// glewInit() - it only needs GLEW's type/macro definitions (GLuint, GL_TEXTURE_2D).
#include <GL/glew.h>

#include <cudaGL.h>

#include "IMkTexture.h"

// -- Device selection -----
bool findCudaDeviceForCurrentGLContext(CUdevice& outDevice)
{
	unsigned int deviceCount= 0;
	CUdevice device= 0;
	if (cuGLGetDevices(&deviceCount, &device, 1, CU_GL_DEVICE_LIST_ALL) != CUDA_SUCCESS || deviceCount == 0)
		return false;

	outDevice= device;
	return true;
}

// -- CudaGLColorTexture -----
CudaGLColorTexture::CudaGLColorTexture()= default;

CudaGLColorTexture::~CudaGLColorTexture() { shutdown(); }

bool CudaGLColorTexture::init(int width, int height)
{
	if (isInitialized())
	{
		if (width == m_width && height == m_height)
			return true; // already initialized at this size

		MIKAN_LOG_ERROR("CudaGLColorTexture::init")
			<< "Already initialized at a different size - call resize() instead";
		return false;
	}

	if (width <= 0 || height <= 0)
	{
		MIKAN_LOG_ERROR("CudaGLColorTexture::init") << "Invalid size " << width << "x" << height;
		return false;
	}

	// Luma: full-resolution, single-channel 8-bit. Chroma: half-resolution (both
	// dimensions) two-channel 8-bit, interleaved U/V - matches NV12's 4:2:0 layout.
	m_lumaTexture=
		CreateMkTexture(static_cast<uint16_t>(width), static_cast<uint16_t>(height), nullptr, MK_RED, MK_RED);
	if (m_lumaTexture == nullptr || !m_lumaTexture->createTexture())
	{
		MIKAN_LOG_ERROR("CudaGLColorTexture::init") << "Failed to create MK_RED luma texture";
		m_lumaTexture.reset();
		return false;
	}

	const int chromaWidth= width / 2;
	const int chromaHeight= height / 2;
	m_chromaTexture=
		CreateMkTexture(static_cast<uint16_t>(chromaWidth), static_cast<uint16_t>(chromaHeight), nullptr, MK_RG, MK_RG);
	if (m_chromaTexture == nullptr || !m_chromaTexture->createTexture())
	{
		MIKAN_LOG_ERROR("CudaGLColorTexture::init") << "Failed to create MK_RG chroma texture";
		m_lumaTexture->disposeTexture();
		m_lumaTexture.reset();
		m_chromaTexture.reset();
		return false;
	}

	m_width= width;
	m_height= height;

	if (!registerWithCuda())
	{
		m_lumaTexture->disposeTexture();
		m_lumaTexture.reset();
		m_chromaTexture->disposeTexture();
		m_chromaTexture.reset();
		m_width= 0;
		m_height= 0;
		return false;
	}

	return true;
}

bool CudaGLColorTexture::registerWithCuda()
{
	// WRITE_DISCARD: copyFromDevice() overwrites the entire texture every frame,
	// never reads back prior contents. Unlike the previous kernel-based design,
	// nothing here does a surface load/store (surf2Dwrite) - copyFromDevice() maps
	// each plane as a CUDA array and cuMemcpy2Ds into it directly - so
	// CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST is not needed.
	const unsigned int registerFlags= CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD;

	const bool lumaOk=
		checkCudaResult(cuGraphicsGLRegisterImage(&m_lumaGraphicsResource, m_lumaTexture->getGlTextureId(),
												  GL_TEXTURE_2D, registerFlags),
						"cuGraphicsGLRegisterImage(luma)");
	const bool chromaOk=
		checkCudaResult(cuGraphicsGLRegisterImage(&m_chromaGraphicsResource, m_chromaTexture->getGlTextureId(),
												  GL_TEXTURE_2D, registerFlags),
						"cuGraphicsGLRegisterImage(chroma)");

	return lumaOk && chromaOk;
}

void CudaGLColorTexture::unregisterFromCuda()
{
	if (m_lumaGraphicsResource != nullptr)
	{
		checkCudaResult(cuGraphicsUnregisterResource(m_lumaGraphicsResource), "cuGraphicsUnregisterResource(luma)");
		m_lumaGraphicsResource= nullptr;
	}

	if (m_chromaGraphicsResource != nullptr)
	{
		checkCudaResult(cuGraphicsUnregisterResource(m_chromaGraphicsResource), "cuGraphicsUnregisterResource(chroma)");
		m_chromaGraphicsResource= nullptr;
	}
}

void CudaGLColorTexture::shutdown()
{
	unregisterFromCuda();

	if (m_lumaTexture != nullptr)
	{
		m_lumaTexture->disposeTexture();
		m_lumaTexture.reset();
	}

	if (m_chromaTexture != nullptr)
	{
		m_chromaTexture->disposeTexture();
		m_chromaTexture.reset();
	}

	m_width= 0;
	m_height= 0;
}

bool CudaGLColorTexture::isInitialized() const
{
	return m_lumaTexture != nullptr && m_chromaTexture != nullptr && m_lumaGraphicsResource != nullptr
		   && m_chromaGraphicsResource != nullptr;
}

int CudaGLColorTexture::getWidth() const { return m_width; }

int CudaGLColorTexture::getHeight() const { return m_height; }

IMkTexturePtr CudaGLColorTexture::getLumaTexture() const { return m_lumaTexture; }

IMkTexturePtr CudaGLColorTexture::getChromaTexture() const { return m_chromaTexture; }

bool CudaGLColorTexture::resize(int width, int height)
{
	if (width == m_width && height == m_height && isInitialized())
		return true;

	shutdown();
	return init(width, height);
}

bool CudaGLColorTexture::copyFromDevice(CUdeviceptr yPlane, int yStrideBytes, CUdeviceptr uvPlane, int uvStrideBytes)
{
	if (!isInitialized())
	{
		MIKAN_LOG_ERROR("CudaGLColorTexture::copyFromDevice") << "Not initialized";
		return false;
	}

	CUgraphicsResource resources[2]= {m_lumaGraphicsResource, m_chromaGraphicsResource};
	if (!checkCudaResult(cuGraphicsMapResources(2, resources, nullptr), "cuGraphicsMapResources"))
		return false;

	bool bSuccess= true;

	CUarray lumaArray= nullptr;
	bSuccess= bSuccess
			  && checkCudaResult(cuGraphicsSubResourceGetMappedArray(&lumaArray, m_lumaGraphicsResource, 0, 0),
								 "cuGraphicsSubResourceGetMappedArray(luma)");

	CUarray chromaArray= nullptr;
	bSuccess= bSuccess
			  && checkCudaResult(cuGraphicsSubResourceGetMappedArray(&chromaArray, m_chromaGraphicsResource, 0, 0),
								 "cuGraphicsSubResourceGetMappedArray(chroma)");

	if (bSuccess)
	{
		// Y plane: full resolution, 1 byte/pixel (MK_RED/R8).
		CUDA_MEMCPY2D lumaCopy= {};
		lumaCopy.srcMemoryType= CU_MEMORYTYPE_DEVICE;
		lumaCopy.srcDevice= yPlane;
		lumaCopy.srcPitch= static_cast<size_t>(yStrideBytes);
		lumaCopy.dstMemoryType= CU_MEMORYTYPE_ARRAY;
		lumaCopy.dstArray= lumaArray;
		lumaCopy.WidthInBytes= static_cast<size_t>(m_width);
		lumaCopy.Height= static_cast<size_t>(m_height);
		bSuccess= bSuccess && checkCudaResult(cuMemcpy2D(&lumaCopy), "cuMemcpy2D(luma)");

		// UV plane: half resolution in each dimension, 2 bytes/pixel interleaved
		// U/V (MK_RG/RG8) - a full-width row of interleaved UV bytes is exactly
		// m_width bytes (half as many pixel pairs, 2 bytes each).
		CUDA_MEMCPY2D chromaCopy= {};
		chromaCopy.srcMemoryType= CU_MEMORYTYPE_DEVICE;
		chromaCopy.srcDevice= uvPlane;
		chromaCopy.srcPitch= static_cast<size_t>(uvStrideBytes);
		chromaCopy.dstMemoryType= CU_MEMORYTYPE_ARRAY;
		chromaCopy.dstArray= chromaArray;
		chromaCopy.WidthInBytes= static_cast<size_t>(m_width);
		chromaCopy.Height= static_cast<size_t>(m_height / 2);
		bSuccess= bSuccess && checkCudaResult(cuMemcpy2D(&chromaCopy), "cuMemcpy2D(chroma)");
	}

	const bool unmapOk= checkCudaResult(cuGraphicsUnmapResources(2, resources, nullptr), "cuGraphicsUnmapResources");

	return bSuccess && unmapOk;
}
