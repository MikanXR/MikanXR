#include "DMXObjectSystem.h"
#include "MikanPropertyDatabase.h"
#include "MikanFunctionDatabase.h"
#include "MikanLightTypes.h"
#include "ProjectManager.h"
#include "MikanLightTypes.h"
#include "ProjectConfig.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// Defined below, beside the setter they round-trip for
static configuru::Config writeUniverseDestinationsToJSON(
	const DMXObjectSystemDefinition::UniverseDestinationMap& destinations);
static void readUniverseDestinationsFromJSON(const configuru::Config& rows,
											 DMXObjectSystemDefinition::UniverseDestinationMap& outDestinations);

// -- DMXObjectSystemDefinition -----
const std::string DMXObjectSystemDefinition::k_networkInterfaceIPPropertyId= "network_interface_ip";
const std::string DMXObjectSystemDefinition::k_dmxPriorityPropertyId= "dmx_priority";
const std::string DMXObjectSystemDefinition::k_dmxDestinationsPropertyId= "dmx_destinations";
const std::string DMXObjectSystemDefinition::k_transmitRateHzPropertyId= "transmit_rate_hz";

configuru::Config DMXObjectSystemDefinition::writeToJSON()
{
	configuru::Config pt= MikanObjectSystemDefinition::writeToJSON();

	pt[k_networkInterfaceIPPropertyId]= m_dmxConfig.networkInterfaceIP;
	pt[k_dmxPriorityPropertyId]= m_dmxConfig.priority;
	pt[k_dmxDestinationsPropertyId]= writeUniverseDestinationsToJSON(m_dmxConfig.universeDestinations);
	pt[k_transmitRateHzPropertyId]= m_dmxConfig.transmitRateHz;

	return pt;
}

void DMXObjectSystemDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanObjectSystemDefinition::readFromJSON(pt);

	m_dmxConfig.networkInterfaceIP= pt.get_or<std::string>(k_networkInterfaceIPPropertyId, "0.0.0.0");
	m_dmxConfig.priority= static_cast<uint8_t>(pt.get_or<int>(k_dmxPriorityPropertyId, 100));
	if (pt.has_key(k_dmxDestinationsPropertyId))
		readUniverseDestinationsFromJSON(pt[k_dmxDestinationsPropertyId], m_dmxConfig.universeDestinations);
	m_dmxConfig.transmitRateHz= pt.get_or<float>(k_transmitRateHzPropertyId, 44.0f);
}

void DMXObjectSystemDefinition::setNetworkInterfaceIP(const std::string& ip)
{
	if (m_dmxConfig.networkInterfaceIP != ip)
	{
		m_dmxConfig.networkInterfaceIP= ip;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_networkInterfaceIPPropertyId));
	}
}

void DMXObjectSystemDefinition::setUniverseDestinations(const UniverseDestinationMap& destinations)
{
	if (m_dmxConfig.universeDestinations != destinations)
	{
		m_dmxConfig.universeDestinations= destinations;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_dmxDestinationsPropertyId));
	}
}

static configuru::Config writeUniverseDestinationsToJSON(
	const DMXObjectSystemDefinition::UniverseDestinationMap& destinations)
{
	auto rows= configuru::Config::array();
	for (const auto& [universe, ips] : destinations)
	{
		auto ipList= configuru::Config::array();
		for (const std::string& ip : ips)
			ipList.push_back(ip);

		auto row= configuru::Config::object();
		row["universe"]= (int)universe;
		row["ips"]= ipList;
		rows.push_back(row);
	}

	return rows;
}

static void readUniverseDestinationsFromJSON(const configuru::Config& rows,
											 DMXObjectSystemDefinition::UniverseDestinationMap& outDestinations)
{
	outDestinations.clear();
	if (!rows.is_array())
		return;

	for (const configuru::Config& row : rows.as_array())
	{
		if (!row.has_key("universe"))
			continue;

		const int universe= (int)row["universe"];
		if (universe < 1 || universe > 63999)
			continue;

		std::vector<std::string> ips;
		if (row.has_key("ips") && row["ips"].is_array())
		{
			for (const configuru::Config& ip : row["ips"].as_array())
			{
				const std::string ipText= (std::string)ip;
				if (!ipText.empty())
					ips.push_back(ipText);
			}
		}

		if (!ips.empty())
			outDestinations[(uint16_t)universe]= ips;
	}
}

std::string DMXObjectSystemDefinition::universeDestinationsToJsonString() const
{
	configuru::FormatOptions options= configuru::make_json_options();
	options.indentation= "";

	return configuru::dump_string(writeUniverseDestinationsToJSON(m_dmxConfig.universeDestinations), options);
}

bool DMXObjectSystemDefinition::universeDestinationsFromJsonString(const std::string& jsonText)
{
	try
	{
		configuru::Config rows= configuru::parse_string(jsonText.c_str(), configuru::JSON, "DMXObjectSystemDefinition");

		UniverseDestinationMap parsed;
		readUniverseDestinationsFromJSON(rows, parsed);
		setUniverseDestinations(parsed);

		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}
}

void DMXObjectSystemDefinition::setDMXPriority(uint8_t priority)
{
	if (m_dmxConfig.priority != priority)
	{
		m_dmxConfig.priority= priority;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_dmxPriorityPropertyId));
	}
}

void DMXObjectSystemDefinition::setTransmitRateHz(float hz)
{
	if (m_dmxConfig.transmitRateHz != hz)
	{
		m_dmxConfig.transmitRateHz= hz;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_transmitRateHzPropertyId));
	}
}

// -- DMXObjectSystem -----
bool DMXObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	MikanObjectSystem::init(definitionPtr);

	// Start the DMX manager
	m_dmxManager= IDMXManager::create();
	if (!m_dmxManager->startup(getDMXManagerConfig()))
	{
		// Non-fatal: editor still loads, fixtures just won't send DMX
		m_dmxManager.reset();
	}

	// Listen for config property changes so we can restart the DMX manager
	definitionPtr->OnPropertyChanged+= MakeDelegate(this, &DMXObjectSystem::onDefinitionMarkedDirty);

	return true;
}

void DMXObjectSystem::dispose()
{
	if (m_dmxManager)
	{
		m_dmxManager->shutdown();
		m_dmxManager.reset();
	}

	auto definitionPtr= getDefinition();
	if (definitionPtr)
	{
		definitionPtr->OnPropertyChanged-= MakeDelegate(this, &DMXObjectSystem::onDefinitionMarkedDirty);
	}

	MikanObjectSystem::dispose();
}

void DMXObjectSystem::update(float deltaSeconds)
{
	// See if any universe has been modified
	bool bAnyUniverseMarkedDirty= false;
	for (const auto& [_, universeData] : m_universeBuffers)
	{
		if (universeData->dirty)
		{
			bAnyUniverseMarkedDirty= true;
			break;
		}
	}

	if (bAnyUniverseMarkedDirty)
	{
		// Broadcast DMX channel channels to clients with light subscriptions
		if (OnDMXDataChanged)
			OnDMXDataChanged();

		// Clear all dirty flags
		for (const auto& [_, universeData] : m_universeBuffers)
		{
			universeData->dirty= false;
		}
	}
}

DMXObjectSystem::UniverseDataPtr DMXObjectSystem::getOrAddUniverseData(uint16_t universeId)
{
	UniverseDataPtr universeData;
	auto it= m_universeBuffers.find(universeId);
	if (it == m_universeBuffers.end())
	{
		universeData= std::make_shared<UniverseData>();
		universeData->universeId= universeId;
		std::memset(universeData->channelData, 0, kDMXUniverseChannelCount);
		universeData->dirty= false;

		m_universeBuffers.insert({universeId, universeData});
	}
	else
	{
		universeData= it->second;
	}

	return universeData;
}

void DMXObjectSystem::writeUniverseData(uint16_t universeId, uint16_t startChannel, const uint8_t* values,
										uint16_t count)
{
	assert(startChannel >= 1 && startChannel <= 512);

	// Forward universe data write directly on to the DMX E1.31 protocol handler
	// to set the physical lights
	m_dmxManager->setChannels(universeId, startChannel, values, count);

	// Update the universe data and mark dirty
	// This
	auto universeData= getOrAddUniverseData(universeId);
	if (universeData)
	{
		const uint16_t maxCount= static_cast<uint16_t>(512 - (startChannel - 1));
		const uint16_t clampedCount= std::min(count, maxCount);

		std::memcpy(&universeData->channelData[startChannel - 1], values, clampedCount);
		universeData->dirty= true;
	}
}

std::set<uint16_t> DMXObjectSystem::getActiveDMXUniverseIdSet() const
{
	std::set<uint16_t> result;

	for (const auto& [universeId, _] : m_universeBuffers)
	{
		result.insert(universeId);
	}

	return result;
}

std::set<uint16_t> DMXObjectSystem::getDirtyDMXUniverseIdSet() const
{
	std::set<uint16_t> result;

	for (const auto& [universeId, universeData] : m_universeBuffers)
	{
		if (universeData->dirty)
		{
			result.insert(universeId);
		}
	}

	return result;
}

bool DMXObjectSystem::extractUniverseData(uint16_t universeId, MikanUniverseDMXData& outUniverseData)
{
	auto it= m_universeBuffers.find(universeId);
	if (it != m_universeBuffers.end())
	{
		UniverseDataPtr universeData= it->second;
		outUniverseData.dmx_universe_id= universeId;

		return mikanRLEEncodeDMXUniverseBuffer(kDMXUniverseChannelCount, universeData->channelData, &outUniverseData)
			   > 0;
	}

	return false;
}

DMXObjectSystemDefinitionConstPtr DMXObjectSystem::getDMXObjectSystemConfigConst() const
{
	auto projectConfig= getProjectConfig();

	return projectConfig ? projectConfig->dmxObjectSystemDefinition : DMXObjectSystemDefinitionConstPtr();
}

DMXObjectSystemDefinitionPtr DMXObjectSystem::getDMXObjectSystemConfig()
{
	return std::const_pointer_cast<DMXObjectSystemDefinition>(getDMXObjectSystemConfigConst());
}

MikanComponentPtr DMXObjectSystem::getComponentById(int componentId) const
{
	// DMXObjectSystem doesn't manage components by ID
	return MikanComponentPtr();
}

bool DMXObjectSystem::getComponentList(const std::string& componentClassName,
									   std::vector<MikanComponentPtr>& outComponentList) const
{
	// DMXObjectSystem doesn't manage ownership of components
	return false;
}

bool DMXObjectSystem::getComponentIdList(const std::string& componentClassName,
										 std::vector<int>& outComponentIdList) const
{
	// DMXObjectSystem doesn't manage ownership of components
	return false;
}

// Project Config Events
void DMXObjectSystem::onDefinitionMarkedDirty(CommonConfigPtr configPtr, const class ConfigPropertyChangeSet& changeSet)
{
	if (changeSet.hasPropertyName(DMXObjectSystemDefinition::k_networkInterfaceIPPropertyId)
		|| changeSet.hasPropertyName(DMXObjectSystemDefinition::k_dmxPriorityPropertyId)
		|| changeSet.hasPropertyName(DMXObjectSystemDefinition::k_transmitRateHzPropertyId)
		|| changeSet.hasPropertyName(DMXObjectSystemDefinition::k_dmxDestinationsPropertyId))
	{
		if (m_dmxManager)
			m_dmxManager->restart(getDMXManagerConfig());
	}
}

void DMXObjectSystem::registerPropertyDescriptors(MikanPropertyDatabasePtr propertyDatabase)
{
	propertyDatabase->registerPropertiesForSystem<DMXObjectSystem>();
}

void DMXObjectSystem::registerFunctionDescriptors(MikanFunctionDatabasePtr functionDatabase)
{
	functionDatabase->registerFunctionsForSystem<DMXObjectSystem>();
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle DMXObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanDMXObjectSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void DMXObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanObjectSystem::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		DMXObjectSystemDefinition::k_networkInterfaceIPPropertyId, MikanVariantType::STRING));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXObjectSystemDefinition::k_dmxPriorityPropertyId,
																  MikanVariantType::UBYTE));
	// The panel draws a real table and a client has no use for LAN routing, so this
	// carries the JSON only for automation and persistence
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 DMXObjectSystemDefinition::k_dmxDestinationsPropertyId, MikanVariantType::STRING)
								 ->setUIHidden()
								 ->setClientAPIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXObjectSystemDefinition::k_transmitRateHzPropertyId,
																  MikanVariantType::FLOAT));
}

bool DMXObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	DMXObjectSystemDefinitionConstPtr def= getDMXObjectSystemConfigConst();

	if (propertyName == DMXObjectSystemDefinition::k_networkInterfaceIPPropertyId)
	{
		outValue= def->getNetworkInterfaceIP();
		return true;
	}
	else if (propertyName == DMXObjectSystemDefinition::k_dmxPriorityPropertyId)
	{
		outValue= def->getDMXPriority();
		return true;
	}
	else if (propertyName == DMXObjectSystemDefinition::k_dmxDestinationsPropertyId)
	{
		outValue= def->universeDestinationsToJsonString();
		return true;
	}
	else if (propertyName == DMXObjectSystemDefinition::k_transmitRateHzPropertyId)
	{
		outValue= def->getTransmitRateHz();
		return true;
	}

	return MikanObjectSystem::getPropertyValue(propertyName, outValue);
}

bool DMXObjectSystem::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	DMXObjectSystemDefinitionPtr def= getDMXObjectSystemConfig();

	if (propertyName == DMXObjectSystemDefinition::k_networkInterfaceIPPropertyId)
	{
		const std::string ip= inValue.getUtf8Value();
		def->setNetworkInterfaceIP(ip);
		return true;
	}
	else if (propertyName == DMXObjectSystemDefinition::k_dmxPriorityPropertyId)
	{
		def->setDMXPriority(inValue.getUByteValue());
		return true;
	}
	else if (propertyName == DMXObjectSystemDefinition::k_dmxDestinationsPropertyId)
	{
		return def->universeDestinationsFromJsonString(inValue.getUtf8Value());
	}
	else if (propertyName == DMXObjectSystemDefinition::k_transmitRateHzPropertyId)
	{
		def->setTransmitRateHz(inValue.getFloatValue());
		return true;
	}

	return MikanObjectSystem::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string DMXObjectSystem::k_zeroAllChannelsFunctionId= "zero_all_channels";

void DMXObjectSystem::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanObjectSystem::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_zeroAllChannelsFunctionId, "Zero All Channels"));
}

bool DMXObjectSystem::invokeFunction(const std::string& functionName)
{
	if (functionName == k_zeroAllChannelsFunctionId)
	{
		zeroAllChannels();
		return true;
	}

	return MikanObjectSystem::invokeFunction(functionName);
}

void DMXObjectSystem::zeroAllChannels()
{
	// Through the fixtures first, so their runtime colors and the viewport
	// follow the blackout and a later capture records zeros
	const std::vector<uint8_t> noChannels;
	if (RGBSpotLightSystemPtr spotLightSystem= getOwnerProjectManager()->getSystemOfType<RGBSpotLightSystem>())
	{
		spotLightSystem->visitComponents([&noChannels](RGBSpotLightComponentPtr light)
										 { light->setChannelValues(noChannels); });
	}
	if (RGBPixelGridSystemPtr pixelGridSystem= getOwnerProjectManager()->getSystemOfType<RGBPixelGridSystem>())
	{
		pixelGridSystem->visitComponents([&noChannels](RGBPixelGridComponentPtr grid)
										 { grid->setChannelValues(noChannels); });
	}

	// Then every active universe, clearing slots no fixture covers
	std::vector<uint8_t> zeroData(kDMXUniverseChannelCount, 0);
	for (const auto& [universeId, universeData] : m_universeBuffers)
	{
		writeUniverseData(universeId, 1, zeroData.data(), static_cast<uint16_t>(zeroData.size()));
	}
}

// -- Lua Binding ----
void DMXObjectSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<DMXObjectSystem>("DMXObjectSystem")
		.addFunction("getSpotLightCount",
					 [](DMXObjectSystem* s) -> int
					 {
						 auto sys= s->getObjectSystemOfType<RGBSpotLightSystem>();
						 return sys ? static_cast<int>(sys->getComponentMap().size()) : 0;
					 })
		.addFunction("getSpotLightAtIndex",
					 [](DMXObjectSystem* s, int i) -> RGBSpotLightComponent*
					 {
						 auto sys= s->getObjectSystemOfType<RGBSpotLightSystem>();
						 if (!sys)
							 return nullptr;
						 int n= 0;
						 for (auto& [id, wp] : sys->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.addFunction("getPixelGridCount",
					 [](DMXObjectSystem* s) -> int
					 {
						 auto sys= s->getObjectSystemOfType<RGBPixelGridSystem>();
						 return sys ? static_cast<int>(sys->getComponentMap().size()) : 0;
					 })
		.addFunction("getPixelGridAtIndex",
					 [](DMXObjectSystem* s, int i) -> RGBPixelGridComponent*
					 {
						 auto sys= s->getObjectSystemOfType<RGBPixelGridSystem>();
						 if (!sys)
							 return nullptr;
						 int n= 0;
						 for (auto& [id, wp] : sys->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.addProperty("universeChannelCount", [](DMXObjectSystem*) -> int
					 { return static_cast<int>(DMXObjectSystem::kDMXUniverseChannelCount); })
		.addFunction("zeroAllChannels", [](DMXObjectSystem* s) { s->zeroAllChannels(); })
		.endClass();
}
