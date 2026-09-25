#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "ObjectFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "ProjectConfigConstants.h"
#include "VRDevicePoseView.h"

#include <memory>
#include <string>

class TrackingMountDefinition : public MikanComponentDefinition
{
public:
	TrackingMountDefinition();
	TrackingMountDefinition(MikanTrackingMountID trackingMountId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	inline MikanTrackingMountID getTrackingMountId() const { return getComponentId(); }

	static const std::string k_devicePathPropertyId;
	inline const std::string& getDevicePath() const { return m_devicePath; }
	void setDevicePath(const std::string& devicePath);

	static const std::string k_socketNamePropertyId;
	inline const std::string& getSocketName() const { return m_socketName; }
	void setSocketName(const std::string& socketName);

private:
	std::string m_devicePath;
	std::string m_socketName;
};

class TrackingMountComponent : public MikanComponent
{
public:
	TrackingMountComponent(MikanObjectWeakPtr owner);
	virtual void init() override;

	inline static const std::string k_componentClassName= "TrackingMountComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	TrackingMountObjectSystemPtr getOwnerTrackingMountSystem() const;
	inline TrackingMountDefinitionPtr getTrackingMountDefinition() const
	{
		return std::static_pointer_cast<TrackingMountDefinition>(m_definition);
	}
	VRDeviceComponentPtr getVRDeviceComponent() const;

	VRDevicePoseViewPtr makePoseView(eVRDevicePoseSpace space) const;
	void deleteTrackingMount();

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_availableSocketNameListPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		MikanComponent::getFunctionDescriptors(outDescriptors);
	}

protected:
	virtual void onDefinitionMarkedDirty(CommonConfigPtr configPtr,
										 const ConfigPropertyChangeSet& changedPropertySet) override;
};
