#pragma once

#include "OutputComponent.h"

#include <memory>

class RtpOutputStream;

class RTPOutputDefinition : public OutputDefinition
{
public:
	RTPOutputDefinition();
	RTPOutputDefinition(MikanOutputID outputId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	// Where the RTP packets go. An empty host sends nothing.
	static const std::string k_rtpOutputHostPropertyId;
	inline const std::string& getRtpOutputHost() const { return m_rtpOutputHost; }
	void setRtpOutputHost(const std::string& host);

	static const std::string k_rtpOutputPortPropertyId;
	inline int getRtpOutputPort() const { return m_rtpOutputPort; }
	void setRtpOutputPort(int port);

	// The encoded picture height. 0 keeps the composited frame's own size.
	static const std::string k_rtpOutputHeightPropertyId;
	inline int getRtpOutputHeight() const { return m_rtpOutputHeight; }
	void setRtpOutputHeight(int height);

	static const std::string k_rtpOutputBitrateKbpsPropertyId;
	inline int getRtpOutputBitrateKbps() const { return m_rtpOutputBitrateKbps; }
	void setRtpOutputBitrateKbps(int bitrateKbps);

private:
	std::string m_rtpOutputHost;
	int m_rtpOutputPort= 0;
	int m_rtpOutputHeight= 0;
	int m_rtpOutputBitrateKbps= 0;
};

// Streams the composited frame as H.264 over RTP to one destination. The encoder
// opens on the first frame, survives scene switches, and is rebuilt when a setting,
// the frame size or the frame rate changes.
class RTPOutputComponent : public OutputComponent
{
public:
	RTPOutputComponent(MikanObjectWeakPtr owner);

	inline RTPOutputDefinitionPtr getRTPOutputDefinition() const
	{
		return std::static_pointer_cast<RTPOutputDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "RTPOutputComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	virtual void update(float deltaSeconds) override;

	// -- Output interface ----
	virtual void writeFrame(const OutputFrame& frame) override;
	virtual void closeOutput() override;

	// Whether the output would send given a frame: enabled with a destination
	bool getHasDestination() const;
	// The encoded picture size and frame rate. False until a frame has been handed in.
	bool getStreamFormat(int& outWidth, int& outHeight, float& outFps) const;
	// "off", "no destination", "idle" while nothing publishes, else the sender's own report
	std::string getStatusText() const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_rtpOutputStatusPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;

private:
	std::shared_ptr<RtpOutputStream> m_stream;
};
