#pragma once

#include "CommonConfig.h"

#include "ComponentFwd.h"
#include "MikanComponent.h"
#include "MikanTypeFwd.h"
#include "ObjectFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectSystemFwd.h"
#include "ProjectConfigConstants.h"
#include "TrackingVolumeComponent.h"

#include <memory>
#include <string>

class TrackingVolumeDefinition : public MikanComponentDefinition
{
public:
	TrackingVolumeDefinition();
	TrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	MarkerObjectSystemPtr getMarkerObjectSystem() const;

	virtual eTrackingVolumeType getTrackingVolumeType() const { return eTrackingVolumeType::INVALID; }
	inline MikanTrackingVolumeID getTrackingVolumeId() const { return getComponentId(); }

	static const std::string k_originMarkerIdPropertyId;
	inline MikanMarkerID getOriginMarkerId() const { return m_originMarkeId; }
	MarkerDefinitionConstPtr getOriginMarker() const;
	void setOriginMarkerId(MikanMarkerID arucoId);

private:
	MikanMarkerID m_originMarkeId;
};

class TrackingVolumeComponent : public MikanComponent
{
public:
	TrackingVolumeComponent(MikanObjectWeakPtr owner);
	virtual void init() override;

	inline static const std::string k_componentClassName= "TrackingVolumeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline TrackingVolumeDefinitionPtr getTrackingVolumeDefinition() const
	{
		return std::static_pointer_cast<TrackingVolumeDefinition>(m_definition);
	}
	inline eTrackingVolumeType getTrackingVolumeType() const
	{
		return getTrackingVolumeDefinition()->getTrackingVolumeType();
	}
	inline MikanMarkerID getOriginMarkerId() const { return getTrackingVolumeDefinition()->getOriginMarkerId(); }

	void deleteTrackingVolume();

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		MikanComponent::getFunctionDescriptors(outDescriptors);
	}
};