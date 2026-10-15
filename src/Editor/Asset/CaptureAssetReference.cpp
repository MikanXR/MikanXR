#include "CaptureAssetReference.h"
#include "Capture.h"
#include "IMkTexture.h"
#include "MovieDecoder.h"
#include "PathUtils.h"

#include "opencv2/core.hpp"

// -- CaptureAssetReference -----
void CaptureAssetReference::rebuildPreview()
{
	m_previewTexture= nullptr;

	Capture capture;
	std::string error;
	if (!capture.open(getResolvedAssetPath(), error))
		return;

	cv::Mat frame;
	if (capture.hasMember(eCaptureMemberRole::plate_image))
	{
		capture.decodeImage(eCaptureMemberRole::plate_image, frame, error);
	}
	if (frame.empty() && capture.hasMember(eCaptureMemberRole::movie))
	{
		MovieDecoder decoder;
		int64_t ptsUs= 0;
		if (decoder.openMovieUrl(capture.makeSubfileUrl(eCaptureMemberRole::movie)))
		{
			decoder.readNext(frame, ptsUs);
		}
	}
	if (frame.empty() && capture.hasMember(eCaptureMemberRole::marker_image))
	{
		capture.decodeImage(eCaptureMemberRole::marker_image, frame, error);
	}
	if (frame.empty())
		return;

	m_previewTexture= CreateMkTexture((uint16_t)frame.cols, (uint16_t)frame.rows, frame.data,
									  MK_RGB,  // texture format
									  MK_BGR); // buffer format
	m_previewTexture->setGenerateMipMap(false);
	m_previewTexture->createTexture();
}

// -- CaptureAssetReferenceFactory -----
CaptureAssetReferenceFactory::CaptureAssetReferenceFactory()
	: TypedAssetReferenceFactory<CaptureAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= getDefaultCapturePath();
}

std::string CaptureAssetReferenceFactory::getDefaultCapturePath()
{
	return (PathUtils::getProjectDirectory() / "captures" / "").string();
}
