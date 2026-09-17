#include "ModelRepository.h"
#include "PathUtils.h"

namespace
{
bool directoryHasEveryFile(const ModelCatalogEntry& entry, const std::filesystem::path& directory)
{
	if (directory.empty())
		return false;

	for (const std::string& fileName : entry.requiredFiles)
	{
		std::error_code ec;
		if (!std::filesystem::exists(directory / fileName, ec))
			return false;
	}

	return true;
}
} // namespace

namespace ModelRepository
{
std::filesystem::path getUserModelsRoot() { return PathUtils::getLocalDataDirectory() / "MikanXR" / "models"; }

std::filesystem::path getDownloadDirectory(eModelId id)
{
	const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
	if (entry == nullptr)
		return std::filesystem::path();

	return getUserModelsRoot() / entry->name;
}

std::filesystem::path findInstalledDirectory(eModelId id)
{
	const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
	if (entry == nullptr)
		return std::filesystem::path();

	// The working directory first: a developer checkout with a hand-built
	// model there is expected to win over a previously downloaded copy.
	std::error_code ec;
	const std::filesystem::path workingDirectory= std::filesystem::current_path(ec) / "models" / entry->name;
	if (!ec && directoryHasEveryFile(*entry, workingDirectory))
		return workingDirectory;

	const std::filesystem::path userDirectory= getUserModelsRoot() / entry->name;
	if (directoryHasEveryFile(*entry, userDirectory))
		return userDirectory;

	return std::filesystem::path();
}

std::filesystem::path resolveDirectory(eModelId id, const std::string& overrideDirectory)
{
	if (!overrideDirectory.empty())
		return std::filesystem::path(overrideDirectory);

	const std::filesystem::path installed= findInstalledDirectory(id);
	if (!installed.empty())
		return installed;

	return getDownloadDirectory(id);
}

bool isModelInstalled(eModelId id, const std::string& overrideDirectory)
{
	const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
	if (entry == nullptr)
		return false;

	if (!overrideDirectory.empty())
		return directoryHasEveryFile(*entry, std::filesystem::path(overrideDirectory));

	return !findInstalledDirectory(id).empty();
}

std::vector<std::string> getMissingFiles(eModelId id, const std::filesystem::path& directory)
{
	std::vector<std::string> missing;

	const ModelCatalogEntry* entry= ModelCatalog::findEntry(id);
	if (entry == nullptr)
		return missing;

	for (const std::string& fileName : entry->requiredFiles)
	{
		std::error_code ec;
		if (!std::filesystem::exists(directory / fileName, ec))
			missing.push_back(fileName);
	}

	return missing;
}
}; // namespace ModelRepository
