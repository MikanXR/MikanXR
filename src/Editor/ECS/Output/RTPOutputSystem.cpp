#include "RTPOutputSystem.h"
#include "IEditorWindow.h"
#include "MikanObject.h"
#include "MikanPropertyDatabase.h"
#include "MikanServer.h"
#include "MikanVariantTypes.h"
#include "NetworkVideoSourceSystem.h"
#include "ProjectConfig.h"
#include "ProjectConfigConstants.h"
#include "PropertyInterface.h"
#include "RtpOutputStream.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- RTPOutputSystemDefinition -----
RTPOutputSystemDefinition::RTPOutputSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- RTPOutputSystem -----
RTPOutputSystem::RTPOutputSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

void RTPOutputSystem::update(float deltaSeconds)
{
	Super::update(deltaSeconds);

	if (m_returnFeedStream)
	{
		m_returnFeedStream->update(deltaSeconds);
	}
}

void RTPOutputSystem::dispose()
{
	m_returnFeedStream= nullptr;
	m_bHasReturnFeeds= false;

	Super::dispose();
}

// -- Client return feeds ----
void RTPOutputSystem::writeReturnFeedFrame(const OutputFrame& frame)
{
	// MikanCmd runs no server
	RtpOutputStream::Config config;
	if (MikanServer* mikanServer= MikanServer::getInstance())
	{
		mikanServer->collectReturnFeedDestinations(frame.compositorId, true, config.destinations);
	}
	m_bHasReturnFeeds= !config.destinations.empty();

	if (!m_bHasReturnFeeds && !m_returnFeedStream)
		return;

	config.height= DEFAULT_RTP_OUTPUT_HEIGHT;
	config.bitrateKbps= DEFAULT_RTP_OUTPUT_BITRATE_KBPS;

	if (!m_returnFeedStream)
	{
		IEditorWindow* ownerWindow= getOwnerWindow();
		m_returnFeedStream=
			std::make_shared<RtpOutputStream>(ownerWindow ? ownerWindow->getGraphicsContext().get() : nullptr,
											  getObjectSystemOfType<NetworkVideoSourceSystem>());
	}

	// With no destinations left the stream closes itself
	m_returnFeedStream->writeFrame(frame, config);
}

void RTPOutputSystem::closeReturnFeed()
{
	if (m_returnFeedStream)
	{
		m_returnFeedStream->close();
	}
}

std::string RTPOutputSystem::getReturnFeedStatusText() const
{
	if (!m_bHasReturnFeeds || !m_returnFeedStream)
		return "off";

	return m_returnFeedStream->getStatusText();
}

bool RTPOutputSystem::getIsReturnFeedStreaming() const
{
	return m_bHasReturnFeeds && m_returnFeedStream && m_returnFeedStream->getIsStreaming();
}

// -- IPropertyInterface ----
const std::string RTPOutputSystem::k_returnFeedStatusPropertyId= "return_feed_status";

void RTPOutputSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	// Runtime state for a drive to read, so it stays out of the panels and the client API
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_returnFeedStatusPropertyId, MikanVariantType::STRING)
			->setReadOnly()
			->setUIHidden()
			->setClientAPIHidden());
}

bool RTPOutputSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == k_returnFeedStatusPropertyId)
	{
		outValue= getReturnFeedStatusText().c_str();
		return true;
	}

	return Super::getPropertyValue(propertyName, outValue);
}

// -- Lua Binding ----
void RTPOutputSystem::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.beginClass<RTPOutputSystem>("RTPOutputSystem")
		.addFunction("getOutputById", [](RTPOutputSystem* s, int id) -> RTPOutputComponent*
					 { return s->getOutputById(static_cast<MikanOutputID>(id)).get(); })
		.addFunction("getOutputByName", [](RTPOutputSystem* s, const std::string& name) -> RTPOutputComponent*
					 { return s->getOutputByName(name).get(); })
		.addFunction("getOutputCount",
					 [](RTPOutputSystem* s) -> int { return static_cast<int>(s->getComponentMap().size()); })
		.addFunction("getOutputAtIndex",
					 [](RTPOutputSystem* s, int i) -> RTPOutputComponent*
					 {
						 int n= 0;
						 for (auto& [id, wp] : s->getComponentMap())
							 if (n++ == i)
								 return wp.lock().get();
						 return nullptr;
					 })
		.endClass();
}
