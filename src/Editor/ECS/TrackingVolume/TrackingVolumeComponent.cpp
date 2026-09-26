#include "TrackingVolumeComponent.h"
#include "App.h"
#include "MathTypeConversion.h"
#include "MarkerObjectSystem.h"
#include "MikanObject.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "SelectionComponent.h"
#include "StringUtils.h"
#include "TrackingVolumeQueries.h"

// -- TrackingVolumeDefinition -----
const std::string TrackingVolumeDefinition::k_originMarkerIdPropertyId= "origin_marker_id";

TrackingVolumeDefinition::TrackingVolumeDefinition()
	: MikanComponentDefinition()
	, m_originMarkeId(INVALID_MIKAN_ID)
{
}

TrackingVolumeDefinition::TrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId)
	: MikanComponentDefinition(trackingVolumeId, "")
	, m_originMarkeId(INVALID_MIKAN_ID)
{
}

configuru::Config TrackingVolumeDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_originMarkerIdPropertyId.c_str()]= m_originMarkeId;

	return pt;
}

void TrackingVolumeDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_originMarkeId= pt.get_or<MikanMarkerID>(k_originMarkerIdPropertyId.c_str(), m_originMarkeId);
}

bool TrackingVolumeDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanTrackingVolumeComponentValues>();
	if (componentValues)
	{
		m_originMarkeId= componentValues->origin_marker_id;
	}

	return true;
}

MarkerObjectSystemPtr TrackingVolumeDefinition::getMarkerObjectSystem() const
{
	return getOwnerComponent()->getObjectSystemOfType<MarkerObjectSystem>();
}

void TrackingVolumeDefinition::setOriginMarkerId(MikanMarkerID arucoId)
{
	if (arucoId != m_originMarkeId)
	{
		m_originMarkeId= arucoId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_originMarkerIdPropertyId));
	}
}

MarkerDefinitionConstPtr TrackingVolumeDefinition::getOriginMarker() const
{
	if (m_originMarkeId != INVALID_MIKAN_ID)
	{
		return getMarkerObjectSystem()->getTypedDefinition()->getDefinitionById(m_originMarkeId);
	}

	return MarkerDefinitionConstPtr();
}

// -- TrackingVolumeComponent -----
TrackingVolumeComponent::TrackingVolumeComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle TrackingVolumeComponent::getClientAPIValuesStructType() const
{
	return &MikanTrackingVolumeComponentValues::staticGetArchetype();
}

void TrackingVolumeComponent::init() { MikanComponent::init(); }

void TrackingVolumeComponent::deleteTrackingVolume()
{
	TrackingVolumeDefinitionPtr def= getTrackingVolumeDefinition();
	if (def)
	{
		ProjectManagerPtr projectManager= getOwnerProjectManager();
		TrackingVolumeQueries::removeTrackingVolume(projectManager, def->getTrackingVolumeId());
	}
}

// -- IPropertyInterface ----
void TrackingVolumeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(TrackingVolumeDefinition::k_originMarkerIdPropertyId,
																  MikanVariantType::INT)
								 ->setDefaultValue(-1));
}

bool TrackingVolumeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == TrackingVolumeDefinition::k_originMarkerIdPropertyId)
	{
		outValue= getTrackingVolumeDefinition()->getOriginMarkerId();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool TrackingVolumeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == TrackingVolumeDefinition::k_originMarkerIdPropertyId)
	{
		MikanMarkerID markerId= inValue.getIntValue();
		getTrackingVolumeDefinition()->setOriginMarkerId(markerId);
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}