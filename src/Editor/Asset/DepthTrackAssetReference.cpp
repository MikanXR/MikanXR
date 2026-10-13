#include "DepthTrackAssetReference.h"
#include "PathUtils.h"

// -- DepthTrackAssetReferenceFactory -----
DepthTrackAssetReferenceFactory::DepthTrackAssetReferenceFactory()
	: TypedAssetReferenceFactory<DepthTrackAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= (PathUtils::getProjectDirectory() / "movies" / "").string();
}
