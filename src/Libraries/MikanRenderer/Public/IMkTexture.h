#pragma once

#include "MkRendererExport.h"
#include "MkRendererFwd.h"

#include <filesystem>
#include <stdint.h>

// TODO: Replace this with an enum to abstract OpenGL buffer/texture format constants
#define MK_RED 0x1903
#define MK_R32F 0x822E
#define MK_RGBA 0x1908
#define MK_RGB 0x1907
#define MK_RGB16 0x8054
#define MK_RGBA16 0x805B
#define MK_RGBA16F 0x881A
#define MK_BGR 0x80E0
#define MK_BGRA 0x80E1
#define MK_RG32F 0x8230
#define MK_RG 0x8227

class IMkTexture
{
public:
	enum class PixelBufferObjectMode
	{
		NoPBO,
		SinglePBOWrite,
		DoublePBOWrite,
		SinglePBORead,
		DoublePBORead,
	};

	enum class TextureWrapMode
	{
		ClampToEdge,
		Repeat,
		MirroredRepeat,
	};

	virtual ~IMkTexture() {}

	virtual IMkTexture* setName(const std::string& name)= 0;
	virtual IMkTexture* setSize(uint16_t width, uint16_t height)= 0;
	virtual IMkTexture* setTextureMapData(const uint8_t* textureMapData)= 0;
	virtual IMkTexture* setTextureFormat(uint32_t textureFormat)= 0;
	virtual IMkTexture* setBufferFormat(uint32_t bufferFormat)= 0;
	virtual IMkTexture* setPixelType(uint32_t pixelType)= 0;
	virtual IMkTexture* setGenerateMipMap(bool bFlag)= 0;
	virtual IMkTexture* setPixelBufferObjectMode(PixelBufferObjectMode mode)= 0;
	virtual IMkTexture* setTextureWrapMode(TextureWrapMode mode)= 0;

	virtual void setImagePath(const std::filesystem::path& path)= 0;
	virtual const std::filesystem::path getImagePath() const= 0;
	virtual bool reloadTextureFromImagePath()= 0;

	virtual bool createTexture()= 0;
	virtual void copyBufferIntoTexture(const uint8_t* buffer, size_t bufferSize)= 0;
	// Per-frame readback through the pixel buffer objects the texture was created
	// with. In a double PBO read mode the pixels handed back are the ones captured
	// by the previous call, so the read never waits on the GPU, and the first call
	// after creation returns false because nothing has been captured yet. In a
	// single PBO read mode the pixels are current and the call waits. Without a
	// read PBO this falls back to readTextureIntoBuffer.
	virtual bool copyTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize)= 0;
	// Synchronous read that needs no pixel buffer object, for the occasional
	// one-off read of a texture that was never set up for streaming readback,
	// such as pulling a GPU-direct video frame back for marker detection.
	// Returns false when the texture is invalid or the buffer is the wrong size.
	virtual bool readTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize)= 0;
	virtual void disposeTexture()= 0;

	virtual bool bindTexture(int textureUnit= 0) const= 0;
	virtual void clearTexture(int textureUnit= 0) const= 0;

	virtual const std::string getName() const= 0;
	virtual bool getIsValid() const= 0;
	virtual void* getPlatformTexture()= 0;
	virtual uint32_t getGlTextureId() const= 0;
	virtual uint16_t getTextureWidth() const= 0;
	virtual uint16_t getTextureHeight() const= 0;
	virtual uint32_t getTextureFormat() const= 0;
	virtual uint32_t getBufferFormat() const= 0;
};

class IMkExternalTexture : public IMkTexture
{
public:
	virtual void setExternalPlatformTexture(void* platformTexture)= 0;
};

MIKAN_RENDERER_FUNC(IMkTexturePtr) CreateMkTexture();
MIKAN_RENDERER_FUNC(IMkTexturePtr) CreateMkTexture(uint16_t width, uint16_t height, const uint8_t* textureMapData,
												   uint32_t textureFormat, uint32_t bufferFormat);

MIKAN_RENDERER_FUNC(IMkExternalTexturePtr) CreateMkExternalTexture();
MIKAN_RENDERER_FUNC(IMkExternalTexturePtr) CreateMkExternalTexture(void* platformTexture);

MIKAN_RENDERER_FUNC(bool) saveMkTextureToPNG(IMkTexturePtr texture, const char* filename);

/// Read the default framebuffer's back buffer and write it as a PNG.
/// Call with the target window's GL context bound, after rendering completes
/// and before the frame is presented.
MIKAN_RENDERER_FUNC(bool) saveDefaultFramebufferToPNG(int width, int height, const char* filename);