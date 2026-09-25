#include "DMXFixtureGroupComponent.h"
#include "CommonScriptContext.h"
#include "DMXFixtureComponent.h"
#include "MikanObjectSystem.h"
#include "MikanVariantTypes.h"
#include "RGBPixelGridComponent.h"
#include "RGBPixelGridSystem.h"
#include "RGBSpotLightComponent.h"
#include "RGBSpotLightSystem.h"
#include "StageObjectSystem.h"

#include <algorithm>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- DMXFixtureGroupDefinition -----
const std::string DMXFixtureGroupDefinition::k_ownerStageIdPropertyId= "stage_id";
const std::string DMXFixtureGroupDefinition::k_fixtureIdsPropertyId= "fixture_ids";

DMXFixtureGroupDefinition::DMXFixtureGroupDefinition()
	: MikanComponentDefinition()
{
}

DMXFixtureGroupDefinition::DMXFixtureGroupDefinition(MikanDMXFixtureGroupID groupId)
	: MikanComponentDefinition(groupId, "")
{
}

configuru::Config DMXFixtureGroupDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_ownerStageIdPropertyId]= m_stageId;
	writeStdValueVector(pt, k_fixtureIdsPropertyId, m_fixtureIds);

	return pt;
}

void DMXFixtureGroupDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_stageId= pt.get_or<MikanStageID>(k_ownerStageIdPropertyId, m_stageId);

	m_fixtureIds.clear();
	if (pt.has_key(k_fixtureIdsPropertyId))
	{
		readStdValueVector(pt, k_fixtureIdsPropertyId, m_fixtureIds);
	}
}

bool DMXFixtureGroupDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanDMXFixtureGroupComponentValues>();
	if (values)
	{
		m_stageId= values->stage_id;

		m_fixtureIds.clear();
		m_fixtureIds.reserve(values->fixture_ids.size());
		for (const MikanLightID fixtureId : values->fixture_ids)
		{
			if (!containsFixture(fixtureId))
				m_fixtureIds.push_back(fixtureId);
		}
	}

	if (m_stageId == INVALID_MIKAN_ID)
	{
		// If no owning stage was specified, use the first one
		auto stageSystem= ownerObjectSystem->getObjectSystemOfType<StageObjectSystem>();

		m_stageId= stageSystem->getFirstStageId();
	}

	return true;
}

void DMXFixtureGroupDefinition::setOwnerStageId(MikanStageID stageId)
{
	if (stageId != m_stageId)
	{
		m_stageId= stageId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_ownerStageIdPropertyId));
	}
}

bool DMXFixtureGroupDefinition::containsFixture(MikanLightID fixtureId) const
{
	return std::find(m_fixtureIds.begin(), m_fixtureIds.end(), fixtureId) != m_fixtureIds.end();
}

bool DMXFixtureGroupDefinition::addFixture(MikanLightID fixtureId)
{
	if (fixtureId == INVALID_MIKAN_ID || containsFixture(fixtureId))
		return false;

	m_fixtureIds.push_back(fixtureId);
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_fixtureIdsPropertyId));
	return true;
}

bool DMXFixtureGroupDefinition::removeFixture(MikanLightID fixtureId)
{
	auto it= std::find(m_fixtureIds.begin(), m_fixtureIds.end(), fixtureId);
	if (it == m_fixtureIds.end())
		return false;

	m_fixtureIds.erase(it);
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_fixtureIdsPropertyId));
	return true;
}

void DMXFixtureGroupDefinition::setFixtureIds(const std::vector<MikanLightID>& fixtureIds)
{
	if (fixtureIds == m_fixtureIds)
		return;

	m_fixtureIds.clear();
	for (const MikanLightID fixtureId : fixtureIds)
	{
		if (fixtureId != INVALID_MIKAN_ID && !containsFixture(fixtureId))
			m_fixtureIds.push_back(fixtureId);
	}
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_fixtureIdsPropertyId));
}

// -- DMXFixtureGroupComponent -----
DMXFixtureGroupComponent::DMXFixtureGroupComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

DMXFixtureComponentPtr DMXFixtureGroupComponent::resolveFixture(MikanLightID fixtureId) const
{
	if (fixtureId == INVALID_MIKAN_ID)
		return nullptr;

	if (RGBSpotLightSystemPtr spotLightSystem= getObjectSystemOfType<RGBSpotLightSystem>())
	{
		if (RGBSpotLightComponentPtr spotLight= spotLightSystem->getLightById(fixtureId))
			return spotLight;
	}

	if (RGBPixelGridSystemPtr pixelGridSystem= getObjectSystemOfType<RGBPixelGridSystem>())
	{
		if (RGBPixelGridComponentPtr pixelGrid= pixelGridSystem->getGridById(fixtureId))
			return pixelGrid;
	}

	return nullptr;
}

void DMXFixtureGroupComponent::getFixtures(std::vector<DMXFixtureComponentPtr>& outFixtures) const
{
	for (const MikanLightID fixtureId : getDMXFixtureGroupDefinition()->getFixtureIds())
	{
		if (DMXFixtureComponentPtr fixture= resolveFixture(fixtureId))
			outFixtures.push_back(fixture);
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle DMXFixtureGroupComponent::getClientAPIValuesStructType() const
{
	return &MikanDMXFixtureGroupComponentValues::staticGetArchetype();
}

// -- IPropertyInterface ----
void DMXFixtureGroupComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXFixtureGroupDefinition::k_ownerStageIdPropertyId,
																  MikanVariantType::INT));
	// The group panel draws the membership list itself
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXFixtureGroupDefinition::k_fixtureIdsPropertyId,
																  MikanVariantType::INT_ARRAY)
								 ->setUIHidden());
}

bool DMXFixtureGroupComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	DMXFixtureGroupDefinitionPtr definition= getDMXFixtureGroupDefinition();

	if (propertyName == DMXFixtureGroupDefinition::k_ownerStageIdPropertyId)
	{
		outValue= static_cast<int>(definition->getOwnerStageId());
		return true;
	}
	else if (propertyName == DMXFixtureGroupDefinition::k_fixtureIdsPropertyId)
	{
		outValue= definition->getFixtureIds();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool DMXFixtureGroupComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	DMXFixtureGroupDefinitionPtr definition= getDMXFixtureGroupDefinition();

	if (propertyName == DMXFixtureGroupDefinition::k_ownerStageIdPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT)
			return false;

		definition->setOwnerStageId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == DMXFixtureGroupDefinition::k_fixtureIdsPropertyId)
	{
		if (inValue.value_type != MikanVariantType::INT_ARRAY)
			return false;

		const auto& idList= inValue.getIntArrayValue();
		definition->setFixtureIds(std::vector<MikanLightID>(idList.begin(), idList.end()));
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void DMXFixtureGroupComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<DMXFixtureGroupComponent, MikanComponent>(DMXFixtureGroupComponent::k_componentClassName.c_str())
		.addProperty("stageId", [](DMXFixtureGroupComponent* c) -> int
					 { return c->getDMXFixtureGroupDefinition()->getOwnerStageId(); })
		.addFunction("getFixtureCount",
					 [](DMXFixtureGroupComponent* c) -> int
					 {
						 std::vector<DMXFixtureComponentPtr> fixtures;
						 c->getFixtures(fixtures);
						 return static_cast<int>(fixtures.size());
					 })
		// A group holds fixtures of mixed kinds, so this returns each one as its
		// concrete class rather than as a DMXFixtureComponent. Returning the
		// base pointer would hand Lua the base class's metatable and hide the
		// subclass's own bindings, since LuaBridge pushes by static type.
		.addFunction("getFixtureAtIndex",
					 [](DMXFixtureGroupComponent* c, int index, lua_State* L) -> luabridge::LuaRef
					 {
						 std::vector<DMXFixtureComponentPtr> fixtures;
						 c->getFixtures(fixtures);

						 DMXFixtureComponentPtr fixture;
						 if (index >= 0 && index < static_cast<int>(fixtures.size()))
							 fixture= fixtures[index];

						 CommonScriptContext* scriptContext= CommonScriptContext::getFromLuaState(L);
						 if (scriptContext != nullptr)
							 scriptContext->pushComponent(L, fixture);
						 else
							 lua_pushnil(L);

						 luabridge::LuaRef typedFixture= luabridge::LuaRef::fromStack(L, -1);
						 lua_pop(L, 1);

						 return typedFixture;
					 })
		.addFunction("containsFixture", [](DMXFixtureGroupComponent* c, int fixtureId) -> bool
					 { return c->getDMXFixtureGroupDefinition()->containsFixture(fixtureId); })
		.addFunction("addFixture", [](DMXFixtureGroupComponent* c, int fixtureId) -> bool
					 { return c->getDMXFixtureGroupDefinition()->addFixture(fixtureId); })
		.addFunction("removeFixture", [](DMXFixtureGroupComponent* c, int fixtureId) -> bool
					 { return c->getDMXFixtureGroupDefinition()->removeFixture(fixtureId); })
		.endClass();
}
