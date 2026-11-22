#include "AssetPathRepair.h"
#include "Logger.h"
#include "PathUtils.h"

#include <algorithm>
#include <assert.h>

AssetPathRepairScope* AssetPathRepairScope::s_activeScope= nullptr;

AssetPathRepairScope::AssetPathRepairScope()
{
	assert(s_activeScope == nullptr);
	s_activeScope= this;
}

AssetPathRepairScope::~AssetPathRepairScope()
{
	if (s_activeScope == this)
	{
		s_activeScope= nullptr;
	}
}

void AssetPathRepairScope::repairStoredPath(std::string& inOutAssetPath)
{
	if (s_activeScope == nullptr || inOutAssetPath.empty())
		return;

	const std::filesystem::path relocatedPath=
		PathUtils::findRelocatedProjectResource(PathUtils::utf8ToPath(inOutAssetPath));
	if (relocatedPath.empty())
		return;

	AssetPathRepair repair;
	repair.oldPath= inOutAssetPath;
	repair.newPath= PathUtils::makeStoredProjectPath(relocatedPath);
	inOutAssetPath= repair.newPath;

	MIKAN_LOG_WARNING("AssetPathRepairScope::repairStoredPath")
		<< "Asset path " << repair.oldPath << " no longer exists, repointed to " << repair.newPath;

	// Several components naming one file list it once
	std::vector<AssetPathRepair>& repairs= s_activeScope->m_repairs;
	const bool bListed= std::any_of(repairs.begin(), repairs.end(), [&repair](const AssetPathRepair& listed)
									{ return listed.oldPath == repair.oldPath; });
	if (!bListed)
	{
		repairs.push_back(repair);
	}
}
