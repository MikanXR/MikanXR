#include "ModelCatalog.h"

namespace
{
// Sizes are the measured transfer, rounded for display only where the header
// says so. The MoGe-2 hash is the LFS object id Hugging Face reports for the
// file, which is its SHA256.
constexpr uint64_t k_moge2ModelBytes= 1324265014ull;

// Marigold arrives as twelve assets whose exact sizes live in the release
// manifest. This is the total for the prompt, and the disk figure adds the
// 3.46GB sidecar that the two split parts are concatenated into before the
// parts are deleted.
constexpr uint64_t k_marigoldDownloadBytes= 3800000000ull;
constexpr uint64_t k_marigoldDiskBytes= 7300000000ull;

std::vector<ModelCatalogEntry> buildEntries()
{
	std::vector<ModelCatalogEntry> entries;

	{
		ModelCatalogEntry entry;
		entry.id= eModelId::moge2;
		entry.name= "moge2";
		entry.displayNameLocKey= "modelDownload.moge2Name";
		entry.descriptionLocKey= "modelDownload.moge2Description";
		entry.approxDownloadBytes= k_moge2ModelBytes;
		entry.requiredDiskBytes= k_moge2ModelBytes;
		entry.licenseName= "MIT";
		entry.bRequiresLicenseAcceptance= false;
		entry.licenseUrl= "https://huggingface.co/Ruicheng/moge-2-vitl-normal-onnx";
		entry.requiredFiles= {"model.onnx"};
		entry.directFiles= {
			{"model.onnx", "https://huggingface.co/Ruicheng/moge-2-vitl-normal-onnx/resolve/main/model.onnx",
			 k_moge2ModelBytes, "afbc4ccc3450298f3afb35b90f015f4c4f552dea21dc6470d5f7b78b77e2d751"},
		};
		entries.push_back(entry);
	}

	{
		ModelCatalogEntry entry;
		entry.id= eModelId::marigold;
		entry.name= "marigold";
		entry.displayNameLocKey= "modelDownload.marigoldName";
		entry.descriptionLocKey= "modelDownload.marigoldDescription";
		entry.approxDownloadBytes= k_marigoldDownloadBytes;
		entry.requiredDiskBytes= k_marigoldDiskBytes;
		entry.licenseName= "CreativeML Open RAIL++-M";
		entry.bRequiresLicenseAcceptance= true;
		entry.licenseUrl= "https://github.com/MikanXR/MikanMarigoldOnnx/blob/main/NOTICE.md";
		// What MarigoldInference::startup opens. The .data sidecars are named
		// from inside their graphs, so they have to sit here under exactly
		// these names.
		entry.requiredFiles= {
			"vae_encoder.onnx",         "vae_encoder.onnx.data",  "vae_decoder.onnx",
			"vae_decoder.onnx.data",    "unet_iid_lighting.onnx", "unet_iid_lighting.onnx.data",
			"empty_text_embedding.bin", "scheduler.json",
		};
		entry.releaseBaseUrl= "https://github.com/MikanXR/MikanMarigoldOnnx/releases/download/iid-lighting-v1-1-onnx-1";
		entries.push_back(entry);
	}

	return entries;
}
} // namespace

namespace ModelCatalog
{
const std::vector<ModelCatalogEntry>& getEntries()
{
	static const std::vector<ModelCatalogEntry> s_entries= buildEntries();

	return s_entries;
}

const ModelCatalogEntry* findEntry(eModelId id)
{
	for (const ModelCatalogEntry& entry : getEntries())
	{
		if (entry.id == id)
			return &entry;
	}

	return nullptr;
}

const ModelCatalogEntry* findEntryByName(const std::string& name)
{
	for (const ModelCatalogEntry& entry : getEntries())
	{
		if (entry.name == name)
			return &entry;
	}

	return nullptr;
}

std::string getEntryNameList()
{
	std::string list;
	for (const ModelCatalogEntry& entry : getEntries())
	{
		if (!list.empty())
			list+= ", ";
		list+= entry.name;
	}

	return list;
}
}; // namespace ModelCatalog
