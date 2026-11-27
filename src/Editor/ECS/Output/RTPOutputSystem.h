#pragma once

#include "ComponentFwd.h"
#include "MikanTypedObjectSystem.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "RTPOutputComponent.h"

#include <memory>
#include <string>

class RtpOutputStream;

class RTPOutputSystemDefinition
	: public MikanTypedObjectSystemDefinition<RTPOutputComponent, RTPOutputDefinition, MikanOutputID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<RTPOutputComponent, RTPOutputDefinition, MikanOutputID>;

	RTPOutputSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);
};

class RTPOutputSystem : public MikanTypedObjectSystem<RTPOutputComponent, RTPOutputDefinition, MikanOutputID,
													  RTPOutputSystem, RTPOutputSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<RTPOutputComponent, RTPOutputDefinition, MikanOutputID, RTPOutputSystem,
										RTPOutputSystemDefinition>;

	RTPOutputSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "RTPOutputSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	inline RTPOutputComponentPtr getOutputById(MikanOutputID outputId) const
	{
		return Super::getTypedComponentById(outputId);
	}
	inline RTPOutputComponentPtr getOutputByName(const std::string& outputName) const
	{
		return Super::getTypedComponentByName(outputName);
	}

	// -- Client return feeds ----
	// The return feeds connected clients asked for are one stream of their own, kept
	// apart from the project's RTP outputs so an output's settings never change
	// what a client receives and a project with no RTP output still serves them.
	// It runs at the default height and bitrate while any client holds a feed.
	void writeReturnFeedFrame(const OutputFrame& frame);
	void closeReturnFeed();
	// "off" with no client feed, else the sender's own report
	std::string getReturnFeedStatusText() const;
	// Whether a client holds a feed and the stream is sending to it
	bool getIsReturnFeedStreaming() const;

	// -- IPropertyInterface ----
	// Read-only, editor-side: the return feed stream's encoder tier, size, rate and counters
	static const std::string k_returnFeedStatusPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

private:
	std::shared_ptr<RtpOutputStream> m_returnFeedStream;
	bool m_bHasReturnFeeds= false;
};
