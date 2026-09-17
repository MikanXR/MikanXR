#include "ModelRepositoryTests.h"
#include "unit_test.h"

#include "ModelCatalog.h"
#include "ModelRepository.h"

#include <assert.h>
#include <filesystem>
#include <fstream>
#include <stdlib.h>
#include <string>

namespace
{
// A throwaway folder under the OS temp directory, removed on the way out
class ScopedModelTempDir
{
public:
	ScopedModelTempDir(const std::string& name)
	{
		m_dir= std::filesystem::temp_directory_path() / name;
		std::filesystem::remove_all(m_dir);
		std::filesystem::create_directories(m_dir);
	}

	~ScopedModelTempDir()
	{
		std::error_code ec;
		std::filesystem::remove_all(m_dir, ec);
	}

	const std::filesystem::path& path() const { return m_dir; }

private:
	std::filesystem::path m_dir;
};

// Points the per-user model root at a temp folder for the duration of a test.
// The repository reads the environment on every call (PathUtils does), which
// is what makes the real search order testable rather than only a stubbed one.
class ScopedUserModelRoot
{
public:
	ScopedUserModelRoot(const std::filesystem::path& localAppData)
	{
		const char* existing= getenv(k_variable);
		m_bHadPrevious= existing != nullptr;
		if (m_bHadPrevious)
			m_previous= existing;

		_putenv_s(k_variable, localAppData.string().c_str());
	}

	~ScopedUserModelRoot() { _putenv_s(k_variable, m_bHadPrevious ? m_previous.c_str() : ""); }

private:
	static constexpr const char* k_variable= "LOCALAPPDATA";
	std::string m_previous;
	bool m_bHadPrevious= false;
};

// Restores the process working directory, which the repository searches.
class ScopedWorkingDirectory
{
public:
	ScopedWorkingDirectory(const std::filesystem::path& directory)
	{
		m_previous= std::filesystem::current_path();
		std::filesystem::current_path(directory);
	}

	~ScopedWorkingDirectory()
	{
		std::error_code ec;
		std::filesystem::current_path(m_previous, ec);
	}

private:
	std::filesystem::path m_previous;
};

/// Writes every file the catalog requires, or all but one of them.
void installFakeModel(eModelId id, const std::filesystem::path& directory, const std::string& skipFile= std::string())
{
	const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
	std::filesystem::create_directories(directory);

	for (const std::string& fileName : entry->requiredFiles)
	{
		if (fileName == skipFile)
			continue;

		std::ofstream file(directory / fileName, std::ios::binary);
		file << "not a real model";
	}
}
} // namespace

static bool model_repository_test_catalog_is_complete()
{
	UNIT_TEST_BEGIN("every catalog entry names files, a license and exactly one source")

	success= !ModelCatalog::getEntries().empty();
	assert(success);

	for (const ModelCatalogEntry& entry : ModelCatalog::getEntries())
	{
		success= success && !entry.name.empty();
		success= success && !entry.requiredFiles.empty();
		success= success && !entry.licenseName.empty();
		// Exactly one source kind, or the download task has nothing to follow.
		success= success && (entry.hasManifest() != !entry.directFiles.empty());
		assert(success);

		// The lookup helpers have to agree with the table.
		success= success && ModelCatalog::findEntryByName(entry.name) == &entry;
		success= success && ModelCatalog::findEntry(entry.id) == &entry;
		assert(success);
	}

	success= success && ModelCatalog::findEntryByName("no-such-model") == nullptr;
	assert(success);

	UNIT_TEST_COMPLETE()
}

static bool model_repository_test_working_directory_wins()
{
	UNIT_TEST_BEGIN("a model in the working directory beats a downloaded copy")

	ScopedModelTempDir root("mikan_model_repository_tests");
	const std::filesystem::path workingDir= root.path() / "working";
	const std::filesystem::path localAppData= root.path() / "localappdata";
	std::filesystem::create_directories(workingDir);

	ScopedUserModelRoot userRoot(localAppData);
	ScopedWorkingDirectory cwd(workingDir);

	// Nothing installed anywhere yet.
	success= !ModelRepository::isModelInstalled(eModelId::moge2);
	assert(success);
	success= success && ModelRepository::findInstalledDirectory(eModelId::moge2).empty();
	assert(success);

	// A downloaded copy is found.
	const std::filesystem::path userModelDir= ModelRepository::getDownloadDirectory(eModelId::moge2);
	installFakeModel(eModelId::moge2, userModelDir);
	success= success && ModelRepository::isModelInstalled(eModelId::moge2);
	success= success && ModelRepository::findInstalledDirectory(eModelId::moge2) == userModelDir;
	assert(success);

	// A developer checkout's own copy takes precedence over it.
	const std::filesystem::path workingModelDir= workingDir / "models" / "moge2";
	installFakeModel(eModelId::moge2, workingModelDir);
	success= success && ModelRepository::findInstalledDirectory(eModelId::moge2) == workingModelDir;
	assert(success);

	UNIT_TEST_COMPLETE()
}

static bool model_repository_test_incomplete_is_missing()
{
	UNIT_TEST_BEGIN("a directory missing one required file is not installed, and says which")

	ScopedModelTempDir root("mikan_model_repository_tests_partial");
	const std::filesystem::path workingDir= root.path() / "working";
	const std::filesystem::path localAppData= root.path() / "localappdata";
	std::filesystem::create_directories(workingDir);

	ScopedUserModelRoot userRoot(localAppData);
	ScopedWorkingDirectory cwd(workingDir);

	// Marigold arrives as several files; dropping the one a graph names by
	// itself is exactly the half-installed case worth catching.
	const std::string skipped= "unet_iid_lighting.onnx.data";
	const std::filesystem::path modelDir= ModelRepository::getDownloadDirectory(eModelId::marigold);
	installFakeModel(eModelId::marigold, modelDir, skipped);

	success= !ModelRepository::isModelInstalled(eModelId::marigold);
	assert(success);

	const std::vector<std::string> missing= ModelRepository::getMissingFiles(eModelId::marigold, modelDir);
	success= success && missing.size() == 1;
	success= success && !missing.empty() && missing[0] == skipped;
	assert(success);

	// Completing it flips both answers.
	installFakeModel(eModelId::marigold, modelDir);
	success= success && ModelRepository::isModelInstalled(eModelId::marigold);
	success= success && ModelRepository::getMissingFiles(eModelId::marigold, modelDir).empty();
	assert(success);

	UNIT_TEST_COMPLETE()
}

static bool model_repository_test_override_is_taken_literally()
{
	UNIT_TEST_BEGIN("an override directory is used as given, not searched past")

	ScopedModelTempDir root("mikan_model_repository_tests_override");
	const std::filesystem::path workingDir= root.path() / "working";
	const std::filesystem::path localAppData= root.path() / "localappdata";
	const std::filesystem::path overrideDir= root.path() / "elsewhere";
	std::filesystem::create_directories(workingDir);

	ScopedUserModelRoot userRoot(localAppData);
	ScopedWorkingDirectory cwd(workingDir);

	// A perfectly good copy in the working directory must not rescue a bad
	// override: an explicit path is a statement of intent, and silently using
	// a different model than the one asked for is worse than failing.
	installFakeModel(eModelId::moge2, workingDir / "models" / "moge2");
	success= !ModelRepository::isModelInstalled(eModelId::moge2, overrideDir.string());
	success= success && ModelRepository::resolveDirectory(eModelId::moge2, overrideDir.string()) == overrideDir;
	assert(success);

	installFakeModel(eModelId::moge2, overrideDir);
	success= success && ModelRepository::isModelInstalled(eModelId::moge2, overrideDir.string());
	assert(success);

	// With no override, an uninstalled model still resolves to somewhere, so
	// a caller's error message can name a concrete path.
	success= success
			 && ModelRepository::resolveDirectory(eModelId::marigold)
					== ModelRepository::getDownloadDirectory(eModelId::marigold);
	assert(success);

	UNIT_TEST_COMPLETE()
}

//-- public interface -----
bool run_model_repository_tests()
{
	UNIT_TEST_MODULE_BEGIN("model_repository")
	UNIT_TEST_MODULE_CALL_TEST(model_repository_test_catalog_is_complete);
	UNIT_TEST_MODULE_CALL_TEST(model_repository_test_working_directory_wins);
	UNIT_TEST_MODULE_CALL_TEST(model_repository_test_incomplete_is_missing);
	UNIT_TEST_MODULE_CALL_TEST(model_repository_test_override_is_taken_literally);
	UNIT_TEST_MODULE_END()
}
