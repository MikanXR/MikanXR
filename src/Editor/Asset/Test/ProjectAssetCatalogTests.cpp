#include "ProjectAssetCatalogTests.h"
#include "unit_test.h"

#include "AssetReference.h"
#include "AssetUploadRequestHandler.h"
#include "CompositorMaterialAssetReference.h"
#include "PathUtils.h"
#include "ProjectAssetCatalog.h"
#include "TextureAssetReference.h"

#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <string>

namespace
{
// A throwaway project folder with one asset of each kind, plus an "external"
// folder outside the project to import from. The project directory is restored
// on destruction so later test modules see what they expect.
struct CatalogTestProject
{
	std::filesystem::path root;
	std::filesystem::path projectDir;
	std::filesystem::path externalDir;
	std::filesystem::path savedProjectDir;

	CatalogTestProject()
	{
		savedProjectDir= PathUtils::getProjectDirectory();

		root= std::filesystem::temp_directory_path() / "mikan_project_asset_catalog_tests";
		std::filesystem::remove_all(root);
		projectDir= root / "project";
		externalDir= root / "external";

		for (const char* folder : {"compositors", "models", "movies", "scripts", "compositor_materials/m", "textures"})
		{
			std::filesystem::create_directories(projectDir / folder);
		}
		std::filesystem::create_directories(externalDir / "n");
		std::filesystem::create_directories(externalDir / "wrongdomain");

		writeFile(projectDir / "textures" / "grid.png", "png");
		writeFile(projectDir / "textures" / "other.png", "png");
		// Shadows the bundled texture of the same stored path
		writeFile(projectDir / "textures" / "whiteRGB.png", "png");
		writeFile(projectDir / "textures" / "notes.txt", "text");
		writeFile(projectDir / "scripts" / "a.lua", "return {}");
		writeFile(projectDir / "models" / "cube.obj", "o cube");

		// A compositor graph that references the material
		writeFile(projectDir / "compositors" / "comp.compgraph",
				  R"({
	"class_name": "CompositorNodeGraph",
	"assetReferences": [
		{ "class_name": "CompositorMaterialAssetReference", "asset_path": "compositor_materials/m/m.compmat" }
	],
	"nodes": []
})");

		// A material whose file defaults a sampler to grid.png and whose graph
		// defaults a texture parameter to other.png
		writeFile(projectDir / "compositor_materials" / "m" / "m.compmat",
				  R"({
	"materialName": "m",
	"vertexShaderPath": "m.vert",
	"fragmentShaderPath": "m.frag",
	"vertexAttributes": [],
	"uniformSemanticMap": {},
	"domain": "compositor",
	"sourceGraphPath": "m.matgraph",
	"uniformDefaults": { "tex": "textures/grid.png" }
})");
		writeFile(projectDir / "compositor_materials" / "m" / "m.matgraph",
				  R"({
	"class_name": "MaterialNodeGraph",
	"assetReferences": [],
	"nodes": [
		{ "class_name": "ShaderTextureParameterNode", "default_texture_path": "textures/other.png" }
	]
})");
		writeFile(projectDir / "compositor_materials" / "m" / "m.vert", "");
		writeFile(projectDir / "compositor_materials" / "m" / "m.frag", "");

		// External imports: a texture, a material with a domain, and one without
		writeFile(externalDir / "extra.png", "png");
		writeFile(externalDir / "extra.txt", "text");
		writeFile(
			externalDir / "n" / "n.compmat",
			R"({ "materialName": "n", "vertexAttributes": [], "uniformSemanticMap": {}, "domain": "compositor" })");
		writeFile(externalDir / "n" / "n.matgraph", "{}");
		// A file carrying the compositor extension whose contents say shape
		writeFile(
			externalDir / "wrongdomain" / "wrongdomain.compmat",
			R"({ "materialName": "wrongdomain", "vertexAttributes": [], "uniformSemanticMap": {}, "domain": "shape" })");

		PathUtils::setProjectDirectory(projectDir);
	}

	~CatalogTestProject()
	{
		PathUtils::setProjectDirectory(savedProjectDir);
		std::error_code ec;
		std::filesystem::remove_all(root, ec);
	}

	static void writeFile(const std::filesystem::path& path, const std::string& content)
	{
		std::ofstream file(path);
		file << content;
	}
};

bool hasEntry(const ProjectAssetCatalog& catalog, const char* folderId, const char* storedPath,
			  const char* className= nullptr)
{
	const ProjectAssetEntry* entry= catalog.findEntry(folderId, storedPath);
	if (entry == nullptr)
		return false;

	return className == nullptr || entry->className == className;
}

int countProjectEntries(const ProjectAssetCatalog& catalog, const char* folderId)
{
	int count= 0;
	for (const ProjectAssetEntry& entry : catalog.getEntries(folderId))
	{
		if (!entry.bBundled)
			count++;
	}

	return count;
}

bool hasReferrer(const std::vector<ProjectAssetReferrer>& referrers, ProjectAssetReferrer::Kind kind, const char* name)
{
	for (const ProjectAssetReferrer& referrer : referrers)
	{
		if (referrer.kind == kind && referrer.name == name)
			return true;
	}

	return false;
}
} // namespace

bool project_asset_catalog_test_scan_lists_each_folder()
{
	UNIT_TEST_BEGIN("scan lists each folder")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	success&= hasEntry(catalog, "compositors", "compositors/comp.compgraph", "CompositorGraphAssetReference");
	success&= hasEntry(catalog, "models", "models/cube.obj", "ModelAssetReference");
	success&= hasEntry(catalog, "scripts", "scripts/a.lua", "ScriptAssetReference");
	success&= hasEntry(catalog, "textures", "textures/grid.png", "TextureAssetReference");
	success&= hasEntry(catalog, "textures", "textures/other.png");
	// Unsupported files are not listed
	success&= !hasEntry(catalog, "textures", "textures/notes.txt");

	// One tile per material file, named after the material, with its sources hidden
	success&= countProjectEntries(catalog, "compositor_materials") == 1;
	const ProjectAssetEntry* material= catalog.findEntry("compositor_materials", "compositor_materials/m/m.compmat");
	success&= material != nullptr && material->displayName == "m" && !material->bReadOnly
			  && material->className == "CompositorMaterialAssetReference";
	// The material's own graph is not a compositors-folder entry
	success&= countProjectEntries(catalog, "compositors") == 1;

	// The bundled fonts show through as read-only overlay entries in stored form
	const std::vector<ProjectAssetEntry>& fonts= catalog.getEntries("fonts");
	success&= !fonts.empty();
	for (const ProjectAssetEntry& font : fonts)
	{
		success&= font.bReadOnly && font.storedPath.rfind("font/", 0) == 0 && font.className == "FontAssetReference";
	}
	success&= hasEntry(catalog, "fonts", "font/MochiyPopOne-Regular.ttf");

	// A texture is also a valid pixel content asset, since both types share the folder
	const ProjectAssetEntry* grid= catalog.findEntry("textures", "textures/grid.png");
	TextureAssetReferenceFactory textureFactory;
	CompositorMaterialAssetReferenceFactory materialFactory;
	success&= grid != nullptr && catalog.entryMatchesFactory(*grid, textureFactory)
			  && !catalog.entryMatchesFactory(*grid, materialFactory);

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_import_copies_into_folder()
{
	UNIT_TEST_BEGIN("import copies into folder")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	std::string storedPath;
	std::string error;

	success&= catalog.importAsset("textures", project.externalDir / "extra.png", storedPath, error);
	success&= storedPath == "textures/extra.png";
	success&= std::filesystem::exists(project.projectDir / "textures" / "extra.png");
	success&= hasEntry(catalog, "textures", "textures/extra.png");

	// The same name again gets a suffix rather than overwriting
	success&= catalog.importAsset("textures", project.externalDir / "extra.png", storedPath, error);
	success&= storedPath == "textures/extra_2.png";
	success&= std::filesystem::exists(project.projectDir / "textures" / "extra_2.png");

	// A file already inside the folder is returned as is
	success&= catalog.importAsset("textures", project.projectDir / "textures" / "grid.png", storedPath, error);
	success&= storedPath == "textures/grid.png";
	success&= !std::filesystem::exists(project.projectDir / "textures" / "grid_2.png");

	// Unsupported types and read-only folders are refused
	success&= !catalog.importAsset("textures", project.externalDir / "extra.txt", storedPath, error);
	success&= !catalog.importAsset("fonts", project.externalDir / "extra.png", storedPath, error);

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_upload_replaces_in_place()
{
	UNIT_TEST_BEGIN("upload moves into the folder and replaces a file of the same name")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	std::string storedPath;
	std::string error;
	std::error_code errorCode;
	bool bReplaced= true;

	// The destination resolves for a movie, its sidecar, and a photo's sidecar in textures
	std::filesystem::path folderDir;
	success&= catalog.resolveUploadDestination("movies", "take.mp4", folderDir, error);
	success&= (folderDir == project.projectDir / "movies");
	success&= catalog.resolveUploadDestination("movies", "take.pose.json", folderDir, error);
	success&= catalog.resolveUploadDestination("textures", "photo.pose.json", folderDir, error);
	// Not for a type the folder does not take, a material folder, or a bundled-only folder
	success&= !catalog.resolveUploadDestination("movies", "notes.txt", folderDir, error) && !error.empty();
	success&= !catalog.resolveUploadDestination("compositor_materials", "m.compmat", folderDir, error);
	success&= !catalog.resolveUploadDestination("fonts", "a.ttf", folderDir, error);
	success&= !catalog.resolveUploadDestination("nope", "take.mp4", folderDir, error);

	// A new upload is moved into place, listed, and not a replacement
	const std::filesystem::path temp1= project.projectDir / "movies" / ".mikan_upload_1";
	CatalogTestProject::writeFile(temp1, "first");
	success&= catalog.importUploadedAsset("movies", "take.mp4", temp1, storedPath, bReplaced, errorCode, error);
	success&= (storedPath == "movies/take.mp4");
	success&= !bReplaced;
	success&= !std::filesystem::exists(temp1);
	success&= hasEntry(catalog, "movies", "movies/take.mp4", "MovieAssetReference");

	// The same name again replaces the file, with no numeric suffix
	const std::filesystem::path temp2= project.projectDir / "movies" / ".mikan_upload_2";
	CatalogTestProject::writeFile(temp2, "second");
	success&= catalog.importUploadedAsset("movies", "take.mp4", temp2, storedPath, bReplaced, errorCode, error);
	success&= (storedPath == "movies/take.mp4");
	success&= bReplaced;
	success&= !std::filesystem::exists(project.projectDir / "movies" / "take_2.mp4");
	{
		std::ifstream replaced(project.projectDir / "movies" / "take.mp4");
		std::string content;
		std::getline(replaced, content);
		success&= (content == "second");
	}

	// A missing temp file and a refused type both fail without touching the folder
	success&= !catalog.importUploadedAsset("movies", "gone.mp4", project.projectDir / "movies" / ".missing", storedPath,
										   bReplaced, errorCode, error);
	CatalogTestProject::writeFile(temp1, "text");
	success&= !catalog.importUploadedAsset("movies", "notes.txt", temp1, storedPath, bReplaced, errorCode, error);
	success&= !std::filesystem::exists(project.projectDir / "movies" / "notes.txt");

	// The upload name guard
	success&= AssetUploadRequestHandler::isSafeUploadFileName("take_20260915_161634.mp4");
	success&= AssetUploadRequestHandler::isSafeUploadFileName("take.pose.json");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName("");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName("../take.mp4");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName("a/b.mp4");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName("a\\b.mp4");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName("c:take.mp4");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName(".hidden.mp4");
	success&= !AssetUploadRequestHandler::isSafeUploadFileName(std::string(256, 'a'));

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_material_import_copies_folder()
{
	UNIT_TEST_BEGIN("material import copies folder")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	std::string storedPath;
	std::string error;

	success&= catalog.importAsset("compositor_materials", project.externalDir / "n" / "n.compmat", storedPath, error);
	success&= storedPath == "compositor_materials/n/n.compmat";
	success&= std::filesystem::exists(project.projectDir / "compositor_materials" / "n" / "n.compmat");
	success&= std::filesystem::exists(project.projectDir / "compositor_materials" / "n" / "n.matgraph");
	success&= hasEntry(catalog, "compositor_materials", "compositor_materials/n/n.compmat");

	// A second import of the same material is refused, since the folder is its identity
	success&= !catalog.importAsset("compositor_materials", project.externalDir / "n" / "n.compmat", storedPath, error);
	success&= !error.empty();

	// A material whose contents name the other domain is refused
	success&= !catalog.importAsset("compositor_materials", project.externalDir / "wrongdomain" / "wrongdomain.compmat",
								   storedPath, error);
	success&= !std::filesystem::exists(project.projectDir / "compositor_materials" / "wrongdomain");

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_find_references()
{
	UNIT_TEST_BEGIN("find references")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	// The material file defaults a sampler to grid.png
	std::vector<ProjectAssetReferrer> gridReferrers= catalog.findReferences("textures/grid.png");
	success&= gridReferrers.size() == 1 && hasReferrer(gridReferrers, ProjectAssetReferrer::Kind::material, "m");

	// The material graph's texture parameter node defaults to other.png
	std::vector<ProjectAssetReferrer> otherReferrers= catalog.findReferences("textures/other.png");
	success&= otherReferrers.size() == 1 && hasReferrer(otherReferrers, ProjectAssetReferrer::Kind::graph, "m");

	// The compositor graph references the material; the material's own graph does not count
	std::vector<ProjectAssetReferrer> materialReferrers= catalog.findReferences("compositor_materials/m/m.compmat");
	success&=
		materialReferrers.size() == 1 && hasReferrer(materialReferrers, ProjectAssetReferrer::Kind::graph, "comp");

	// An absolute path under the project compares equal to its stored form
	const std::string absoluteGrid= (project.projectDir / "textures" / "grid.png").string();
	success&= catalog.findReferences(absoluteGrid).size() == 1;

	success&= catalog.findReferences("scripts/a.lua").empty();

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_bundled_overlay()
{
	UNIT_TEST_BEGIN("bundled resources overlay every folder")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	const std::filesystem::path resourceDir= PathUtils::getResourceDirectory();

	// Bundled files show through as read-only entries in stored form
	// A bundled graph every install ships; nothing in the test depends on its contents
	const ProjectAssetEntry* bundledGraph= catalog.findEntry("compositors", "compositors/video_only.compgraph");
	success&= bundledGraph != nullptr && bundledGraph->bBundled && bundledGraph->bReadOnly;
	if (bundledGraph == nullptr)
	{
		fprintf(stdout, "    FAILED: bundled compositors/video_only.compgraph not listed (run from the repo root)\n");
		UNIT_TEST_COMPLETE()
	}
	const ProjectAssetEntry* bundledMaterial= catalog.findEntry(
		"compositor_materials", "compositor_materials/rgbUndistortionFrame/rgbUndistortionFrame.compmat");
	success&= bundledMaterial != nullptr && bundledMaterial->bBundled && bundledMaterial->bReadOnly
			  && bundledMaterial->displayName == "rgbUndistortionFrame";
	const ProjectAssetEntry* bundledModel= catalog.findEntry("models", "models/shapes/sphere.obj");
	success&= bundledModel != nullptr && bundledModel->bBundled;
	success&= catalog.findEntry("scripts", "scripts/easing.lua") != nullptr;

	// A project file with the same stored path hides the bundled one
	int whiteCount= 0;
	for (const ProjectAssetEntry& entry : catalog.getEntries("textures"))
	{
		if (entry.storedPath == "textures/whiteRGB.png")
		{
			whiteCount++;
			success&= !entry.bBundled && !entry.bReadOnly;
		}
	}
	success&= whiteCount == 1;

	// Path helpers tell bundled from project and name the shadowing project path
	success&= ProjectAssetCatalog::isBundledPath("textures/blackRGB.png");
	success&= !ProjectAssetCatalog::isBundledPath("textures/grid.png");
	success&= !ProjectAssetCatalog::isBundledPath("textures/whiteRGB.png");
	success&= catalog.isReadOnlyPath("textures/blackRGB.png") && !catalog.isReadOnlyPath("textures/grid.png");
	success&= ProjectAssetCatalog::makeProjectShadowPath(resourceDir / "textures" / "blackRGB.png")
			  == (project.projectDir / "textures" / "blackRGB.png").lexically_normal();
	success&= ProjectAssetCatalog::makeProjectShadowPath(project.projectDir / "textures" / "grid.png").empty();
	success&= ProjectAssetCatalog::makeOverlayStoredPath(resourceDir / "compositors" / "video_only.compgraph")
			  == "compositors/video_only.compgraph";

	// Bundled entries refuse deletion
	std::string error;
	success&= !catalog.deleteAsset(*bundledGraph, error) && !error.empty();
	success&= std::filesystem::exists(resourceDir / "compositors" / "video_only.compgraph");

	// Copying a bundled file into the project keeps its relative path, so the copy shadows it
	std::string storedPath;
	success&= catalog.importAsset("models", resourceDir / "models" / "shapes" / "sphere.obj", storedPath, error);
	success&= storedPath == "models/shapes/sphere.obj";
	success&= std::filesystem::exists(project.projectDir / "models" / "shapes" / "sphere.obj");
	const ProjectAssetEntry* projectModel= catalog.findEntry("models", "models/shapes/sphere.obj");
	success&= projectModel != nullptr && !projectModel->bBundled;

	// Bundled referrers count: the bundled video_only graph names this bundled material
	const std::vector<ProjectAssetReferrer> referrers=
		catalog.findReferences("compositor_materials/rgbFrame/rgbFrame.compmat");
	success&= hasReferrer(referrers, ProjectAssetReferrer::Kind::graph, "video_only");
	for (const ProjectAssetReferrer& referrer : referrers)
	{
		if (referrer.name == "video_only")
			success&= referrer.detail == "compositors/video_only.compgraph";
	}

	// The developer switch makes bundled entries writable
	catalog.setBundledResourcesEditable(true);
	const ProjectAssetEntry* editableGraph= catalog.findEntry("compositors", "compositors/video_only.compgraph");
	success&= editableGraph != nullptr && editableGraph->bBundled && !editableGraph->bReadOnly;
	success&= !catalog.isReadOnlyPath("textures/blackRGB.png");
	catalog.setBundledResourcesEditable(false);
	success&= catalog.isReadOnlyPath("textures/blackRGB.png");

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_delete_removes_files()
{
	UNIT_TEST_BEGIN("delete removes files")

	CatalogTestProject project;
	ProjectAssetCatalog catalog;
	catalog.refresh();

	std::string error;

	const ProjectAssetEntry* script= catalog.findEntry("scripts", "scripts/a.lua");
	success&= script != nullptr && catalog.deleteAsset(*script, error);
	success&= !std::filesystem::exists(project.projectDir / "scripts" / "a.lua");
	success&= !hasEntry(catalog, "scripts", "scripts/a.lua");

	// A material delete takes the whole folder
	const ProjectAssetEntry* material= catalog.findEntry("compositor_materials", "compositor_materials/m/m.compmat");
	success&= material != nullptr && catalog.deleteAsset(*material, error);
	success&= !std::filesystem::exists(project.projectDir / "compositor_materials" / "m");
	success&= countProjectEntries(catalog, "compositor_materials") == 0;

	// Bundled entries are refused
	const std::vector<ProjectAssetEntry>& fonts= catalog.getEntries("fonts");
	success&= !fonts.empty() && !catalog.deleteAsset(fonts[0], error) && !error.empty();
	success&= !fonts.empty() && std::filesystem::exists(fonts[0].absolutePath);

	UNIT_TEST_COMPLETE()
}

bool project_asset_catalog_test_asset_reference_stores_project_relative()
{
	UNIT_TEST_BEGIN("asset reference stores project relative")

	CatalogTestProject project;

	AssetReference assetRef;

	// An absolute path under the project stores relative with forward slashes
	assetRef.setAssetPath(project.projectDir / "textures" / "grid.png");
	success&= assetRef.getInternalAssetPath() == std::filesystem::path("textures/grid.png");
	success&= assetRef.getResolvedAssetPath() == (project.projectDir / "textures" / "grid.png");

	// A stored path stays as it is
	assetRef.setAssetPath("font/MochiyPopOne-Regular.ttf");
	success&= assetRef.getInternalAssetPath() == std::filesystem::path("font/MochiyPopOne-Regular.ttf");
	success&= !assetRef.getResolvedAssetPath().empty();

	// An absolute path outside the project stays absolute
	assetRef.setAssetPath(project.externalDir / "extra.png");
	success&= assetRef.getInternalAssetPath().is_absolute();

	// The config carries the stored form
	assetRef.setAssetPath(project.projectDir / "textures" / "grid.png");
	AssetReferenceConfigPtr config= std::make_shared<AssetReferenceConfig>();
	assetRef.saveToConfig(config);
	success&= config->assetPath == "textures/grid.png";

	AssetReference loadedRef;
	success&= loadedRef.loadFromConfig(config);
	success&= loadedRef.getInternalAssetPath() == assetRef.getInternalAssetPath();

	UNIT_TEST_COMPLETE()
}

bool run_project_asset_catalog_tests()
{
	UNIT_TEST_MODULE_BEGIN("project_asset_catalog")
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_scan_lists_each_folder);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_import_copies_into_folder);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_upload_replaces_in_place);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_material_import_copies_folder);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_find_references);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_bundled_overlay);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_delete_removes_files);
	UNIT_TEST_MODULE_CALL_TEST(project_asset_catalog_test_asset_reference_stores_project_relative);
	UNIT_TEST_MODULE_END()
}
