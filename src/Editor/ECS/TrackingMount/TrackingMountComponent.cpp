#include "TrackingMountComponent.h"
#include "App.h"
#include "MathTypeConversion.h"
#include "MikanObject.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanTrackingMountTypes.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "StringUtils.h"
#include "TrackingMountObjectSystem.h"
#include "VRObjectSystem.h"

// -- TrackingMountDefinition -----
const std::string TrackingMountDefinition::k_devicePathPropertyId= "device_path";
const std::string TrackingMountDefinition::k_socketNamePropertyId= "socket_name";

TrackingMountDefinition::TrackingMountDefinition()
	: MikanComponentDefinition()
{
}

TrackingMountDefinition::TrackingMountDefinition(MikanTrackingMountID trackingMountId)
	: MikanComponentDefinition(trackingMountId, "")
{
}

configuru::Config TrackingMountDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_devicePathPropertyId]= m_devicePath;
	pt[k_socketNamePropertyId]= m_socketName;

	return pt;
}

void TrackingMountDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_devicePath= pt.get_or<std::string>(k_devicePathPropertyId, m_devicePath);
	m_socketName= pt.get_or<std::string>(k_socketNamePropertyId, m_socketName);
}

bool TrackingMountDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												 const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanTrackingMountComponentValues>();
	if (componentValues)
	{
		m_devicePath= componentValues->device_path.getUtf8Value();
		m_socketName= componentValues->socket_name.getUtf8Value();
	}

	return true;
}

void TrackingMountDefinition::setDevicePath(const std::string& devicePath)
{
	if (devicePath != m_devicePath)
	{
		m_devicePath= devicePath;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_devicePathPropertyId));
	}
}

void TrackingMountDefinition::setSocketName(const std::string& socketName)
{
	if (socketName != m_socketName)
	{
		m_socketName= socketName;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_socketNamePropertyId));
	}
}

// -- TrackingMountComponent -----
TrackingMountComponent::TrackingMountComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle TrackingMountComponent::getClientAPIValuesStructType() const
{
	return &MikanTrackingMountComponentValues::staticGetArchetype();
}

void TrackingMountComponent::init() { MikanComponent::init(); }

TrackingMountObjectSystemPtr TrackingMountComponent::getOwnerTrackingMountSystem() const
{
	return std::static_pointer_cast<TrackingMountObjectSystem>(getOwnerObject()->getOwnerSystem());
}

VRDeviceComponentPtr TrackingMountComponent::getVRDeviceComponent() const
{
	const std::string& vrDevicePath= getTrackingMountDefinition()->getDevicePath();
	auto vrObjectSystem= getObjectSystemOfType<VRObjectSystem>();

	return vrObjectSystem->getVRDeviceByPath(vrDevicePath);
}

VRDevicePoseViewPtr TrackingMountComponent::makePoseView(eVRDevicePoseSpace space) const
{
	TrackingMountDefinitionConstPtr trackingMount= getTrackingMountDefinition();
	assert(trackingMount);

	auto vrObjectSystem= getObjectSystemOfType<VRObjectSystem>();
	VRDeviceComponentPtr vrDeviceComponent= vrObjectSystem->getVRDeviceByPath(trackingMount->getDevicePath());

	if (vrDeviceComponent)
	{
		return vrDeviceComponent->makePoseView(space, trackingMount->getSocketName());
	}

	return VRDevicePoseViewPtr();
}

void TrackingMountComponent::deleteTrackingMount()
{
	TrackingMountDefinitionPtr trackingMountDefinition= getTrackingMountDefinition();
	if (trackingMountDefinition)
	{
		const MikanTrackingMountID trackingMountId= trackingMountDefinition->getTrackingMountId();
		getOwnerTrackingMountSystem()->removeObjectByPrimaryComponentId(trackingMountId);
	}
}

// -- IPropertyInterface ----
const std::string TrackingMountComponent::k_availableSocketNameListPropertyId= "available_socket_names";

void TrackingMountComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(TrackingMountDefinition::k_devicePathPropertyId,
																  MikanVariantType::STRING));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(TrackingMountDefinition::k_socketNamePropertyId,
																  MikanVariantType::STRING));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(TrackingMountComponent::k_availableSocketNameListPropertyId,
											 MikanVariantType::STRING_ARRAY)
			->setReadOnly()
			->setUIHidden());
}

bool TrackingMountComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == TrackingMountDefinition::k_devicePathPropertyId)
	{
		outValue= getTrackingMountDefinition()->getDevicePath();
		return true;
	}
	else if (propertyName == TrackingMountDefinition::k_socketNamePropertyId)
	{
		outValue= getTrackingMountDefinition()->getSocketName();
		return true;
	}
	else if (propertyName == TrackingMountComponent::k_availableSocketNameListPropertyId)
	{
		VRDeviceComponentPtr vrDeviceComponent= getVRDeviceComponent();
		outValue= vrDeviceComponent ? vrDeviceComponent->getSocketNames() : std::vector<std::string>();
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool TrackingMountComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == TrackingMountDefinition::k_devicePathPropertyId)
	{
		const std::string devicePath= inValue.getUtf8Value();
		getTrackingMountDefinition()->setDevicePath(devicePath);
		return true;
	}
	else if (propertyName == TrackingMountDefinition::k_socketNamePropertyId)
	{
		const std::string socketName= inValue.getUtf8Value();
		getTrackingMountDefinition()->setSocketName(socketName);
		return true;
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

void TrackingMountComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
													 const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(TrackingMountDefinition::k_devicePathPropertyId))
	{
		getDefinition()->notifyPropertyChanged(
			ConfigPropertyChangeSet().addPropertyName(k_availableSocketNameListPropertyId));
	}
}