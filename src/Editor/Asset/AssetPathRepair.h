#pragma once

#include <string>
#include <vector>

// A stored asset path a project load repointed because the file it named no longer existed
struct AssetPathRepair
{
	std::string oldPath;
	std::string newPath;
};

// Repairs the stored asset paths read while it is alive. A project copied or moved
// from another folder keeps absolute paths into the old one, and each such path that
// no longer exists is repointed to the project or bundled file it names. The scope
// covers only the read of the project file, so graph and material files read later
// are left as they are.
class AssetPathRepairScope
{
public:
	AssetPathRepairScope();
	~AssetPathRepairScope();

	AssetPathRepairScope(const AssetPathRepairScope&)= delete;
	AssetPathRepairScope& operator=(const AssetPathRepairScope&)= delete;

	inline const std::vector<AssetPathRepair>& getRepairs() const { return m_repairs; }

	// Repoint a stored path read from JSON when a scope is alive and the path needs it
	static void repairStoredPath(std::string& inOutAssetPath);

private:
	std::vector<AssetPathRepair> m_repairs;

	static AssetPathRepairScope* s_activeScope;
};
