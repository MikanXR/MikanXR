#pragma once

#include "TrackingVolumeComponent.h"
#include "MarkerTrackingVolumeComponent.h"

class MarkerTrackingVolumeDefinition : public TrackingVolumeDefinition
{
public:
	MarkerTrackingVolumeDefinition();
	MarkerTrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId);

	virtual eTrackingVolumeType getTrackingVolumeType() const override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;
};

class MarkerTrackingVolumeComponent : public TrackingVolumeComponent
{
public:
	MarkerTrackingVolumeComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "MarkerTrackingVolumeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline MarkerTrackingVolumeDefinitionPtr getMarkerTrackingVolumeDefinition() const
	{
		return std::static_pointer_cast<MarkerTrackingVolumeDefinition>(m_definition);
	}

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;
};