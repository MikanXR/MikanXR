#include "GLVideoFrameProcessor.h"

#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "Logger.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkShaderConstants.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"

#include "opencv2/core.hpp"

GLVideoFrameProcessor::GLVideoFrameProcessor() {}

void GLVideoFrameProcessor::init(IMkShaderCache* shaderCache)
{
	// Create the RGB frame buffer; size and resources are initialized lazily in computeUndistortion
	m_undistortionFrameBuffer= createMkFrameBuffer("Undistortion Frame Buffer");
	m_undistortionFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_undistortionFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGB);

	// Setup the undistortion material
	m_undistortionMaterial= shaderCache->getMaterialByName(INTERNAL_MATERIAL_PT_UNDISTORT_FULLSCREEN_RGB_TEXTURE);
	m_undistortMaterialInstance= createMkMaterialInstance(m_undistortionMaterial);
}

void GLVideoFrameProcessor::ensureBufferSize(int width, int height)
{
	// Keep the undistortion framebuffer size in sync so it gets re-created on the next render
	if (m_undistortionFrameBuffer)
	{
		m_undistortionFrameBuffer->setSize(width, height);
	}

	// Allocate a GL texture to copy the source video frame into
	// This is only used when doing shader based undistortion
	m_bgrSourceTextureMap= CreateMkTexture(width, height, nullptr,
										   MK_RGB,  // texture format
										   MK_BGR); // buffer format
	m_bgrSourceTextureMap->setGenerateMipMap(false);
	m_bgrSourceTextureMap->setPixelBufferObjectMode(IMkTexture::PixelBufferObjectMode::DoublePBOWrite);
	m_bgrSourceTextureMap->createTexture();
}

void GLVideoFrameProcessor::uploadSourceBuffer(const cv::Mat& srcBuffer)
{
	if (m_bgrSourceTextureMap)
	{
		const size_t bufferSize= srcBuffer.step[0] * srcBuffer.rows;
		m_bgrSourceTextureMap->copyBufferIntoTexture(srcBuffer.data, bufferSize);
	}
}

void GLVideoFrameProcessor::computeUndistortion(IMkTexturePtr sourceTexture, IMkTexturePtr writeTexture,
												IMkTexturePtr distortionTexture, IMkTriangulatedMeshPtr fullscreenQuad,
												IMkGraphicsContext* graphicsContext)
{
	// Point the framebuffer at the target queue slot texture.
	// If the FBO already exists this is a cheap glFramebufferTexture2D re-attach;
	// otherwise the texture is stored and used when createResources() is called below.
	m_undistortionFrameBuffer->attachColorTexture(writeTexture);

	// Lazily create (or re-create after a resolution change) the FBO
	if (!m_undistortionFrameBuffer->isValid())
	{
		if (!m_undistortionFrameBuffer->createResources())
		{
			MIKAN_LOG_ERROR("GLVideoFrameProcessor") << "Failed to create undistortion framebuffer";
			return;
		}
	}

	// Run the undistortion shader on the frame buffer
	{
		MkScopedObjectBinding colorFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
													  "GLVideoFrameProcessor Framebuffer Scope",
													  m_undistortionFrameBuffer);

		if (colorFramebufferBinding)
		{
			if (auto materialBinding= m_undistortionMaterial->bindMaterial())
			{
				m_undistortMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture, sourceTexture);
				m_undistortMaterialInstance->setTextureBySemantic(eUniformSemantic::distortionTexture,
																  distortionTexture);

				// Draw the color texture
				if (auto materialInstanceBinding= m_undistortMaterialInstance->bindMaterialInstance(materialBinding))
				{
					fullscreenQuad->drawElements();
				}
			}
		}
	}
}
