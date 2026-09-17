#pragma once

#include "ModelCatalog.h"

#include <filesystem>
#include <string>
#include <vector>

/// Where the ML models live and whether they are installed.
///
/// Two locations, because two situations. A developer checkout has the models
/// under models/ beside the executable's working directory, which is what the
/// tools in tools/ write and what every doc example assumes. An installed
/// build has no writable models/ next to Mikan.exe, so a downloaded model goes
/// to the per-user location instead. Both are searched, and an explicit path
/// from the command line beats both.
namespace ModelRepository
{
/// %LOCALAPPDATA%/MikanXR/models. Where a download lands.
std::filesystem::path getUserModelsRoot();

/// Where a download would install this model.
std::filesystem::path getDownloadDirectory(eModelId id);

/// The directory the model is actually installed in, or an empty path when it
/// is nowhere to be found.
std::filesystem::path findInstalledDirectory(eModelId id);

/// The directory to hand an inference config: the override when one is given,
/// otherwise wherever the model is installed, otherwise where a download would
/// put it. Always returns a path, so the caller's own "missing model" error
/// can name a concrete location.
std::filesystem::path resolveDirectory(eModelId id, const std::string& overrideDirectory= std::string());

/// True when every file the catalog requires is present in the resolved
/// directory. An override that is missing files is reported missing rather
/// than silently searched past: an explicit path is a statement of intent.
bool isModelInstalled(eModelId id, const std::string& overrideDirectory= std::string());

/// Required files that are absent from the given directory, for an error
/// message that says what is actually wrong.
std::vector<std::string> getMissingFiles(eModelId id, const std::filesystem::path& directory);
}; // namespace ModelRepository
