#include "PoseTrackAssetReference.h"
#include "PathUtils.h"

// -- PoseTrackAssetReferenceFactory -----
PoseTrackAssetReferenceFactory::PoseTrackAssetReferenceFactory()
	: TypedAssetReferenceFactory<PoseTrackAssetReference, AssetReferenceConfig>()
{
	m_defaultPath= (PathUtils::getProjectDirectory() / "movies" / "").string();
}
