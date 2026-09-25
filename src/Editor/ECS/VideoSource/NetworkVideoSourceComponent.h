#pragma once

#include "INetworkVideoDevice.h"
#include "VideoSourceComponent.h"

extern const std::string* k_NetworkVideoProtocol;

class NetworkVideoSourceDefinition : public VideoSourceDefinition
{
public:
	NetworkVideoSourceDefinition();
	NetworkVideoSourceDefinition(MikanVideoSourceID videoSourceId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	void setURL(const std::string& URL);

	static const std::string k_addressPropertyId;
	inline const std::string& getAddress() const { return m_address; }
	void setAddress(const std::string& address);

	static const std::string k_pathPropertyId;
	inline const std::string& getPath() const { return m_path; }
	void setPath(const std::string& path);

	static const std::string k_protocolPropertyId;
	inline eNetworkVideoProtocol getProtocol() const { return m_protocol; }
	const std::string getProtocolString() const;
	void setProtocol(eNetworkVideoProtocol protocol);
	void setProtocol(const std::string& protocolString);

	static const std::string k_portPropertyId;
	inline int getPort() const { return m_port; }
	void setPort(int port);

	static bool parseUrl(const std::string& url, eNetworkVideoProtocol& outProtocol, std::string& outAddress,
						 int& outPort, std::string& outPath);

private:
	eNetworkVideoProtocol m_protocol;
	std::string m_address;
	std::string m_path;
	int m_port= 0;
};

class NetworkVideoSourceComponent : public VideoSourceComponent, public INetworkVideoDeviceListener
{
public:
	NetworkVideoSourceComponent(MikanObjectWeakPtr owner);

	virtual void init() override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "NetworkVideoSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline NetworkVideoSourceDefinitionPtr getNetworkVideoSourceDefinition() const
	{
		return std::static_pointer_cast<NetworkVideoSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	// -- Video Source Interface ----
	virtual std::string getDevicePath() const override;
	virtual std::string getDeviceAPI() const override;
	virtual bool openVideoSource() override;
	virtual void closeVideoSource() override;
	virtual eVideoStreamingStatus getVideoStreamingStatus() const override;
	virtual bool getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const override;
	virtual bool getVideoColorimetry(VideoColorimetry& outColorimetry) const override;

	// -- INetworkVideoDeviceListener ----
	virtual void notifyVideoDeviceOpened(const class INetworkVideoDevice* device) override;
	virtual void notifyVideoDeviceClosed(const class INetworkVideoDevice* device) override;
	virtual void notifyVideoModePropertiesChanged(const class INetworkVideoDevice* device) override;
	virtual void notifyVideoFrameReceived(const NetworkVideoFrameBuffer& bufferInfo) override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	bool isPendingOpen() const { return m_bPendingOpen; }

protected:
	virtual eVideoStreamingStatus startVideoStreamInternal() override;
	virtual void stopVideoStreamInternal() override;

	void onDefinitionMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

private:
	INetworkVideoDevicePtr m_networkVideoDevice= nullptr;
	bool m_bPendingOpen= false;
};