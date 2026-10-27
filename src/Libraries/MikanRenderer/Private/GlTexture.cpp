#include "IMkTexture.h"
#include "GlCommon.h"
#include "Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class GlTexture : public IMkTexture
{
public:
	GlTexture()
		: m_width(0)
		, m_height(0)
		, m_textureMapData(nullptr)
		, m_pixelType(GL_UNSIGNED_BYTE)
		, m_wrapMode(TextureWrapMode::ClampToEdge)
	{
	}

	GlTexture(uint16_t width, uint16_t height, const uint8_t* textureMapData, uint32_t textureFormat,
			  uint32_t bufferFormat)
		: m_width(width)
		, m_height(height)
		, m_textureMapData(textureMapData)
		, m_textureFormat(textureFormat)
		, m_bufferFormat(bufferFormat)
	{
		determinePixelType();
	}

	virtual ~GlTexture() { disposeTexture(); }

	virtual IMkTexture* setName(const std::string& name) override
	{
		m_name= name;

		return this;
	}

	virtual IMkTexture* setSize(uint16_t width, uint16_t height) override
	{
		m_width= width;
		m_height= height;

		return this;
	}

	virtual IMkTexture* setTextureMapData(const uint8_t* textureMapData) override
	{
		m_textureMapData= textureMapData;

		return this;
	}

	virtual IMkTexture* setTextureFormat(uint32_t textureFormat) override
	{
		m_textureFormat= textureFormat;
		m_bufferFormat= textureFormat;
		determinePixelType();

		return this;
	}

	virtual IMkTexture* setBufferFormat(uint32_t bufferFormat) override
	{
		m_bufferFormat= bufferFormat;

		return this;
	}

	virtual IMkTexture* setPixelType(uint32_t pixelType) override
	{
		m_pixelType= pixelType;

		return this;
	}

	virtual IMkTexture* setGenerateMipMap(bool bFlag) override
	{
		m_bGenerateMipMap= bFlag;

		return this;
	}

	virtual IMkTexture* setPixelBufferObjectMode(PixelBufferObjectMode mode) override
	{
		m_pboMode= mode;

		return this;
	}

	virtual IMkTexture* setTextureWrapMode(TextureWrapMode mode) override
	{
		m_wrapMode= mode;

		return this;
	}

	virtual void setImagePath(const std::filesystem::path& path) override { m_imagePath= path; }

	virtual const std::filesystem::path getImagePath() const override { return m_imagePath; }

	virtual bool reloadTextureFromImagePath() override
	{
		if (m_imagePath.empty())
		{
			MIKAN_LOG_ERROR("reloadTextureFromImagePath") << "Image filename is empty";
			return false;
		}

		if (!std::filesystem::exists(m_imagePath))
		{
			MIKAN_LOG_ERROR("reloadTextureFromImagePath") << "Given filename does not exist at path: " << m_imagePath;
			return false;
		}

		// Free any existing texture data
		disposeTexture();

		int width, height, nrComponents;
		unsigned char* data= stbi_load(m_imagePath.string().c_str(), &width, &height, &nrComponents, 0);
		if (data == nullptr)
		{
			MIKAN_LOG_ERROR("reloadTextureFromImagePath") << "Texture failed to load at path: " << m_imagePath;
			stbi_image_free(data);
		}

		GLenum format= 0;

		if (nrComponents == 1)
			format= GL_RED;
		else if (nrComponents == 3)
			format= GL_RGB;
		else if (nrComponents == 4)
			format= GL_RGBA;

		if (format != 0)
		{
			m_width= width;
			m_height= height;
			m_textureMapData= data;
			m_textureFormat= format;
			m_bufferFormat= format;
			m_pixelType= GL_UNSIGNED_BYTE;

			if (!createTexture())
			{
				MIKAN_LOG_ERROR("reloadTextureFromImagePath")
					<< "Failed to create GL Texture from image at path: " << m_imagePath;
			}
		}

		stbi_image_free(data);

		return true;
	}

	virtual bool createTexture() override
	{
		if (m_width > 0 && m_height > 0)
		{
			glGenTextures(1, &m_glTextureId);
			glBindTexture(GL_TEXTURE_2D, m_glTextureId);

			if (!m_name.empty())
			{
				glObjectLabel(GL_TEXTURE, m_glTextureId, -1, m_name.c_str());
			}

			if (m_textureFormat == GL_R8)
			{
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			}
			else
			{
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			}

			glTexImage2D(GL_TEXTURE_2D, 0, m_textureFormat, m_width, m_height, 0, m_bufferFormat, m_pixelType,
						 m_textureMapData);

			bool bMipmapGenerated= false;
			if (m_bGenerateMipMap && m_textureMapData != nullptr && m_textureFormat != GL_R8
				&& m_bufferFormat != GL_DEPTH_COMPONENT)
			{
				glGenerateMipmap(GL_TEXTURE_2D);
				bMipmapGenerated= true;
			}

			GLenum glWrapMode= GL_CLAMP_TO_EDGE;
			switch (m_wrapMode)
			{
			case TextureWrapMode::ClampToEdge:
				glWrapMode= GL_CLAMP_TO_EDGE;
				break;
			case TextureWrapMode::Repeat:
				glWrapMode= GL_REPEAT;
				break;
			case TextureWrapMode::MirroredRepeat:
				glWrapMode= GL_MIRRORED_REPEAT;
				break;
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapMode);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapMode);

			if (m_textureFormat == GL_R16UI || m_textureFormat == GL_R32F || m_textureFormat == GL_RG32F
				|| m_textureFormat == GL_RGB32F || m_textureFormat == GL_RGBA32F
				|| m_textureFormat == GL_DEPTH_COMPONENT32F)
			{
				// Integer textures only support NEAREST filtering
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			}
			else
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				// Use trilinear filtering if mipmaps were generated
				if (bMipmapGenerated)
				{
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
				}
				else
				{
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				}
			}

			glBindTexture(GL_TEXTURE_2D, 0);

			if (m_pboMode != PixelBufferObjectMode::NoPBO)
			{
				// Assumes no extra padding in stride
				m_PBOByteSize= m_width * m_height * getBytesPerPixel(m_bufferFormat, m_pixelType);
				m_pboWriteIndex= 0;
				m_bPboReadPrimed= false;

				switch (m_pboMode)
				{
				case PixelBufferObjectMode::DoublePBOWrite:
				{
					glGenBuffers(2, m_glPixelBufferObjectIDs);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_glPixelBufferObjectIDs[0]);
					glBufferData(GL_PIXEL_UNPACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_DRAW);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_glPixelBufferObjectIDs[1]);
					glBufferData(GL_PIXEL_UNPACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_DRAW);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				}
				break;
				case PixelBufferObjectMode::SinglePBOWrite:
				{
					glGenBuffers(1, m_glPixelBufferObjectIDs);
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_glPixelBufferObjectIDs[0]);
					glBufferData(GL_PIXEL_UNPACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_DRAW);
					m_glPixelBufferObjectIDs[1]= 0;
					glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
				}
				break;
				case PixelBufferObjectMode::DoublePBORead:
				{
					glGenBuffers(2, m_glPixelBufferObjectIDs);
					glBindBuffer(GL_PIXEL_PACK_BUFFER, m_glPixelBufferObjectIDs[0]);
					glBufferData(GL_PIXEL_PACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_COPY);
					glBindBuffer(GL_PIXEL_PACK_BUFFER, m_glPixelBufferObjectIDs[1]);
					glBufferData(GL_PIXEL_PACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_COPY);
					glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
				}
				break;
				case PixelBufferObjectMode::SinglePBORead:
				{
					glGenBuffers(1, m_glPixelBufferObjectIDs);
					glBindBuffer(GL_PIXEL_PACK_BUFFER, m_glPixelBufferObjectIDs[0]);
					glBufferData(GL_PIXEL_PACK_BUFFER, m_PBOByteSize, 0, GL_STREAM_COPY);
					m_glPixelBufferObjectIDs[1]= 0;
					glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
				}
				break;
				}
			}

			return true;
		}

		return false;
	}

	virtual void copyBufferIntoTexture(const uint8_t* buffer, size_t bufferSize) override
	{
		if (m_glTextureId != 0)
		{
			// TODO: Move this to a GLStateModifier
			class ScopedPixelStoreSetting
			{
			public:
				ScopedPixelStoreSetting(GLenum pname, GLint param)
					: m_pname(pname)
					, m_param(param)
				{
					glGetIntegerv(m_pname, &m_prevParam);
				}

				void apply()
				{
					if (m_prevParam != m_param)
					{
						glPixelStorei(m_pname, m_param);
					}
				}

				void revert()
				{
					if (m_prevParam != m_param)
					{
						glPixelStorei(m_pname, m_prevParam);
					}
				}

			protected:
				GLenum m_pname;
				GLint m_param;
				GLint m_prevParam;
			};
			std::vector<ScopedPixelStoreSetting> scopedPixelStoreSettings;

			if (m_pixelType == GL_UNSIGNED_BYTE)
			{
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_SWAP_BYTES, GL_FALSE));
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_LSB_FIRST, GL_TRUE));
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_ROW_LENGTH, 0));
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_SKIP_PIXELS, 0));
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_SKIP_ROWS, 0));
				scopedPixelStoreSettings.push_back(ScopedPixelStoreSetting(GL_UNPACK_ALIGNMENT, 1));

				for (auto& setting : scopedPixelStoreSettings)
				{
					setting.apply();
				}
			}

			glBindTexture(GL_TEXTURE_2D, m_glTextureId);

			// See http://www.songho.ca/opengl/gl_pbo.html#create
			if (m_pboMode == PixelBufferObjectMode::SinglePBOWrite
				|| m_pboMode == PixelBufferObjectMode::DoublePBOWrite)
			{
				// Make sure buffer size matches the PBO size
				assert(bufferSize == m_PBOByteSize);

				int nextPBOIndex= 0;

				if (m_pboMode == PixelBufferObjectMode::SinglePBOWrite)
				{
					// In single PBO mode, the index and nextIndex are set to 0
					m_pboWriteIndex= nextPBOIndex= 0;
				}
				else if (m_pboMode == PixelBufferObjectMode::DoublePBOWrite)
				{
					// In double PBO mode, increment current index first then get the next index
					m_pboWriteIndex= (m_pboWriteIndex + 1) % 2;
					nextPBOIndex= (m_pboWriteIndex + 1) % 2;
				}

				// Bind the the current PBO to write to the texture
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_glPixelBufferObjectIDs[m_pboWriteIndex]);

				// Copy pixels from PBO to texture object
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_bufferFormat, m_pixelType,
								0); // Treated as offset instead of pointer to pixel data

				// Bind PBO to fill in from main memory
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_glPixelBufferObjectIDs[nextPBOIndex]);

				// Map the buffer object into system memory
				// Note that glMapBuffer() causes sync issue.
				// If GPU is working with this buffer, glMapBuffer() will wait(stall)
				// for GPU to finish its job. To avoid waiting (stall), you can call
				// first glBufferData() with NULL pointer before glMapBuffer().
				// If you do that, the previous data in PBO will be discarded and
				// glMapBuffer() returns a new allocated pointer immediately
				// even if GPU is still working with the previous data.
				glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize, 0, GL_STREAM_DRAW);
				GLubyte* writePointer= (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
				if (writePointer)
				{
					// update data directly on the mapped buffer
					std::memcpy(writePointer, buffer, bufferSize);
					glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
				}

				// it is good idea to release PBOs with ID 0 after use.
				// Once bound with 0, all pixel operations behave normal ways.
				glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			}
			else
			{
				// Copy pixels from system memory to GPU memory
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, m_bufferFormat, m_pixelType, buffer);
			}

			glBindTexture(GL_TEXTURE_2D, 0);

			// Restore the previous pixel store settings
			for (auto& setting : scopedPixelStoreSettings)
			{
				setting.revert();
			}
		}
	}

	virtual bool readTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize) override
	{
		if (outBuffer == nullptr || m_glTextureId == 0)
		{
			return false;
		}

		const size_t expectedSize=
			(size_t)m_width * (size_t)m_height * (size_t)getBytesPerPixel(m_bufferFormat, m_pixelType);
		if (expectedSize == 0 || bufferSize < expectedSize)
		{
			return false;
		}

		// Drain any error left behind by earlier, unrelated GL work. glGetError
		// reports the oldest flagged error rather than the state of the last
		// call, so without this the check below fails the read for something
		// that happened elsewhere entirely.
		while (glGetError() != GL_NO_ERROR)
		{
		}

		// Straight synchronous read, no pixel buffer object. The stall this
		// incurs is the point: the caller wants these pixels now, once, rather
		// than every frame.
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_glTextureId);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glGetTexImage(GL_TEXTURE_2D, 0, m_bufferFormat, m_pixelType, outBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);

		return glGetError() == GL_NO_ERROR;
	}

	virtual bool copyTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize) override
	{
		if (outBuffer == nullptr || m_glTextureId == 0)
		{
			return false;
		}

		if (m_pboMode != PixelBufferObjectMode::SinglePBORead && m_pboMode != PixelBufferObjectMode::DoublePBORead)
		{
			// No pixel buffer object to stream through, so the read is the synchronous one
			return readTextureIntoBuffer(outBuffer, bufferSize);
		}

		if (bufferSize < m_PBOByteSize)
		{
			return false;
		}

		// Queue this call's transfer into the write PBO. With a pack buffer bound the
		// pointer argument is an offset into that buffer, so the copy runs on the GPU
		// and this returns without waiting for it.
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_glTextureId);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, m_glPixelBufferObjectIDs[m_pboWriteIndex]);
		glGetTexImage(GL_TEXTURE_2D, 0, m_bufferFormat, m_pixelType, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		bool bRead= false;
		if (m_pboMode == PixelBufferObjectMode::SinglePBORead)
		{
			// One buffer: mapping it waits for the transfer just queued, so the pixels
			// are current and the stall is the price of this mode.
			bRead= copyBoundPackBufferInto(outBuffer);
		}
		else
		{
			// Two buffers: map the one the previous call filled, which the GPU has had
			// a whole call to finish, so this does not wait. The pixels are one call
			// old, and the first call after creation has nothing to hand back yet.
			const uint16_t readIndex= (m_pboWriteIndex + 1) % 2;
			if (m_bPboReadPrimed)
			{
				glBindBuffer(GL_PIXEL_PACK_BUFFER, m_glPixelBufferObjectIDs[readIndex]);
				bRead= copyBoundPackBufferInto(outBuffer);
			}
			m_bPboReadPrimed= true;
			m_pboWriteIndex= readIndex;
		}

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		return bRead;
	}

	virtual void disposeTexture() override
	{
		if (m_glTextureId != 0)
		{
			glDeleteTextures(1, &m_glTextureId);
			m_glTextureId= 0;
		}

		switch (m_pboMode)
		{
		case PixelBufferObjectMode::DoublePBOWrite:
		case PixelBufferObjectMode::DoublePBORead:
			glDeleteBuffers(2, m_glPixelBufferObjectIDs);
			break;
		case PixelBufferObjectMode::SinglePBOWrite:
		case PixelBufferObjectMode::SinglePBORead:
			glDeleteBuffers(1, m_glPixelBufferObjectIDs);
			break;
		default:
			break;
		}
		m_glPixelBufferObjectIDs[0]= 0;
		m_glPixelBufferObjectIDs[1]= 0;
		m_pboWriteIndex= 0;
		m_bPboReadPrimed= false;
	}

	virtual bool bindTexture(int textureUnit) const override
	{
		if (m_glTextureId != 0)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, m_glTextureId);
			return true;
		}

		return false;
	}

	virtual void clearTexture(int textureUnit) const override
	{
		if (m_glTextureId != 0)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	virtual const std::string getName() const override { return m_name; }
	virtual bool getIsValid() const override { return m_glTextureId != 0; }
	virtual void* getPlatformTexture() override { return &m_glTextureId; }
	virtual uint32_t getGlTextureId() const override { return m_glTextureId; }
	virtual uint16_t getTextureWidth() const override { return m_width; }
	virtual uint16_t getTextureHeight() const override { return m_height; }
	virtual uint32_t getTextureFormat() const override { return m_textureFormat; }
	virtual uint32_t getBufferFormat() const override { return m_bufferFormat; }

protected:
	void determinePixelType()
	{
		GLenum pixelType;
		switch (m_textureFormat)
		{
		case GL_R32F:
		case GL_RG32F:
		case GL_RGB32F:
		case GL_RGBA32F:
		case GL_DEPTH_COMPONENT32F:
			m_pixelType= GL_FLOAT;
			break;
		case GL_RGB16F:
		case GL_RGBA16F:
			m_pixelType= GL_HALF_FLOAT;
			break;
		case GL_DEPTH_COMPONENT16:
		case GL_RGB16:
		case GL_RGBA16:
			m_pixelType= GL_UNSIGNED_SHORT;
			break;
		default:
			m_pixelType= GL_UNSIGNED_BYTE;
		}
	}

	size_t getBytesPerPixel(uint32_t format, uint32_t pixelType)
	{
		size_t bytesPerChannel= 0;
		switch (pixelType)
		{
		case GL_UNSIGNED_BYTE:
		case GL_BYTE:
			bytesPerChannel= 1;
			break;
		case GL_UNSIGNED_SHORT:
		case GL_SHORT:
		case GL_HALF_FLOAT:
			bytesPerChannel= 2;
			break;
		case GL_FLOAT:
		case GL_UNSIGNED_INT_24_8:
			bytesPerChannel= 4;
			break;
		default:
		{
			MIKAN_LOG_ERROR("getBytesPerPixel") << "Unknown pixelType: " << format;
			assert(false);
		}
		}

		size_t bytesPerPixel= 0;
		switch (format)
		{
		case GL_RED:
		{
			bytesPerPixel= bytesPerChannel;
		}
		break;
		case GL_RG:
		{
			bytesPerPixel= bytesPerChannel * 2;
		}
		break;
		case GL_RGB:
		case GL_BGR:
		{
			bytesPerPixel= bytesPerChannel * 3;
		}
		break;
		case GL_RGBA:
		case GL_BGRA:
		{
			bytesPerPixel= bytesPerChannel * 4;
		}
		break;
		case GL_DEPTH_COMPONENT:
		{
			bytesPerPixel= bytesPerChannel;
		}
		break;
		case GL_DEPTH_STENCIL:
		{
			assert(pixelType == GL_UNSIGNED_INT_24_8);
			bytesPerPixel= bytesPerChannel;
		}
		break;
		default:
		{
			MIKAN_LOG_ERROR("getBytesPerPixel") << "Unknown format: " << format;
			assert(false);
		}
		}

		return bytesPerPixel;
	}

protected:
	bool copyBoundPackBufferInto(uint8_t* outBuffer) const
	{
		const GLubyte* readPointer= (const GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
		if (readPointer == nullptr)
		{
			return false;
		}

		std::memcpy(outBuffer, readPointer, m_PBOByteSize);
		glUnmapBuffer(GL_PIXEL_PACK_BUFFER);

		return true;
	}

	std::string m_name;
	uint32_t m_glTextureId= 0;
	uint16_t m_width= 0;
	uint16_t m_height= 0;
	uint32_t m_textureFormat= 0;
	uint32_t m_bufferFormat= 0;
	uint32_t m_pixelType= 0;
	uint32_t m_glPixelBufferObjectIDs[2]= {0, 0};
	uint16_t m_pboWriteIndex= 0;
	bool m_bPboReadPrimed= false;
	PixelBufferObjectMode m_pboMode= PixelBufferObjectMode::NoPBO;
	size_t m_PBOByteSize= 0;
	bool m_bGenerateMipMap= true;
	TextureWrapMode m_wrapMode= TextureWrapMode::ClampToEdge;
	const uint8_t* m_textureMapData;
	std::filesystem::path m_imagePath;
};

class GlExternalTexture : public IMkExternalTexture
{
public:
	GlExternalTexture() {}

	GlExternalTexture(const void* platformTexture) { setExternalPlatformTexture(const_cast<void*>(platformTexture)); }

	virtual void setExternalPlatformTexture(void* platformTexture) override
	{
		auto* glTextureId= reinterpret_cast<const uint32_t*>(platformTexture);

		if (glTextureId != nullptr)
		{
			if (*glTextureId != m_glTextureId)
			{
				m_glTextureId= *glTextureId;

				glBindTexture(GL_TEXTURE_2D, m_glTextureId);

				// Width and height
				GLint width= 0, height= 0;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
				m_width= static_cast<uint16_t>(width);
				m_height= static_cast<uint16_t>(height);

				// Internal (texture) format
				GLint internalFormat= 0;
				glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internalFormat);
				m_textureFormat= static_cast<uint32_t>(internalFormat);

				switch (internalFormat)
				{
				case GL_RGBA8:
					m_bufferFormat= GL_RGBA;
					m_pixelType= GL_UNSIGNED_BYTE;
					break;
				case GL_RGB8:
					m_bufferFormat= GL_RGB;
					m_pixelType= GL_UNSIGNED_BYTE;
					break;
				case GL_RG8:
					m_bufferFormat= GL_RG;
					m_pixelType= GL_UNSIGNED_BYTE;
					break;
				case GL_R8:
					m_bufferFormat= GL_RED;
					m_pixelType= GL_UNSIGNED_BYTE;
					break;
				case GL_RGB16:
					m_bufferFormat= GL_RGB;
					m_pixelType= GL_UNSIGNED_SHORT;
					break;
				case GL_RGBA16:
					m_bufferFormat= GL_RGBA;
					m_pixelType= GL_UNSIGNED_SHORT;
					break;
				case GL_RGBA16F:
					m_bufferFormat= GL_RGBA;
					m_pixelType= GL_HALF_FLOAT;
					break;
				case GL_RGBA32F:
					m_bufferFormat= GL_RGBA;
					m_pixelType= GL_FLOAT;
					break;
				case GL_DEPTH_COMPONENT24:
				case GL_DEPTH_COMPONENT32F:
					m_bufferFormat= GL_DEPTH_COMPONENT;
					m_pixelType= GL_FLOAT;
					break;
					// Add others as needed
				default:
					m_bufferFormat= GL_RGBA;
					m_pixelType= GL_UNSIGNED_BYTE;
					break;
				}

				glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
		else
		{
			m_glTextureId= 0;
			m_width= 0;
			m_height= 0;
			m_textureFormat= 0;
			m_bufferFormat= 0;
			m_pixelType= 0;
		}
	}

	virtual IMkTexture* setName(const std::string& name) override
	{
		m_name= name;

		return this;
	}

	virtual IMkTexture* setSize(uint16_t width, uint16_t height) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setTextureMapData(const uint8_t* textureMapData) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setTextureFormat(uint32_t textureFormat) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setBufferFormat(uint32_t bufferFormat) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setPixelType(uint32_t pixelType) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setGenerateMipMap(bool bFlag) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setPixelBufferObjectMode(PixelBufferObjectMode mode) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual IMkTexture* setTextureWrapMode(TextureWrapMode mode) override
	{
		// Do nothing since this class is for external textures that are created outside of this class

		return this;
	}

	virtual void setImagePath(const std::filesystem::path& path) override
	{
		// Do nothing since this class is for external textures that are created outside of this class
	}

	virtual const std::filesystem::path getImagePath() const override
	{
		// Do nothing since this class is for external textures that are created outside of this class
		return {};
	}

	virtual bool reloadTextureFromImagePath() override
	{
		// Do nothing since this class is for external textures that are created outside of this class
		return false;
	}

	virtual bool createTexture() override
	{
		// Do nothing since this class is for external textures that are created outside of this class
		return false;
	}

	virtual void copyBufferIntoTexture(const uint8_t* buffer, size_t bufferSize) override
	{
		// Do nothing
		return;
	}

	virtual bool copyTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize) override
	{
		// An external texture owns no pixel buffer objects to stream through
		return false;
	}

	virtual bool readTextureIntoBuffer(uint8_t* outBuffer, size_t bufferSize) override
	{
		// An external texture's format and pixel type were chosen by whoever
		// created it, so there is nothing reliable to hand glGetTexImage here.
		return false;
	}

	virtual void disposeTexture() override
	{
		// Forget about the external texture, but don't delete it since we don't own it
		m_glTextureId= 0;
	}

	virtual bool bindTexture(int textureUnit) const override
	{
		if (m_glTextureId != 0)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, m_glTextureId);
			return true;
		}

		return false;
	}

	virtual void clearTexture(int textureUnit) const override
	{
		if (m_glTextureId != 0)
		{
			glActiveTexture(GL_TEXTURE0 + textureUnit);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	virtual const std::string getName() const override { return m_name; }
	virtual bool getIsValid() const override { return m_glTextureId != 0; }
	virtual void* getPlatformTexture() override { return &m_glTextureId; }
	virtual uint32_t getGlTextureId() const override { return m_glTextureId; }
	virtual uint16_t getTextureWidth() const override { return m_width; }
	virtual uint16_t getTextureHeight() const override { return m_height; }
	virtual uint32_t getTextureFormat() const override { return m_textureFormat; }
	virtual uint32_t getBufferFormat() const override { return m_bufferFormat; }

protected:
	std::string m_name;
	uint32_t m_glTextureId= 0;
	uint16_t m_width= 0;
	uint16_t m_height= 0;
	uint32_t m_textureFormat= 0;
	uint32_t m_bufferFormat= 0;
	uint32_t m_pixelType= 0;
};

IMkTexturePtr CreateMkTexture() { return std::make_shared<GlTexture>(); }

IMkTexturePtr CreateMkTexture(uint16_t width, uint16_t height, const uint8_t* textureMapData, uint32_t textureFormat,
							  uint32_t bufferFormat)
{
	return std::make_shared<GlTexture>(width, height, textureMapData, textureFormat, bufferFormat);
}

IMkExternalTexturePtr CreateMkExternalTexture() { return std::make_shared<GlExternalTexture>(); }

IMkExternalTexturePtr CreateMkExternalTexture(void* platformTexture)
{
	return std::make_shared<GlExternalTexture>(platformTexture);
}

bool saveMkTextureToPNG(IMkTexturePtr texture, const char* filename)
{
	int channels= 0;
	bool bIsBGR= false;
	bool bIs16Bit= false;
	switch (texture->getBufferFormat())
	{
	case MK_RGB:
		channels= 3;
		break;
	case MK_BGR:
		channels= 3;
		bIsBGR= true;
		break;
	case MK_RGBA:
		channels= 4;
		break;
	case MK_BGRA:
		channels= 4;
		bIsBGR= true;
		break;
	case MK_RGB16:
		channels= 3;
		bIs16Bit= true;
		break;
	case MK_RGBA16:
		channels= 4;
		bIs16Bit= true;
		break;
	default:
		break;
	}

	const int width= texture->getTextureWidth();
	const int height= texture->getTextureHeight();

	if (width == 0 || height == 0 || channels == 0)
	{
		return false;
	}

	const size_t bytesPerChannel= bIs16Bit ? 2 : 1;
	const size_t bufferSize= (size_t)width * height * channels * bytesPerChannel;
	uint8_t* buffer= new uint8_t[bufferSize];

	// Synchronous read rather than copyTextureIntoBuffer. That path hands back the previous
	// call's pixels in a double PBO read mode, which is how framebuffer color textures are
	// created, so a one-shot caller would get nothing on its first call.
	if (!texture->readTextureIntoBuffer(buffer, bufferSize))
	{
		delete[] buffer;
		return false;
	}

	// Convert 16-bit HDR to 8-bit sRGB and optionally swap R/B
	uint8_t* pngBuffer= nullptr;
	if (bIs16Bit)
	{
		pngBuffer= new uint8_t[(size_t)width * height * channels];
		for (size_t i= 0; i < (size_t)width * height * channels; i++)
		{
			// Reconstruct the 16-bit value (little-endian) and normalize to [0, 1]
			float color= (float)(buffer[i * 2] | (buffer[i * 2 + 1] << 8)) / 65535.0f;

			// Reinhard tonemapping: x / (x + 1)
			color= color / (color + 1.0f);

			// Gamma correction (linear to sRGB, gamma ~2.2)
			color= std::pow(color, 1.0f / 2.2f);

			// Clamp and quantize to 8-bit
			color= std::clamp(color, 0.0f, 1.0f);
			pngBuffer[i]= static_cast<uint8_t>(color * 255.0f);
		}
		delete[] buffer;
	}
	else
	{
		pngBuffer= buffer;
	}

	if (bIsBGR)
	{
		// Swap R and B channels so stbi_write_png outputs standard RGB(A)
		for (size_t i= 0; i < (size_t)width * height * channels; i+= channels)
		{
			std::swap(pngBuffer[i], pngBuffer[i + 2]);
		}
	}

	const int stride= width * channels;
	const bool bSuccess= stbi_write_png(filename, width, height, channels, pngBuffer, stride) != 0;

	delete[] pngBuffer;

	return bSuccess;
}

bool saveDefaultFramebufferToPNG(int width, int height, const char* filename)
{
	if (width <= 0 || height <= 0)
	{
		return false;
	}

	const int channels= 3;
	const int stride= width * channels;
	const size_t bufferSize= (size_t)stride * height;
	uint8_t* buffer= new uint8_t[bufferSize];

	// Read the back buffer, preserving the caller's read framebuffer and pack state
	GLint prevReadFramebuffer= 0;
	GLint prevPackAlignment= 4;
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFramebuffer);
	glGetIntegerv(GL_PACK_ALIGNMENT, &prevPackAlignment);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glReadBuffer(GL_BACK);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	glPixelStorei(GL_PACK_ALIGNMENT, prevPackAlignment);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, prevReadFramebuffer);

	// glReadPixels rows are bottom-up; flip vertically for the PNG
	for (int row= 0; row < height / 2; ++row)
	{
		uint8_t* topRow= buffer + (size_t)row * stride;
		uint8_t* bottomRow= buffer + (size_t)(height - 1 - row) * stride;
		for (int i= 0; i < stride; ++i)
		{
			std::swap(topRow[i], bottomRow[i]);
		}
	}

	const bool bSuccess= stbi_write_png(filename, width, height, channels, buffer, stride) != 0;

	delete[] buffer;

	return bSuccess;
}