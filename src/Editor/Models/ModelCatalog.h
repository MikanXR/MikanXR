#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// The ML models the capture stages run. Not shipped with the editor: together
/// they are about 5GB, past what a release can carry, so they are downloaded on
/// first use (see ModelDownloadTask) or dropped into a developer's models/
/// folder by hand.
enum class eModelId : int
{
	INVALID= -1,

	/// MoGe-2 ViT-L with the normal head. Metric geometry for the depth proxy
	/// mesh, and the surface normals half of the scene lighting fit.
	moge2,
	/// Marigold IID lighting, exported to ONNX by MikanXR/MikanMarigoldOnnx.
	/// The diffuse shading half of the scene lighting fit.
	marigold,

	COUNT
};

/// One file fetched directly from a URL, for a source that publishes no
/// manifest of its own.
struct ModelDirectFile
{
	std::string name;   ///< what it is called inside the model directory
	std::string url;    ///< absolute https URL
	uint64_t sizeBytes; ///< expected size, checked after the download
	std::string sha256; ///< expected content hash, lowercase hex
};

struct ModelCatalogEntry
{
	eModelId id= eModelId::INVALID;

	/// Stable short name. Doubles as the model's directory name and as the
	/// value of MikanCmd's -model= argument, so it appears in paths and in
	/// documentation and should not change.
	std::string name;

	/// Localization keys for the download prompt.
	std::string displayNameLocKey;
	std::string descriptionLocKey;

	/// Rounded total transfer, for the prompt. The exact figure comes from the
	/// manifest or the file list at download time.
	uint64_t approxDownloadBytes= 0;

	/// Peak disk needed while installing, which exceeds the download whenever
	/// split parts have to be concatenated before they can be deleted.
	uint64_t requiredDiskBytes= 0;

	std::string licenseName;
	/// True when the license carries use restrictions the operator has to be
	/// shown and accept before anything is fetched. See NOTICE.md in
	/// MikanXR/MikanMarigoldOnnx for why this is not a formality.
	bool bRequiresLicenseAcceptance= false;
	std::string licenseUrl;

	/// Files that must exist for the model to count as installed. The manifest
	/// is authoritative when downloading; this list is what lets a presence
	/// check work with no network.
	std::vector<std::string> requiredFiles;

	/// Exactly one source is set. directFiles names every file and its hash;
	/// releaseBaseUrl points at a directory holding a manifest.json that does.
	std::vector<ModelDirectFile> directFiles;
	std::string releaseBaseUrl;

	bool hasManifest() const { return !releaseBaseUrl.empty(); }
};

namespace ModelCatalog
{
/// Every model, in eModelId order.
const std::vector<ModelCatalogEntry>& getEntries();

/// Null when the id or name is not one of ours.
const ModelCatalogEntry* findEntry(eModelId id);
const ModelCatalogEntry* findEntryByName(const std::string& name);

/// Comma separated list of every name, for command line help and errors.
std::string getEntryNameList();
}; // namespace ModelCatalog
