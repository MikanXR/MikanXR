#pragma once

#include "AssetFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemFwd.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// One browsable folder of project assets: a project subfolder plus the overlay of
// the same kind under the bundled resources folder. A project file shadows a
// bundled one with the same stored path. Bundled entries are read-only unless the
// app's "edit bundled resources" setting is on. The fonts folder is bundled-only.
struct ProjectAssetFolderDesc
{
	// Stable id used by the panels and the automation channel
	std::string id;
	// Localization key for the folder's display name
	const char* locKey= nullptr;
	// Subfolder under the project directory, empty when the folder is bundled-only
	std::filesystem::path projectSubfolder;
	// Subfolder under PathUtils::getResourceDirectory() that shows through behind
	// the project entries, empty when the folder has no overlay
	std::filesystem::path bundledSubfolder;
	// A read-only folder accepts no imports
	bool bReadOnly= false;
	// Entries draw as preview tiles (an image tells them apart) rather than as
	// glyph-and-name rows in a list
	bool bPreviewTiles= false;
	// One entry per material file, its sibling graph and shader sources hidden.
	// Import copies the material's folder and delete removes it. The domain is
	// the one the folder's material extension names, checked against an import.
	bool bMaterialFolder= false;
	int materialDomain= -1;
	// The asset types the folder holds, first match wins for an entry's class
	std::vector<AssetReferenceFactoryPtr> factories;
};

struct ProjectAssetEntry
{
	std::string folderId;
	// The AssetReference class the entry is browsed as
	std::string className;
	// The stored form: forward slashes, relative to the project (or to the bundled
	// resources for overlay entries), the string that goes into a property or a graph
	std::string storedPath;
	std::filesystem::path absolutePath;
	// The material name for a .mat, otherwise the filename
	std::string displayName;
	// The entry comes from the bundled resources rather than the project
	bool bBundled= false;
	// Bundled entries while the app's "edit bundled resources" setting is off
	bool bReadOnly= false;

	std::string key() const { return folderId + "|" + storedPath; }
};

struct ProjectAssetReferrer
{
	enum class Kind
	{
		graph,
		material,
		component
	};

	Kind kind= Kind::graph;
	// The graph or material name, or the component name
	std::string name;
	// The stored graph or material path, or the component class name
	std::string detail;
};

// Fixed-size payload the project Assets panel hands to ImGui drag and drop. The
// drop target looks the entry up again by folder id and stored path.
struct ProjectAssetDragPayload
{
	char folderId[32]= {};
	char storedPath[512]= {};

	ProjectAssetDragPayload()= default;
	ProjectAssetDragPayload(const ProjectAssetEntry& entry);
};

// The scanned contents of the project's asset folders, shared by the project
// Assets panel and the node editors' filtered Graph Assets panels. The scan stores
// paths only. AssetReference instances, which may own a GL preview texture, are
// created on first request from a panel and cached, so the catalog is safe to
// refresh headless and before any window exists.
class ProjectAssetCatalog
{
public:
	ProjectAssetCatalog();
	~ProjectAssetCatalog();

	inline static const char* k_dragPayloadType= "ProjectAssetEntry";

	static const std::vector<ProjectAssetFolderDesc>& getFolderDescs();
	static const ProjectAssetFolderDesc* findFolderDesc(const std::string& folderId);
	// The project folder an import of the given folder lands in, empty when the
	// folder is bundled-only or no project is loaded
	static std::filesystem::path getFolderDirectory(const std::string& folderId);

	// Binds to the project manager's load and unload delegates and scans the
	// project already loaded. The tests construct the catalog without a main
	// window and call refresh directly, which skips the component scan.
	bool startup(class MainWindow* mainWindow);
	void shutdown();

	// The developer switch that makes bundled entries writable. Rescans when it changes.
	void setBundledResourcesEditable(bool bEditable);
	inline bool isBundledResourcesEditable() const { return m_bBundledEditable; }

	// Whether the path (stored or absolute) resolves to a file under the bundled resources
	static bool isBundledPath(const std::filesystem::path& path);
	// A bundled path while bundled resources are not editable
	bool isReadOnlyPath(const std::filesystem::path& path) const;
	// The project path that would shadow a bundled file: the project directory
	// joined with the file's path relative to the resources directory. Empty when
	// the path is not bundled or no project is loaded.
	static std::filesystem::path makeProjectShadowPath(const std::filesystem::path& bundledPath);
	// The stored form for any file under the project or the bundled resources,
	// relative to whichever root holds it, otherwise the absolute path
	static std::string makeOverlayStoredPath(const std::filesystem::path& path);

	// Rescans every folder from disk and fires OnCatalogChanged
	void refresh();
	void clear();

	const std::vector<ProjectAssetEntry>& getEntries(const std::string& folderId) const;
	const ProjectAssetEntry* findEntry(const std::string& folderId, const std::string& storedPath) const;
	const ProjectAssetEntry* findEntryByStoredPath(const std::string& storedPath) const;
	const ProjectAssetEntry* findEntryByPayload(const ProjectAssetDragPayload& payload) const;

	// The entry's AssetReference for its icon, preview, and the node editor drag
	// payload. Created on first use, so only call with a GL context current.
	AssetReferencePtr getAssetReference(const ProjectAssetEntry& entry);
	// Whether the entry's file is one the factory's type accepts
	bool entryMatchesFactory(const ProjectAssetEntry& entry, const AssetReferenceFactory& factory) const;

	// Copies the file (or the material's folder) into the folder's project
	// directory and returns the stored path of the copy. A source already inside
	// that directory is returned as is. A bundled source keeps its path relative
	// to the resources folder, so the copy shadows it. A plain file whose name
	// collides gets a numeric suffix. A material whose folder exists is refused,
	// as is one without a domain, since the domain names the destination.
	bool importAsset(const std::string& folderId, const std::filesystem::path& sourcePath, std::string& outStoredPath,
					 std::string& outError);

	// Whether an upload may land in the folder under the file name: a writable folder
	// of single files (not materials) with a project loaded, and a factory of the
	// folder accepting the name's extension. Returns the folder's directory, which is
	// where the upload writes its temporary file so the final move stays on one volume.
	// Errors are plain text for the HTTP reply and the log, not localized.
	bool resolveUploadDestination(const std::string& folderId, const std::string& fileName,
								  std::filesystem::path& outFolderDir, std::string& outError);

	// Moves an uploaded file into the folder under the file name, replacing a file
	// already there: an upload of the same name is the same take sent again, and the
	// numeric suffix importAsset applies would part a take from its pose track sidecar.
	// Rescans on success. A filesystem failure also reports its error code, so a
	// caller can tell a destination held open by a reader from the rest.
	bool importUploadedAsset(const std::string& folderId, const std::string& fileName,
							 const std::filesystem::path& sourcePath, std::string& outStoredPath, bool& outReplaced,
							 std::error_code& outErrorCode, std::string& outError);

	// Every graph and material under the project or the bundled resources, and
	// every project component, that names the stored path. Graphs are read as raw
	// JSON, so no window is needed. When the target is a material, referrers inside
	// its own folder are not counted.
	std::vector<ProjectAssetReferrer> findReferences(const std::string& storedPath) const;

	// Removes the file (or the material's folder) and rescans. Refuses read-only
	// entries. Callers run findReferences first and refuse on their own terms.
	bool deleteAsset(const ProjectAssetEntry& entry, std::string& outError);

	MulticastDelegate<void()> OnCatalogChanged;

private:
	void onProjectLoaded(ProjectManagerPtr projectManager);
	void onProjectPreUnload(ProjectManagerPtr projectManager);

	void scanFolder(const ProjectAssetFolderDesc& desc, std::vector<ProjectAssetEntry>& outEntries) const;
	void scanDirectory(const ProjectAssetFolderDesc& desc, const std::filesystem::path& directory,
					   const std::filesystem::path& storedRoot, bool bBundled,
					   std::vector<ProjectAssetEntry>& outEntries) const;
	bool importMaterial(const ProjectAssetFolderDesc& desc, const std::filesystem::path& sourcePath,
						std::string& outStoredPath, std::string& outError);
	// The folder's factory whose filter patterns accept the file, null when none does
	static AssetReferenceFactoryPtr findFactoryForFile(const ProjectAssetFolderDesc& desc,
													   const std::filesystem::path& path);

	// The roots the reference scans walk: the project directory, then the bundled resources
	static std::vector<std::filesystem::path> getReferrerScanRoots();
	void findGraphReferences(const std::string& targetKey, const std::filesystem::path& excludeFolder,
							 std::vector<ProjectAssetReferrer>& outReferrers) const;
	void findMaterialReferences(const std::string& targetKey, const std::filesystem::path& excludeFolder,
								std::vector<ProjectAssetReferrer>& outReferrers) const;
	void findComponentReferences(const std::string& targetKey, std::vector<ProjectAssetReferrer>& outReferrers) const;

	// The comparison form of a path: overlay stored form, lower-cased, since
	// Windows paths compare without case
	static std::string makeComparisonKey(const std::filesystem::path& path);

private:
	class MainWindow* m_mainWindow= nullptr;
	ProjectManagerPtr m_projectManager;
	bool m_bBundledEditable= false;
	std::map<std::string, std::vector<ProjectAssetEntry>> m_entriesByFolder;
	std::map<std::string, AssetReferencePtr> m_assetRefCache;
};
