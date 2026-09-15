#include "AssetReference.h"
#include "PathUtils.h"

#include <algorithm>
#include <cctype>

// -- Asset Reference Config -----
configuru::Config AssetReferenceConfig::writeToJSON()
{
	configuru::Config pt= CommonConfig::writeToJSON();

	pt["class_name"]= className;
	pt["asset_path"]= assetPath;

	return pt;
}

void AssetReferenceConfig::readFromJSON(const configuru::Config& pt)
{
	CommonConfig::readFromJSON(pt);

	className= pt.get_or<std::string>("class_name", "AssetReference");
	assetPath= pt.get_or<std::string>("asset_path", "");
}

bool AssetReferenceConfig::isValid() const { return !className.empty() && !assetPath.empty(); }

// -- Asset Reference -----
AssetReference::~AssetReference() { m_previewTexture= nullptr; }

bool AssetReference::loadFromConfig(AssetReferenceConfigConstPtr config)
{
	setAssetPath(PathUtils::utf8ToPath(config->assetPath));

	return true;
}

void AssetReference::saveToConfig(AssetReferenceConfigPtr config) const
{
	config->className= getClassName();
	config->assetPath= PathUtils::pathToUtf8(m_assetPath);
}

const std::filesystem::path& AssetReference::getInternalAssetPath() const { return m_assetPath; }

const std::filesystem::path AssetReference::getResolvedAssetPath() const
{
	return PathUtils::resolveProjectResource(m_assetPath);
}

void AssetReference::setAssetPath(const std::filesystem::path& inPath)
{
	const std::filesystem::path storedPath=
		inPath.empty() ? std::filesystem::path() : std::filesystem::path(PathUtils::makeStoredProjectPath(inPath));

	if (m_assetPath != storedPath)
	{
		m_assetPath= storedPath;
		rebuildPreview();
	}
}

bool AssetReference::isEmpty() const { return m_assetPath.empty(); }

std::string AssetReference::getShortName() const
{
	if (!m_assetPath.empty() && m_assetPath.has_filename())
	{
		return m_assetPath.filename().string();
	}

	return "";
}

// -- AssetReferenceFactory ----
AssetReferenceConfigPtr AssetReferenceFactory::allocateAssetReferenceConfig() const
{
	return std::make_shared<AssetReferenceConfig>();
}

AssetReferencePtr AssetReferenceFactory::allocateAssetReference() const { return std::make_shared<AssetReference>(); }

bool AssetReferenceFactory::matchesFilterPatterns(const std::filesystem::path& path) const
{
	// Compared as a filename suffix rather than by path::extension(), so a
	// two-part pattern such as "*.pose.json" can match: extension() would see
	// only ".json". Single-extension patterns match exactly as before.
	std::string filename= path.filename().string();
	std::transform(filename.begin(), filename.end(), filename.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	if (filename.empty())
	{
		return false;
	}

	char const* const* patterns= getFilterPatterns();
	const int patternCount= getFilterPatternCount();
	for (int i= 0; i < patternCount; ++i)
	{
		// Patterns are "*.ext", so the match is on the suffix after the star
		std::string pattern(patterns[i]);
		if (!pattern.empty() && pattern[0] == '*')
		{
			pattern.erase(0, 1);
		}
		std::transform(pattern.begin(), pattern.end(), pattern.begin(),
					   [](unsigned char c) { return (char)std::tolower(c); });

		if (!pattern.empty() && filename.size() > pattern.size()
			&& filename.compare(filename.size() - pattern.size(), pattern.size(), pattern) == 0)
		{
			return true;
		}
	}

	return false;
}