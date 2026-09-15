#include "MovieAssetReference.h"
#include "IMkTexture.h"
#include "MovieDecoder.h"
#include "PathUtils.h"
#include "TextureAssetReference.h"

#include "opencv2/opencv.hpp"

// -- MovieAssetReference -----
void MovieAssetReference::rebuildPreview()
{
	m_previewTexture= nullptr;

	MovieDecoder decoder;
	if (!decoder.open(getResolvedAssetPath()))
		return;

	cv::Mat frame;
	int64_t ptsUs= 0;
	if (!decoder.readNext(frame, ptsUs) || frame.empty())
		return;

	m_previewTexture= CreateMkTexture((uint16_t)frame.cols, (uint16_t)frame.rows, frame.data,
									  MK_RGB,  // texture format
									  MK_BGR); // buffer format
	m_previewTexture->setGenerateMipMap(false);
	m_previewTexture->createTexture();
}

// -- MovieAssetReferenceFactory -----
MovieAssetReferenceFactory::MovieAssetReferenceFactory()
	: TypedAssetReferenceFactory<MovieAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= getDefaultMoviePath();
}

std::string MovieAssetReferenceFactory::getDefaultMoviePath()
{
	return (PathUtils::getProjectDirectory() / "movies" / "").string();
}

// -- MediaAssetReferenceFactory -----
MediaAssetReferenceFactory::MediaAssetReferenceFactory()
	: TypedAssetReferenceFactory<MovieAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= MovieAssetReferenceFactory::getDefaultMoviePath();

	char const* const* moviePatterns= MovieAssetReferenceFactory::getMovieFilterPatterns();
	for (int i= 0; i < MovieAssetReferenceFactory::getMovieFilterPatternCount(); ++i)
		m_filterPatterns.push_back(moviePatterns[i]);

	char const* const* texturePatterns= TextureAssetReferenceFactory::getTextureFilterPatterns();
	for (int i= 0; i < TextureAssetReferenceFactory::getTextureFilterPatternCount(); ++i)
		m_filterPatterns.push_back(texturePatterns[i]);
}
