#include "CaptureManifest.h"
#include "Version.h"

#include <algorithm>
#include <cctype>
#include <ctime>

namespace
{
const char* k_kindNames[(int)eCaptureKind::COUNT]= {"stillphoto", "stillcamera", "trackcamera"};
const char* k_kindExtensions[(int)eCaptureKind::COUNT]= {".stillphoto", ".stillcamera", ".trackcamera"};
const char* k_poseSpaceNames[(int)eCapturePoseSpace::COUNT]= {"arkit_world", "stage"};
const char* k_roleNames[(int)eCaptureMemberRole::COUNT]= {"plate_image", "marker_image", "marker_pose", "movie",
														  "movie_pose",  "depth",        "confidence"};

std::string lowerExtension(const std::filesystem::path& path)
{
	std::string extension= path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
				   [](unsigned char c) { return (char)std::tolower(c); });
	return extension;
}

bool readRequiredString(const configuru::Config& config, const char* key, std::string& outValue, std::string& outError)
{
	if (!config.has_key(key) || !config[key].is_string())
	{
		outError= std::string("Manifest is missing the string ") + key;
		return false;
	}

	outValue= config[key].as_string();
	return true;
}
} // namespace

// -- CaptureKindUtils -----
const char* CaptureKindUtils::kindToString(eCaptureKind kind)
{
	return kind > eCaptureKind::INVALID && kind < eCaptureKind::COUNT ? k_kindNames[(int)kind] : "";
}

eCaptureKind CaptureKindUtils::kindFromString(const std::string& name)
{
	for (int index= 0; index < (int)eCaptureKind::COUNT; ++index)
	{
		if (name == k_kindNames[index])
			return (eCaptureKind)index;
	}

	return eCaptureKind::INVALID;
}

const char* CaptureKindUtils::kindToExtension(eCaptureKind kind)
{
	return kind > eCaptureKind::INVALID && kind < eCaptureKind::COUNT ? k_kindExtensions[(int)kind] : "";
}

eCaptureKind CaptureKindUtils::kindFromExtension(const std::filesystem::path& path)
{
	const std::string extension= lowerExtension(path);
	for (int index= 0; index < (int)eCaptureKind::COUNT; ++index)
	{
		if (extension == k_kindExtensions[index])
			return (eCaptureKind)index;
	}

	return eCaptureKind::INVALID;
}

bool CaptureKindUtils::isCaptureExtension(const std::filesystem::path& path)
{
	return kindFromExtension(path) != eCaptureKind::INVALID;
}

const char* CaptureKindUtils::poseSpaceToString(eCapturePoseSpace poseSpace)
{
	return poseSpace > eCapturePoseSpace::INVALID && poseSpace < eCapturePoseSpace::COUNT
			   ? k_poseSpaceNames[(int)poseSpace]
			   : "";
}

eCapturePoseSpace CaptureKindUtils::poseSpaceFromString(const std::string& name)
{
	for (int index= 0; index < (int)eCapturePoseSpace::COUNT; ++index)
	{
		if (name == k_poseSpaceNames[index])
			return (eCapturePoseSpace)index;
	}

	return eCapturePoseSpace::INVALID;
}

const char* CaptureKindUtils::roleToString(eCaptureMemberRole role)
{
	return role > eCaptureMemberRole::INVALID && role < eCaptureMemberRole::COUNT ? k_roleNames[(int)role] : "";
}

eCaptureMemberRole CaptureKindUtils::roleFromString(const std::string& name)
{
	for (int index= 0; index < (int)eCaptureMemberRole::COUNT; ++index)
	{
		if (name == k_roleNames[index])
			return (eCaptureMemberRole)index;
	}

	return eCaptureMemberRole::INVALID;
}

const std::vector<eCaptureMemberRole>& CaptureKindUtils::requiredRoles(eCaptureKind kind)
{
	static const std::vector<eCaptureMemberRole> s_stillPhoto= {eCaptureMemberRole::plate_image};
	static const std::vector<eCaptureMemberRole> s_stillCamera= {eCaptureMemberRole::movie};
	static const std::vector<eCaptureMemberRole> s_trackCamera= {eCaptureMemberRole::movie,
																 eCaptureMemberRole::movie_pose};
	static const std::vector<eCaptureMemberRole> s_none;

	switch (kind)
	{
	case eCaptureKind::stillphoto:
		return s_stillPhoto;
	case eCaptureKind::stillcamera:
		return s_stillCamera;
	case eCaptureKind::trackcamera:
		return s_trackCamera;
	default:
		return s_none;
	}
}

// -- CaptureManifest -----
bool CaptureManifest::hasMember(eCaptureMemberRole role) const { return members.find(role) != members.end(); }

std::string CaptureManifest::getMemberName(eCaptureMemberRole role) const
{
	auto it= members.find(role);
	return it != members.end() ? it->second : std::string();
}

std::vector<std::string> CaptureManifest::getMemberNames() const
{
	std::vector<std::string> names;
	names.reserve(members.size());
	for (const auto& entry : members)
	{
		names.push_back(entry.second);
	}
	return names;
}

configuru::Config CaptureManifest::toConfig() const
{
	configuru::Config config= configuru::Config::object();
	config["format"]= k_formatName;
	config["version"]= static_cast<int64_t>(k_version);
	config["kind"]= CaptureKindUtils::kindToString(kind);
	config["session_id"]= sessionId;
	config["pose_space"]= CaptureKindUtils::poseSpaceToString(poseSpace);
	config["created_utc"]= createdUtc;
	config["producer"]= producer;
	config["interrupted"]= bInterrupted;

	configuru::Config memberConfig= configuru::Config::object();
	for (const auto& entry : members)
	{
		memberConfig[CaptureKindUtils::roleToString(entry.first)]= entry.second;
	}
	config["members"]= memberConfig;

	return config;
}

std::string CaptureManifest::toJsonString() const { return configuru::dump_string(toConfig(), configuru::JSON); }

bool CaptureManifest::loadFromConfig(const configuru::Config& config, std::string& outError)
{
	clear();

	if (!config.is_object())
	{
		outError= "Manifest root is not an object";
		return false;
	}

	std::string format;
	if (!readRequiredString(config, "format", format, outError))
		return false;
	if (format != k_formatName)
	{
		outError= "Manifest format is " + format + ", expected " + k_formatName;
		return false;
	}

	if (!config.has_key("version") || !config["version"].is_int())
	{
		outError= "Manifest version is missing";
		return false;
	}
	const int version= config["version"].as_integer<int>();
	if (version != k_version)
	{
		outError= "Manifest version " + std::to_string(version) + " is not " + std::to_string(k_version);
		return false;
	}

	std::string kindName;
	if (!readRequiredString(config, "kind", kindName, outError))
		return false;
	const eCaptureKind parsedKind= CaptureKindUtils::kindFromString(kindName);
	if (parsedKind == eCaptureKind::INVALID)
	{
		outError= "Manifest kind " + kindName + " is not a capture kind";
		return false;
	}

	std::string parsedSessionId;
	if (!readRequiredString(config, "session_id", parsedSessionId, outError))
		return false;
	if (parsedSessionId.empty())
	{
		outError= "Manifest session_id is empty";
		return false;
	}

	std::string poseSpaceName;
	if (!readRequiredString(config, "pose_space", poseSpaceName, outError))
		return false;
	const eCapturePoseSpace parsedPoseSpace= CaptureKindUtils::poseSpaceFromString(poseSpaceName);
	if (parsedPoseSpace == eCapturePoseSpace::INVALID)
	{
		outError= "Manifest pose_space " + poseSpaceName + " is not a pose space";
		return false;
	}

	std::string parsedCreatedUtc;
	if (config.has_key("created_utc") && config["created_utc"].is_string())
		parsedCreatedUtc= config["created_utc"].as_string();
	std::string parsedProducer;
	if (config.has_key("producer") && config["producer"].is_string())
		parsedProducer= config["producer"].as_string();

	bool parsedInterrupted= false;
	if (config.has_key("interrupted"))
	{
		if (!config["interrupted"].is_bool())
		{
			outError= "Manifest interrupted is not a boolean";
			return false;
		}
		parsedInterrupted= config["interrupted"].as_bool();
	}

	if (!config.has_key("members") || !config["members"].is_object())
	{
		outError= "Manifest members is missing or not an object";
		return false;
	}

	std::map<eCaptureMemberRole, std::string> parsedMembers;
	for (const auto& entry : config["members"].as_object())
	{
		const eCaptureMemberRole role= CaptureKindUtils::roleFromString(entry.key());
		if (role == eCaptureMemberRole::INVALID)
		{
			outError= "Manifest member role " + entry.key() + " is not a capture role";
			return false;
		}
		if (!entry.value().is_string() || entry.value().as_string().empty())
		{
			outError= "Manifest member " + entry.key() + " is not a member name";
			return false;
		}
		parsedMembers[role]= entry.value().as_string();
	}

	kind= parsedKind;
	sessionId= parsedSessionId;
	poseSpace= parsedPoseSpace;
	createdUtc= parsedCreatedUtc;
	producer= parsedProducer;
	bInterrupted= parsedInterrupted;
	members= std::move(parsedMembers);

	if (!validate(outError))
	{
		clear();
		return false;
	}

	return true;
}

bool CaptureManifest::loadFromJsonString(const std::string& json, std::string& outError)
{
	try
	{
		const configuru::Config config=
			configuru::parse_string(json.c_str(), configuru::JSON, k_manifestMemberName.c_str());
		return loadFromConfig(config, outError);
	}
	catch (std::exception& e)
	{
		clear();
		outError= std::string("Manifest is not valid JSON: ") + e.what();
		return false;
	}
}

void CaptureManifest::clear()
{
	kind= eCaptureKind::INVALID;
	sessionId.clear();
	poseSpace= eCapturePoseSpace::INVALID;
	createdUtc.clear();
	producer.clear();
	bInterrupted= false;
	members.clear();
}

bool CaptureManifest::validate(std::string& outError) const
{
	if (kind == eCaptureKind::INVALID)
	{
		outError= "Manifest has no kind";
		return false;
	}

	if (poseSpace == eCapturePoseSpace::INVALID)
	{
		outError= "Manifest has no pose_space";
		return false;
	}

	if (sessionId.empty())
	{
		outError= "Manifest has no session_id";
		return false;
	}

	for (eCaptureMemberRole role : CaptureKindUtils::requiredRoles(kind))
	{
		if (!hasMember(role))
		{
			outError= std::string("A ") + CaptureKindUtils::kindToString(kind) + " capture needs a "
					  + CaptureKindUtils::roleToString(role) + " member";
			return false;
		}
	}

	if (hasMember(eCaptureMemberRole::depth) != hasMember(eCaptureMemberRole::confidence))
	{
		outError= "Depth and confidence members come together or not at all";
		return false;
	}

	if (hasMember(eCaptureMemberRole::depth) && !hasMember(eCaptureMemberRole::movie_pose))
	{
		outError= "Depth members need a movie_pose track to declare their record size";
		return false;
	}

	std::vector<std::string> names= getMemberNames();
	for (const std::string& name : names)
	{
		if (name == k_manifestMemberName)
		{
			outError= k_manifestMemberName + " is reserved for the manifest";
			return false;
		}
	}
	std::sort(names.begin(), names.end());
	if (std::adjacent_find(names.begin(), names.end()) != names.end())
	{
		outError= "Two manifest roles name the same member";
		return false;
	}

	return true;
}

std::string CaptureManifest::makeUtcTimestampNow()
{
	const std::time_t now= std::time(nullptr);
	std::tm utcTime{};
#if defined(_WIN32)
	gmtime_s(&utcTime, &now);
#else
	gmtime_r(&now, &utcTime);
#endif

	char buffer[32];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utcTime);
	return buffer;
}

std::string CaptureManifest::makeEditorProducer() { return std::string("MikanXR ") + MIKAN_RELEASE_VERSION_STRING; }
