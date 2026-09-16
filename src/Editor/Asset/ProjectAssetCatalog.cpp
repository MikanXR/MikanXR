#include "ProjectAssetCatalog.h"
#include "AssetReference.h"
#include "AssetReferencePropertyMetaData.h"
#include "FontAssetReference.h"
#include "LocText.h"
#include "Logger.h"
#include "MainWindow.h"
#include "CompositorMaterialAssetReference.h"
#include "ShapeMaterialAssetReference.h"
#include "MikanComponent.h"
#include "MikanObjectSystem.h"
#include "MikanPropertyDatabase.h"
#include "MikanShaderConfig.h"
#include "ModelAssetReference.h"
#include "CompositorGraphAssetReference.h"
#include "ShapeGraphAssetReference.h"
#include "PathUtils.h"
#include "PixelContentAssetReference.h"
#include "MovieAssetReference.h"
#include "PoseTrackAssetReference.h"
#include "ProjectManager.h"
#include "ScriptAssetReference.h"
#include "TextureAssetReference.h"

#include "Graphs/NodeGraphFileTypes.h"
#include "MaterialCompiler/MaterialDomain.h"

#include <configuru.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>

// -- ProjectAssetDragPayload -----
ProjectAssetDragPayload::ProjectAssetDragPayload(const ProjectAssetEntry& entry)
{
	strncpy_s(folderId, sizeof(folderId), entry.folderId.c_str(), _TRUNCATE);
	strncpy_s(storedPath, sizeof(storedPath), entry.storedPath.c_str(), _TRUNCATE);
}

// -- ProjectAssetCatalog -----
ProjectAssetCatalog::ProjectAssetCatalog() {}

ProjectAssetCatalog::~ProjectAssetCatalog() { shutdown(); }

const std::vector<ProjectAssetFolderDesc>& ProjectAssetCatalog::getFolderDescs()
{
	// Built on first use rather than at static init, since the factories are
	// polymorphic objects with their own construction order needs
	static std::vector<ProjectAssetFolderDesc> s_descs;
	if (s_descs.empty())
	{
		// Compositor and shape graphs are node graphs of different kinds, and
		// neither can stand in for the other, so each has its own folder
		ProjectAssetFolderDesc compositors;
		compositors.id= "compositors";
		compositors.locKey= "assets.folderCompositors";
		compositors.projectSubfolder= "compositors";
		compositors.bundledSubfolder= "compositors";
		compositors.factories= {AssetReferenceFactory::createFactory<CompositorGraphAssetReferenceFactory>()};
		s_descs.push_back(compositors);

		ProjectAssetFolderDesc shapes;
		shapes.id= "shapes";
		shapes.locKey= "assets.folderShapes";
		shapes.projectSubfolder= "shapes";
		shapes.bundledSubfolder= "shapes";
		shapes.factories= {AssetReferenceFactory::createFactory<ShapeGraphAssetReferenceFactory>()};
		s_descs.push_back(shapes);

		ProjectAssetFolderDesc models;
		models.id= "models";
		models.locKey= "assets.folderModels";
		models.projectSubfolder= "models";
		models.bundledSubfolder= "models";
		models.factories= {AssetReferenceFactory::createFactory<ModelAssetReferenceFactory>()};
		s_descs.push_back(models);

		ProjectAssetFolderDesc scripts;
		scripts.id= "scripts";
		scripts.locKey= "assets.folderScripts";
		scripts.projectSubfolder= "scripts";
		scripts.bundledSubfolder= "scripts";
		scripts.factories= {AssetReferenceFactory::createFactory<ScriptAssetReferenceFactory>()};
		s_descs.push_back(scripts);

		// Compositor and shape materials shade different consumers, so each domain
		// has its own folder and extension
		ProjectAssetFolderDesc compositorMaterials;
		compositorMaterials.id= "compositor_materials";
		compositorMaterials.locKey= "assets.folderCompositorMaterials";
		compositorMaterials.projectSubfolder= MaterialDomainUtils::materialFolderName(eMaterialDomain::compositor);
		compositorMaterials.bundledSubfolder= compositorMaterials.projectSubfolder;
		compositorMaterials.bMaterialFolder= true;
		compositorMaterials.materialDomain= (int)eMaterialDomain::compositor;
		compositorMaterials.factories= {
			AssetReferenceFactory::createFactory<CompositorMaterialAssetReferenceFactory>()};
		s_descs.push_back(compositorMaterials);

		ProjectAssetFolderDesc shapeMaterials;
		shapeMaterials.id= "shape_materials";
		shapeMaterials.locKey= "assets.folderShapeMaterials";
		shapeMaterials.projectSubfolder= MaterialDomainUtils::materialFolderName(eMaterialDomain::shape);
		shapeMaterials.bundledSubfolder= shapeMaterials.projectSubfolder;
		shapeMaterials.bMaterialFolder= true;
		shapeMaterials.materialDomain= (int)eMaterialDomain::shape;
		shapeMaterials.factories= {AssetReferenceFactory::createFactory<ShapeMaterialAssetReferenceFactory>()};
		s_descs.push_back(shapeMaterials);

		// The same image serves a graph texture and a DMX pixel sequence, so the
		// folder carries both types. The texture factory is first, which makes it
		// the class an image is browsed as.
		ProjectAssetFolderDesc textures;
		textures.id= "textures";
		textures.locKey= "assets.folderTextures";
		textures.projectSubfolder= "textures";
		textures.bundledSubfolder= "textures";
		textures.bPreviewTiles= true;
		// A photo's pose track sits beside the photo, so the folder lists that
		// type too. Its two-part extension keeps it from matching the image
		// factories ahead of it.
		textures.factories= {AssetReferenceFactory::createFactory<TextureAssetReferenceFactory>(),
							 AssetReferenceFactory::createFactory<PixelContentAssetReferenceFactory>(),
							 AssetReferenceFactory::createFactory<PoseTrackAssetReferenceFactory>()};
		s_descs.push_back(textures);

		// Recorded takes and the pose tracks a phone writes next to them
		ProjectAssetFolderDesc movies;
		movies.id= "movies";
		movies.locKey= "assets.folderMovies";
		movies.projectSubfolder= "movies";
		movies.bPreviewTiles= true;
		movies.factories= {AssetReferenceFactory::createFactory<MovieAssetReferenceFactory>(),
						   AssetReferenceFactory::createFactory<PoseTrackAssetReferenceFactory>()};
		s_descs.push_back(movies);

		ProjectAssetFolderDesc fonts;
		fonts.id= "fonts";
		fonts.locKey= "assets.folderFonts";
		fonts.bundledSubfolder= "font";
		fonts.bReadOnly= true;
		fonts.factories= {AssetReferenceFactory::createFactory<FontAssetReferenceFactory>()};
		s_descs.push_back(fonts);
	}

	return s_descs;
}

const ProjectAssetFolderDesc* ProjectAssetCatalog::findFolderDesc(const std::string& folderId)
{
	for (const ProjectAssetFolderDesc& desc : getFolderDescs())
	{
		if (desc.id == folderId)
		{
			return &desc;
		}
	}

	return nullptr;
}

std::filesystem::path ProjectAssetCatalog::getFolderDirectory(const std::string& folderId)
{
	const ProjectAssetFolderDesc* desc= findFolderDesc(folderId);
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	if (desc == nullptr || desc->projectSubfolder.empty() || projectDir.empty())
	{
		return std::filesystem::path();
	}

	return projectDir / desc->projectSubfolder;
}

bool ProjectAssetCatalog::startup(MainWindow* mainWindow)
{
	m_mainWindow= mainWindow;
	m_projectManager= mainWindow->getProjectManager();

	if (m_projectManager)
	{
		m_projectManager->OnProjectLoaded+= MakeDelegate(this, &ProjectAssetCatalog::onProjectLoaded);
		m_projectManager->OnProjectPreUnload+= MakeDelegate(this, &ProjectAssetCatalog::onProjectPreUnload);

		if (m_projectManager->hasLoadedProject())
		{
			refresh();
		}
	}

	return true;
}

void ProjectAssetCatalog::shutdown()
{
	if (m_projectManager)
	{
		m_projectManager->OnProjectLoaded-= MakeDelegate(this, &ProjectAssetCatalog::onProjectLoaded);
		m_projectManager->OnProjectPreUnload-= MakeDelegate(this, &ProjectAssetCatalog::onProjectPreUnload);
		m_projectManager= nullptr;
	}
	m_mainWindow= nullptr;

	m_entriesByFolder.clear();
	m_assetRefCache.clear();
}

void ProjectAssetCatalog::onProjectLoaded(ProjectManagerPtr projectManager) { refresh(); }

void ProjectAssetCatalog::onProjectPreUnload(ProjectManagerPtr projectManager) { clear(); }

void ProjectAssetCatalog::setBundledResourcesEditable(bool bEditable)
{
	if (m_bBundledEditable != bEditable)
	{
		m_bBundledEditable= bEditable;
		refresh();
	}
}

static bool isUnderFolder(const std::filesystem::path& path, const std::filesystem::path& folder)
{
	if (folder.empty())
	{
		return false;
	}

	const std::filesystem::path relative= path.lexically_normal().lexically_relative(folder.lexically_normal());
	return !relative.empty() && relative.begin()->string() != "..";
}

bool ProjectAssetCatalog::isBundledPath(const std::filesystem::path& path)
{
	if (path.empty())
	{
		return false;
	}

	const std::filesystem::path resolved= path.is_absolute() ? path : PathUtils::resolveProjectResource(path);
	return !resolved.empty() && isUnderFolder(resolved, PathUtils::getResourceDirectory());
}

bool ProjectAssetCatalog::isReadOnlyPath(const std::filesystem::path& path) const
{
	return !m_bBundledEditable && isBundledPath(path);
}

std::filesystem::path ProjectAssetCatalog::makeProjectShadowPath(const std::filesystem::path& bundledPath)
{
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	const std::filesystem::path resourceDir= PathUtils::getResourceDirectory().lexically_normal();
	const std::filesystem::path resolved=
		bundledPath.is_absolute() ? bundledPath : PathUtils::resolveProjectResource(bundledPath);
	if (projectDir.empty() || resolved.empty() || !isUnderFolder(resolved, resourceDir))
	{
		return std::filesystem::path();
	}

	return (projectDir / resolved.lexically_normal().lexically_relative(resourceDir)).lexically_normal();
}

std::string ProjectAssetCatalog::makeOverlayStoredPath(const std::filesystem::path& path)
{
	// Project first, which is what a stored path already is
	const std::string projectStored= PathUtils::makeStoredProjectPath(path);
	if (!std::filesystem::path(projectStored).is_absolute())
	{
		return projectStored;
	}

	const std::filesystem::path resourceDir= PathUtils::getResourceDirectory().lexically_normal();
	const std::filesystem::path normal= path.lexically_normal();
	if (isUnderFolder(normal, resourceDir))
	{
		return normal.lexically_relative(resourceDir).generic_string();
	}

	return projectStored;
}

void ProjectAssetCatalog::refresh()
{
	m_entriesByFolder.clear();

	for (const ProjectAssetFolderDesc& desc : getFolderDescs())
	{
		std::vector<ProjectAssetEntry>& entries= m_entriesByFolder[desc.id];
		scanFolder(desc, entries);
	}

	// Drop cached references whose file is gone. Surviving ones keep their
	// identity, so a drag payload in flight still points at a live object.
	for (auto it= m_assetRefCache.begin(); it != m_assetRefCache.end();)
	{
		bool bStillPresent= false;
		for (const auto& folderEntries : m_entriesByFolder)
		{
			for (const ProjectAssetEntry& entry : folderEntries.second)
			{
				if (entry.key() == it->first)
				{
					bStillPresent= true;
					break;
				}
			}
			if (bStillPresent)
				break;
		}

		if (bStillPresent)
			++it;
		else
			it= m_assetRefCache.erase(it);
	}

	if (OnCatalogChanged)
	{
		OnCatalogChanged();
	}
}

void ProjectAssetCatalog::clear()
{
	m_entriesByFolder.clear();
	m_assetRefCache.clear();

	if (OnCatalogChanged)
	{
		OnCatalogChanged();
	}
}

void ProjectAssetCatalog::scanFolder(const ProjectAssetFolderDesc& desc,
									 std::vector<ProjectAssetEntry>& outEntries) const
{
	outEntries.clear();

	// Project entries first, so a project file shadows a bundled one of the same stored path
	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	if (!desc.projectSubfolder.empty() && !projectDir.empty())
	{
		scanDirectory(desc, projectDir / desc.projectSubfolder, projectDir, false, outEntries);
	}

	if (!desc.bundledSubfolder.empty())
	{
		const std::filesystem::path resourceDir= PathUtils::getResourceDirectory();
		scanDirectory(desc, resourceDir / desc.bundledSubfolder, resourceDir, true, outEntries);
	}

	std::sort(outEntries.begin(), outEntries.end(),
			  [](const ProjectAssetEntry& a, const ProjectAssetEntry& b)
			  {
				  if (a.displayName != b.displayName)
					  return a.displayName < b.displayName;
				  return a.storedPath < b.storedPath;
			  });
}

void ProjectAssetCatalog::scanDirectory(const ProjectAssetFolderDesc& desc, const std::filesystem::path& directory,
										const std::filesystem::path& storedRoot, bool bBundled,
										std::vector<ProjectAssetEntry>& outEntries) const
{
	std::error_code ec;
	if (!std::filesystem::is_directory(directory, ec))
	{
		return;
	}

	const std::filesystem::path normalRoot= storedRoot.lexically_normal();

	for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(
			 directory, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (!dirEntry.is_regular_file(ec))
		{
			continue;
		}

		const std::filesystem::path& filePath= dirEntry.path();
		const std::string filename= filePath.filename().string();
		if (filename.empty() || filename[0] == '.')
		{
			continue;
		}

		// A material folder lists its .mat only; the shader sources and graph beside
		// it belong to the material
		std::string className;
		if (desc.bMaterialFolder)
		{
			if (!desc.factories.empty() && desc.factories[0]->matchesFilterPatterns(filePath))
			{
				className= desc.factories[0]->getAssetRefClassName();
			}
		}
		else
		{
			for (const AssetReferenceFactoryPtr& factory : desc.factories)
			{
				if (factory->matchesFilterPatterns(filePath))
				{
					className= factory->getAssetRefClassName();
					break;
				}
			}
		}

		if (className.empty())
		{
			continue;
		}

		// Stored relative to the root the directory came from, so bundled overlay
		// entries store as "font/x.ttf" just like project entries store "textures/x.png"
		std::string storedPath= filePath.lexically_normal().lexically_relative(normalRoot).generic_string();
		if (storedPath.empty())
		{
			continue;
		}

		bool bAlreadyListed= false;
		for (const ProjectAssetEntry& existing : outEntries)
		{
			if (existing.storedPath == storedPath)
			{
				bAlreadyListed= true;
				break;
			}
		}
		if (bAlreadyListed)
		{
			continue;
		}

		ProjectAssetEntry entry;
		entry.folderId= desc.id;
		entry.className= className;
		entry.storedPath= storedPath;
		entry.absolutePath= filePath.lexically_normal();
		entry.displayName= desc.bMaterialFolder ? filePath.stem().string() : filename;
		entry.bBundled= bBundled;
		entry.bReadOnly= bBundled && !m_bBundledEditable;
		outEntries.push_back(entry);
	}
}

const std::vector<ProjectAssetEntry>& ProjectAssetCatalog::getEntries(const std::string& folderId) const
{
	static const std::vector<ProjectAssetEntry> s_empty;

	auto it= m_entriesByFolder.find(folderId);
	return it != m_entriesByFolder.end() ? it->second : s_empty;
}

const ProjectAssetEntry* ProjectAssetCatalog::findEntry(const std::string& folderId,
														const std::string& storedPath) const
{
	const std::string targetKey= makeComparisonKey(storedPath);
	for (const ProjectAssetEntry& entry : getEntries(folderId))
	{
		if (makeComparisonKey(entry.storedPath) == targetKey)
		{
			return &entry;
		}
	}

	return nullptr;
}

const ProjectAssetEntry* ProjectAssetCatalog::findEntryByStoredPath(const std::string& storedPath) const
{
	for (const ProjectAssetFolderDesc& desc : getFolderDescs())
	{
		if (const ProjectAssetEntry* entry= findEntry(desc.id, storedPath))
		{
			return entry;
		}
	}

	return nullptr;
}

const ProjectAssetEntry* ProjectAssetCatalog::findEntryByPayload(const ProjectAssetDragPayload& payload) const
{
	return findEntry(payload.folderId, payload.storedPath);
}

AssetReferencePtr ProjectAssetCatalog::getAssetReference(const ProjectAssetEntry& entry)
{
	const std::string key= entry.key();
	auto it= m_assetRefCache.find(key);
	if (it != m_assetRefCache.end())
	{
		return it->second;
	}

	const ProjectAssetFolderDesc* desc= findFolderDesc(entry.folderId);
	if (desc == nullptr)
	{
		return AssetReferencePtr();
	}

	AssetReferencePtr assetRef;
	for (const AssetReferenceFactoryPtr& factory : desc->factories)
	{
		if (factory->getAssetRefClassName() == entry.className)
		{
			assetRef= factory->allocateAssetReference();
			break;
		}
	}

	if (assetRef)
	{
		assetRef->setAssetPath(entry.storedPath);
		m_assetRefCache[key]= assetRef;
	}

	return assetRef;
}

bool ProjectAssetCatalog::entryMatchesFactory(const ProjectAssetEntry& entry,
											  const AssetReferenceFactory& factory) const
{
	return factory.matchesFilterPatterns(entry.absolutePath);
}

bool ProjectAssetCatalog::importAsset(const std::string& folderId, const std::filesystem::path& sourcePath,
									  std::string& outStoredPath, std::string& outError)
{
	outStoredPath.clear();
	outError.clear();

	const ProjectAssetFolderDesc* desc= findFolderDesc(folderId);
	if (desc == nullptr)
	{
		outError= "Unknown asset folder: " + folderId;
		return false;
	}

	if (desc->bReadOnly)
	{
		outError= locText("assets.readOnlyFolder");
		return false;
	}

	const std::filesystem::path destDir= getFolderDirectory(folderId);
	if (destDir.empty())
	{
		outError= locText("assets.importNoProject");
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(sourcePath, ec))
	{
		outError= "File not found: " + sourcePath.string();
		return false;
	}

	if (!findFactoryForFile(*desc, sourcePath))
	{
		outError= locFormat("assets.importUnsupportedFileFmt", sourcePath.filename().string().c_str());
		return false;
	}

	if (desc->bMaterialFolder)
	{
		return importMaterial(*desc, sourcePath, outStoredPath, outError);
	}

	const std::filesystem::path absSource= std::filesystem::absolute(sourcePath, ec).lexically_normal();
	const std::filesystem::path normalDestDir= destDir.lexically_normal();

	// A file already under the destination folder is the asset, not a copy of it
	const std::filesystem::path relativeToDest= absSource.lexically_relative(normalDestDir);
	if (!relativeToDest.empty() && relativeToDest.begin()->string() != "..")
	{
		outStoredPath= PathUtils::makeStoredProjectPath(absSource);
		return true;
	}

	// A bundled file keeps its path relative to the resources folder, so the copy
	// shadows it by stored path. Anything else lands at the folder's top level.
	std::filesystem::path destPath= normalDestDir / absSource.filename();
	const std::filesystem::path shadowPath= makeProjectShadowPath(absSource);
	if (!shadowPath.empty())
	{
		destPath= shadowPath;
	}
	std::filesystem::create_directories(destPath.parent_path(), ec);

	// Never overwrite: another referrer may depend on the file already there
	const std::string stem= absSource.stem().string();
	const std::string extension= absSource.extension().string();
	const std::filesystem::path destFolder= destPath.parent_path();
	for (int suffix= 2; std::filesystem::exists(destPath, ec); ++suffix)
	{
		destPath= destFolder / (stem + "_" + std::to_string(suffix) + extension);
	}

	if (!std::filesystem::copy_file(absSource, destPath, ec) || ec)
	{
		outError= ec.message();
		return false;
	}

	outStoredPath= PathUtils::makeStoredProjectPath(destPath);
	refresh();

	return true;
}

AssetReferenceFactoryPtr ProjectAssetCatalog::findFactoryForFile(const ProjectAssetFolderDesc& desc,
																 const std::filesystem::path& path)
{
	for (const AssetReferenceFactoryPtr& factory : desc.factories)
	{
		if (factory->matchesFilterPatterns(path))
		{
			return factory;
		}
	}

	return AssetReferenceFactoryPtr();
}

bool ProjectAssetCatalog::resolveUploadDestination(const std::string& folderId, const std::string& fileName,
												   std::filesystem::path& outFolderDir, std::string& outError)
{
	outFolderDir.clear();
	outError.clear();

	const ProjectAssetFolderDesc* desc= findFolderDesc(folderId);
	if (desc == nullptr)
	{
		outError= "Unknown asset folder: " + folderId;
		return false;
	}

	if (desc->bReadOnly)
	{
		outError= "Asset folder is read-only: " + folderId;
		return false;
	}

	if (desc->bMaterialFolder)
	{
		outError= "Asset folder holds materials, which are folders rather than single files: " + folderId;
		return false;
	}

	const std::filesystem::path folderDir= getFolderDirectory(folderId);
	if (folderDir.empty())
	{
		outError= "No project is loaded";
		return false;
	}

	if (!findFactoryForFile(*desc, std::filesystem::path(fileName)))
	{
		outError= "File type is not accepted by asset folder " + folderId + ": " + fileName;
		return false;
	}

	outFolderDir= folderDir;
	return true;
}

bool ProjectAssetCatalog::importUploadedAsset(const std::string& folderId, const std::string& fileName,
											  const std::filesystem::path& sourcePath, std::string& outStoredPath,
											  bool& outReplaced, std::error_code& outErrorCode, std::string& outError)
{
	outStoredPath.clear();
	outReplaced= false;
	outErrorCode.clear();
	outError.clear();

	// Checked again here: the project may have changed since the caller resolved the destination
	std::filesystem::path folderDir;
	if (!resolveUploadDestination(folderId, fileName, folderDir, outError))
	{
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(sourcePath, ec))
	{
		outError= "Uploaded file not found: " + sourcePath.string();
		return false;
	}

	std::filesystem::create_directories(folderDir, ec);
	const std::filesystem::path destPath= folderDir / fileName;
	outReplaced= std::filesystem::exists(destPath, ec);

	// MSVC's rename is MoveFileExW with replace-existing and copy-allowed, so it
	// replaces the file already there and survives a move across volumes
	ec.clear();
	std::filesystem::rename(sourcePath, destPath, ec);
	if (ec)
	{
		outErrorCode= ec;
		outError= ec.message();
		return false;
	}

	outStoredPath= PathUtils::makeStoredProjectPath(destPath);
	refresh();

	return true;
}

bool ProjectAssetCatalog::importMaterial(const ProjectAssetFolderDesc& desc, const std::filesystem::path& sourcePath,
										 std::string& outStoredPath, std::string& outError)
{
	std::error_code ec;
	const std::filesystem::path absSource= std::filesystem::absolute(sourcePath, ec).lexically_normal();
	const std::filesystem::path materialsDir= getFolderDirectory(desc.id).lexically_normal();

	// A material already under the project's folder for its domain is the asset itself
	const std::filesystem::path relativeToDest= absSource.lexically_relative(materialsDir);
	if (!relativeToDest.empty() && relativeToDest.begin()->string() != "..")
	{
		outStoredPath= PathUtils::makeStoredProjectPath(absSource);
		return true;
	}

	// The extension already names the folder's domain; a material whose own
	// contents say otherwise would fail every consumer, so it is refused here
	MikanShaderConfig materialConfig;
	if (!materialConfig.load(absSource))
	{
		outError= locFormat("assets.importFailedFmt", absSource.filename().string().c_str());
		return false;
	}
	const eMaterialDomain recordedDomain= MaterialDomainUtils::resolveMaterialDomain(materialConfig);
	if (recordedDomain != eMaterialDomain::INVALID && recordedDomain != (eMaterialDomain)desc.materialDomain)
	{
		outError= locFormat("assets.importMaterialDomainMismatchFmt", absSource.filename().string().c_str(),
							MaterialDomainUtils::domainToString(recordedDomain).c_str());
		return false;
	}

	const std::string materialName= absSource.stem().string();
	const std::filesystem::path destFolder= materialsDir / materialName;
	if (std::filesystem::exists(destFolder, ec))
	{
		outError= locFormat("assets.importMaterialExistsFmt", materialName.c_str());
		return false;
	}

	// The graph and shader sources beside the .mat are relative to its folder, so
	// the folder moves as a unit
	std::filesystem::create_directories(destFolder.parent_path(), ec);
	std::filesystem::copy(absSource.parent_path(), destFolder, std::filesystem::copy_options::recursive, ec);
	if (ec)
	{
		outError= ec.message();
		std::filesystem::remove_all(destFolder, ec);
		return false;
	}

	outStoredPath= PathUtils::makeStoredProjectPath(destFolder / absSource.filename());
	refresh();

	return true;
}

std::string ProjectAssetCatalog::makeComparisonKey(const std::filesystem::path& path)
{
	std::string key= makeOverlayStoredPath(path);
	std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return (char)std::tolower(c); });

	return key;
}

std::vector<ProjectAssetReferrer> ProjectAssetCatalog::findReferences(const std::string& storedPath) const
{
	std::vector<ProjectAssetReferrer> referrers;
	if (storedPath.empty())
	{
		return referrers;
	}

	const std::string targetKey= makeComparisonKey(storedPath);

	// A material's own graph and shaders name each other, and they leave with the
	// folder, so nothing inside it counts as a referrer
	std::filesystem::path excludeFolder;
	const std::filesystem::path resolvedTarget= PathUtils::resolveProjectResource(storedPath);
	if (!resolvedTarget.empty() && MaterialDomainUtils::isMaterialFileExtension(resolvedTarget.extension().string()))
	{
		excludeFolder= resolvedTarget.parent_path().lexically_normal();
	}

	findGraphReferences(targetKey, excludeFolder, referrers);
	findMaterialReferences(targetKey, excludeFolder, referrers);
	findComponentReferences(targetKey, referrers);

	return referrers;
}

std::vector<std::filesystem::path> ProjectAssetCatalog::getReferrerScanRoots()
{
	std::vector<std::filesystem::path> roots;
	std::error_code ec;

	const std::filesystem::path projectDir= PathUtils::getProjectDirectory();
	if (!projectDir.empty() && std::filesystem::is_directory(projectDir, ec))
	{
		roots.push_back(projectDir);
	}

	const std::filesystem::path resourceDir= PathUtils::getResourceDirectory();
	if (std::filesystem::is_directory(resourceDir, ec))
	{
		roots.push_back(resourceDir);
	}

	return roots;
}

void ProjectAssetCatalog::findGraphReferences(const std::string& targetKey, const std::filesystem::path& excludeFolder,
											  std::vector<ProjectAssetReferrer>& outReferrers) const
{
	std::error_code ec;

	// Every graph file of any kind under the project and the bundled resources,
	// which includes the material graphs under shaders/
	for (const std::filesystem::path& root : getReferrerScanRoots())
	{
		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(
				 root, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			if (!dirEntry.is_regular_file(ec)
				|| !NodeGraphFileTypes::isGraphFileExtension(dirEntry.path().extension().string()))
			{
				continue;
			}

			const std::filesystem::path& graphPath= dirEntry.path();
			if (isUnderFolder(graphPath, excludeFolder))
			{
				continue;
			}

			configuru::Config graphJson;
			try
			{
				graphJson= configuru::parse_file(graphPath.string(), configuru::JSON);
			}
			catch (const std::exception& e)
			{
				MIKAN_LOG_WARNING("ProjectAssetCatalog::findGraphReferences")
					<< "Skipping unreadable graph " << graphPath << ": " << e.what();
				continue;
			}

			bool bReferences= false;

			if (graphJson.has_key("assetReferences") && graphJson["assetReferences"].is_array())
			{
				for (const configuru::Config& assetRef : graphJson["assetReferences"].as_array())
				{
					const std::string assetPath= assetRef.get_or<std::string>("asset_path", "");
					if (!assetPath.empty() && makeComparisonKey(assetPath) == targetKey)
					{
						bReferences= true;
						break;
					}
				}
			}

			// Material graphs carry texture defaults on their parameter nodes rather
			// than in the asset list
			if (!bReferences && graphJson.has_key("nodes") && graphJson["nodes"].is_array())
			{
				for (const configuru::Config& node : graphJson["nodes"].as_array())
				{
					const std::string texturePath= node.get_or<std::string>("default_texture_path", "");
					if (!texturePath.empty() && makeComparisonKey(texturePath) == targetKey)
					{
						bReferences= true;
						break;
					}
				}
			}

			if (bReferences)
			{
				ProjectAssetReferrer referrer;
				referrer.kind= ProjectAssetReferrer::Kind::graph;
				referrer.name= graphPath.stem().string();
				referrer.detail= makeOverlayStoredPath(graphPath);
				outReferrers.push_back(referrer);
			}
		}
	}
}

void ProjectAssetCatalog::findMaterialReferences(const std::string& targetKey,
												 const std::filesystem::path& excludeFolder,
												 std::vector<ProjectAssetReferrer>& outReferrers) const
{
	std::error_code ec;

	for (const std::filesystem::path& root : getReferrerScanRoots())
	{
		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(
				 root, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			if (!dirEntry.is_regular_file(ec)
				|| !MaterialDomainUtils::isMaterialFileExtension(dirEntry.path().extension().string()))
			{
				continue;
			}

			const std::filesystem::path& materialPath= dirEntry.path();
			if (isUnderFolder(materialPath, excludeFolder))
			{
				continue;
			}

			MikanShaderConfig materialConfig;
			if (!materialConfig.load(materialPath))
			{
				continue;
			}

			bool bReferences= false;

			// Texture defaults are in stored project form
			for (const auto& textureDefault : materialConfig.uniformTextureDefaults)
			{
				if (!textureDefault.second.empty() && makeComparisonKey(textureDefault.second) == targetKey)
				{
					bReferences= true;
					break;
				}
			}

			// The graph and shader sources are relative to the .mat folder
			if (!bReferences)
			{
				const std::filesystem::path materialFolder= materialPath.parent_path();
				const std::filesystem::path folderRelativePaths[]= {
					materialConfig.sourceGraphPath, materialConfig.vertexShaderPath, materialConfig.fragmentShaderPath};
				for (const std::filesystem::path& relativePath : folderRelativePaths)
				{
					if (relativePath.empty())
						continue;

					const std::filesystem::path resolved=
						relativePath.is_absolute() ? relativePath : (materialFolder / relativePath).lexically_normal();
					if (makeComparisonKey(resolved) == targetKey)
					{
						bReferences= true;
						break;
					}
				}
			}

			if (bReferences)
			{
				ProjectAssetReferrer referrer;
				referrer.kind= ProjectAssetReferrer::Kind::material;
				referrer.name= materialPath.stem().string();
				referrer.detail= makeOverlayStoredPath(materialPath);
				outReferrers.push_back(referrer);
			}
		}
	}
}

void ProjectAssetCatalog::findComponentReferences(const std::string& targetKey,
												  std::vector<ProjectAssetReferrer>& outReferrers) const
{
	if (!m_projectManager)
	{
		return;
	}

	MikanPropertyDatabaseConstPtr propertyDatabase= m_projectManager->getPropertyDatabaseConst();
	if (!propertyDatabase)
	{
		return;
	}

	// Any string property tagged with an asset factory is an asset path, so a new
	// component type joins the scan by tagging its descriptor
	for (const MikanPropertyEntry& propertyEntry : propertyDatabase->getAllProperties())
	{
		if (propertyEntry.componentClassName.empty() || !propertyEntry.descriptor)
		{
			continue;
		}
		if (propertyEntry.descriptor->getMetaDataOfType<AssetReferenceFactoryMetaData>() == nullptr)
		{
			continue;
		}

		MikanObjectSystemPtr system= m_projectManager->getSystemByName(propertyEntry.systemName);
		if (!system)
		{
			continue;
		}

		std::vector<MikanComponentPtr> components;
		if (!system->getComponentList(propertyEntry.componentClassName, components))
		{
			continue;
		}

		const std::string& propertyName= propertyEntry.descriptor->getName();
		for (const MikanComponentPtr& component : components)
		{
			MikanVariant value;
			if (!component->getPropertyValue(propertyName, value) || value.value_type != MikanVariantType::STRING)
			{
				continue;
			}

			const char* assetPath= value.getUtf8Value();
			if (assetPath == nullptr || assetPath[0] == '\0')
			{
				continue;
			}

			if (makeComparisonKey(PathUtils::utf8ToPath(assetPath)) == targetKey)
			{
				ProjectAssetReferrer referrer;
				referrer.kind= ProjectAssetReferrer::Kind::component;
				referrer.name= component->getName();
				referrer.detail= propertyEntry.componentClassName;
				outReferrers.push_back(referrer);
			}
		}
	}
}

bool ProjectAssetCatalog::deleteAsset(const ProjectAssetEntry& inEntry, std::string& outError)
{
	// The caller's reference usually points into the entry list the refresh below rebuilds
	const ProjectAssetEntry entry= inEntry;
	outError.clear();

	if (entry.bReadOnly)
	{
		outError= locText("assets.readOnlyAsset");
		return false;
	}

	const ProjectAssetFolderDesc* desc= findFolderDesc(entry.folderId);
	if (desc == nullptr)
	{
		outError= "Unknown asset folder: " + entry.folderId;
		return false;
	}

	std::error_code ec;
	if (desc->bMaterialFolder)
	{
		std::filesystem::remove_all(entry.absolutePath.parent_path(), ec);
	}
	else
	{
		std::filesystem::remove(entry.absolutePath, ec);
	}

	if (ec)
	{
		outError= ec.message();
		return false;
	}

	refresh();

	return true;
}
