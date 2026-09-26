#include "MarkerTrackingVolumeComponent.h"
#include "MikanTrackingVolumeTypes.h"

// -- MarkerTrackingVolumeDefinition -----
MarkerTrackingVolumeDefinition::MarkerTrackingVolumeDefinition()
	: TrackingVolumeDefinition()
{
}

MarkerTrackingVolumeDefinition::MarkerTrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId)
	: TrackingVolumeDefinition(trackingVolumeId)
{
}

eTrackingVolumeType MarkerTrackingVolumeDefinition::getTrackingVolumeType() const
{
	return eTrackingVolumeType::marker;
}

bool MarkerTrackingVolumeDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
														const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TrackingVolumeDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanMarkerTrackingVolumeComponentValues>();
	if (componentValues)
	{
		// No additional fields - just call parent and return
	}

	return true;
}

// -- MarkerTrackingVolumeComponent -----
MarkerTrackingVolumeComponent::MarkerTrackingVolumeComponent(MikanObjectWeakPtr owner)
	: TrackingVolumeComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle MarkerTrackingVolumeComponent::getClientAPIValuesStructType() const
{
	return &MikanMarkerTrackingVolumeComponentValues::staticGetArchetype();
}
