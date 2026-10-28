#pragma once

#include "OpenCVFwd.h"
#include "MkRendererFwd.h"

#include <memory>

class GLVideoFrameProcessor
{
public:
	GLVideoFrameProcessor();
	~GLVideoFrameProcessor()= default;

	void init(IMkShaderCache* shaderCache);
	void ensureBufferSize(int width, int height);

	void uploadSourceBuffer(const cv::Mat& srcBuffer);
	// The texture uploadSourceBuffer writes, for a caller undistorting a CPU frame
	inline IMkTexturePtr getUploadedSourceTexture() const { return m_bgrSourceTextureMap; }
	void computeUndistortion(IMkTexturePtr sourceTexture, IMkTexturePtr writeTexture, IMkTexturePtr distortionTexture,
							 IMkTriangulatedMeshPtr fullscreenQuad, IMkGraphicsContext* graphicsContext);

private:
	// Source texture that receives the raw BGR video frame before shader undistortion
	IMkTexturePtr m_bgrSourceTextureMap;

	// Frame buffer used to render the undistorted result via shader
	IMkFrameBufferPtr m_undistortionFrameBuffer;

	// Undistortion shader material and per-instance uniforms
	MkMaterialConstPtr m_undistortionMaterial;
	MkMaterialInstancePtr m_undistortMaterialInstance;
};
