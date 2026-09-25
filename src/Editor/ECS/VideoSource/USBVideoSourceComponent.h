#pragma once

#include "VideoSourceComponent.h"
#include "IUsbVideoDevice.h"
#include "MulticastDelegate.h"

using USBVideoSettingsArray= std::array<float, (int)eVideoSettingType::COUNT>;
using USBVideoConstraintArray= std::array<VideoSettingConstraint, (int)eVideoSettingType::COUNT>;

class USBVideoSourceDefinition : public VideoSourceDefinition
{
public:
	USBVideoSourceDefinition();
	USBVideoSourceDefinition(MikanVideoSourceID videoSourceId);

	virtual bool wantsSaveForPropertyChange(const ConfigPropertyChangeSet& changedPropertySet) const;
	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_desiredDevicePathPropertyId;
	inline const std::string& getDevicePath() const { return m_devicePath; }
	void setDevicePath(const std::string& devicePath);

	static const std::string k_videoModePropertyId;
	inline const std::string& getVideoMode() const { return m_videoMode; }
	void setVideoMode(const std::string& videoMode);

	// Runtime only properties to video mode settings
	static const std::string k_videoResolutionPropertyId;
	static const std::string k_videoFrameRatePropertyId;
	static const std::string k_videoFormatPropertyId;

	static const std::string k_videoSettingsPropertyId;
	bool getVideoSettingsForMode(const std::string& modeName, USBVideoSettingsArray& outSettings) const;
	void setCameraSettingsForMode(const std::string& modeName, const USBVideoSettingsArray& settings);
	bool hasVideoSettingsForMode(const std::string& videoModeName) const;
	void notifyCameraSettingsChanged();

private:
	std::string m_devicePath;
	std::string m_videoMode;
	std::map<std::string, USBVideoSettingsArray> m_videoSettingsMap;
};

class USBVideoSourceComponent : public VideoSourceComponent, public IUsbVideoDeviceListener
{
public:
	USBVideoSourceComponent(MikanObjectWeakPtr owner);

	virtual void init() override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;

	inline USBVideoSourceDefinitionPtr getUSBVideoSourceDefinition() const
	{
		return std::static_pointer_cast<USBVideoSourceDefinition>(m_definition);
	}
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;

	inline static const std::string k_componentClassName= "USBVideoSourceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline bool isPendingOpen() const { return m_bPendingOpen; }
	std::string getDeviceFriendlyName() const;

	bool hasVideoSetting(eVideoSettingType settingType) const;
	bool getVideoSettingAsFloatFraction(eVideoSettingType settingType, float& outFloatFraction) const;
	bool setVideoSettingAsFloatFraction(eVideoSettingType settingType, float outFloatFraction, bool bForce= false);

	// VideoSourceComponent Interface
	virtual std::string getDevicePath() const override;
	virtual std::string getDeviceAPI() const override;
	virtual bool openVideoSource() override;
	virtual void closeVideoSource() override;
	virtual eVideoStreamingStatus getVideoStreamingStatus() const override;
	virtual bool getVideoModeName(std::string& outVideoModeName) const override;
	virtual bool getVideoPixelDimensions(int& outPixelWidth, int& outPixelHeight) const override;
	virtual bool isVideoSettingSupported(const eVideoSettingType property_type) const override;
	virtual bool setVideoSetting(const eVideoSettingType property_type, float desiredFraction) override;
	virtual bool getVideoSetting(const eVideoSettingType property_type, float& outFractionValue) const override;
	virtual bool getFrameRate(float& outFrameRate) const override;
	virtual bool getVideoColorimetry(VideoColorimetry& outColorimetry) const override;

	// -- USB Video Mode
	bool setDevicePath(const std::string& devicePath);
	size_t getAvailableVideoModesCount() const;
	bool getVideoModeProperties(size_t index, UsbVideoModeProperties& outProperties) const;
	int getVideoModeIndex() const;
	bool getVideoModeResolutionName(std::string& outResolution) const;
	bool getVideoModeFrameRateName(std::string& outFrameRate) const;
	bool getVideoModeFormatName(std::string& outFormat) const;
	bool setVideoModeByName(const std::string& videoModeName);
	bool setVideoModeByIndex(size_t index);
	bool getVideoModeNames(std::vector<std::string>& outVideoModeNames) const;
	bool getVideoResolutionNames(std::vector<std::string>& outVideoResolutionNames) const;
	bool getVideoFrameRateNames(std::vector<std::string>& outVideoFrameRateNames) const;
	bool getVideoFormatNames(std::vector<std::string>& outVideoFormatNames) const;
	bool setVideoModeToBestMatch(const std::string& resolution, const std::string& frameRate,
								 const std::string& format);

	// -- IUsbVideoDeviceListener ----
	virtual void notifyVideoDeviceDisconnected(const IUsbVideoDevice* device) override;
	virtual void notifyVideoModePropertiesChanged(const IUsbVideoDevice* device) override;
	virtual void notifyVideoFrameReceived(const UsbVideoFrameBuffer& bufferInfo) override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_currentDevicePathPropertyId;
	static const std::string k_currentFriendlyNamePropertyId;
	static const std::string k_currentVideoResolutionsPropertyId;
	static const std::string k_currentVideoFrameRatesPropertyId;
	static const std::string k_currentVideoFormatsPropertyId;
	static const std::string k_videoSettingPropertyPrefixes[(int)eVideoSettingType::COUNT];
	static const std::string k_videoSettingValidSuffix;
	static const std::string k_videoSettingFractionSuffix;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_resetToDefaultsFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames);
	virtual bool invokeFunction(const std::string& functionName) override;

protected:
	virtual eVideoStreamingStatus startVideoStreamInternal() override;
	virtual void stopVideoStreamInternal() override;

	void onDefinitionMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet) override;
	bool updateVideoMode();
	int findBestVideoModeIndex(int w, int h, int frameRate) const;
	bool handleVideoModeUpdated();
	void handleVideoModeSettingUpdated();
	void handleWantsActiveStream();
	void saveVideoSettingDefaultsFromCurrentMode();
	void restoreVideoSettingsToCurrentMode();
	void rebuildVideoModeOptionLists();

protected:
	IUsbVideoDevice* m_usbVideoDevice;
	USBVideoSettingsArray m_currentVideoSettings;
	USBVideoConstraintArray m_currentVideoConstraints;
	std::vector<std::string> m_cachedVideoResolutionNames;
	std::vector<std::string> m_cachedVideoFrameRateNames;
	std::vector<std::string> m_cachedVideoFormatNames;
	bool m_bDeviceChanged= false;
	bool m_bModeChanged= false;
	bool m_bSettingsChanged= false;
	bool m_bPendingStartStream= false;
	bool m_bWantsStreamActive= false;
	bool m_bPendingOpen= false;
};