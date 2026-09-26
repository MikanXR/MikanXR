#include "AppStage.h"
#include "CameraObjectSystem.h"
#include "DMXFixtureComponent.h"
#include "DMXObjectSystem.h"
#include "LightFixtureCalibration\AppStage_LightFixtureCalibration.h"
#include "MikanLightTypes.h"
#include "MikanObject.h"
#include "MikanObjectSystem.h"
#include "MikanVariantTypes.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ModalSelectCamera/ModalDialog_SelectCamera.h"
#include "ProjectManager.h"
#include "StringUtils.h"
#include "StageObjectSystem.h"
#include "StageComponent.h"
#include "TrackingVolumeComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- DMXFixtureComponentDefinition -----
const std::string DMXFixtureComponentDefinition::k_ownerStageIdPropertyId= "stage_id";
const std::string DMXFixtureComponentDefinition::k_dmxUniversePropertyId= "dmx_universe";
const std::string DMXFixtureComponentDefinition::k_dmxStartChannelPropertyId= "dmx_start_channel";
const std::string DMXFixtureComponentDefinition::k_dmxChannelCountPropertyId= "dmx_channel_count";
const std::string DMXFixtureComponentDefinition::k_isDisabledPropertyId= "is_disabled";
const std::string DMXFixtureComponentDefinition::k_maxWattagePropertyId= "max_wattage";
const std::string DMXFixtureComponentDefinition::k_lumensPerWattPropertyId= "lumens_per_watt";

DMXFixtureComponentDefinition::DMXFixtureComponentDefinition()
	: TransformComponentDefinition()
{
}

DMXFixtureComponentDefinition::DMXFixtureComponentDefinition(MikanLightID lightId)
	: TransformComponentDefinition(lightId)
{
}

configuru::Config DMXFixtureComponentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	pt[k_ownerStageIdPropertyId]= m_stageId;
	pt[k_dmxUniversePropertyId]= m_dmxUniverse;
	pt[k_dmxStartChannelPropertyId]= m_dmxStartChannel;
	pt[k_dmxChannelCountPropertyId]= m_dmxChannelCount;
	pt[k_isDisabledPropertyId]= m_bIsDisabled;
	pt[k_maxWattagePropertyId]= m_maxWattage;
	pt[k_lumensPerWattPropertyId]= m_lumensPerWatt;

	return pt;
}

void DMXFixtureComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_stageId= pt.get_or<MikanStageID>(k_ownerStageIdPropertyId, m_stageId);
	m_dmxUniverse= pt.get_or<uint16_t>(k_dmxUniversePropertyId, 1);
	m_dmxStartChannel= pt.get_or<uint16_t>(k_dmxStartChannelPropertyId, 1);
	m_dmxChannelCount= pt.get_or<uint16_t>(k_dmxChannelCountPropertyId, 3);
	m_bIsDisabled= pt.get_or<bool>(k_isDisabledPropertyId, false);
	m_maxWattage= pt.get_or<float>(k_maxWattagePropertyId, m_maxWattage);
	m_lumensPerWatt= pt.get_or<float>(k_lumensPerWattPropertyId, m_lumensPerWatt);
}

bool DMXFixtureComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TransformComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* values= initParams.getTypedPointer<MikanDMXFixtureComponentValues>();
	if (values)
	{
		m_stageId= values->stage_id;
		m_dmxUniverse= values->dmx_universe;
		m_dmxStartChannel= values->dmx_start_channel;
		m_dmxChannelCount= values->dmx_channel_count;
		m_bIsDisabled= values->is_disabled;
		m_maxWattage= values->max_wattage;
		m_lumensPerWatt= values->lumens_per_watt;

		// Make sure our parent is always the stage component (if a stage was given)
		if (m_stageId != INVALID_MIKAN_ID)
		{
			m_parentTransformId= m_stageId;
		}
	}

	if (m_stageId == INVALID_MIKAN_ID)
	{
		// If no owning stage was specified, use the first one
		auto stageSystem= ownerObjectSystem->getObjectSystemOfType<StageObjectSystem>();

		m_stageId= stageSystem->getFirstComponentId();
	}

	return true;
}

void DMXFixtureComponentDefinition::setOwnerStageId(MikanStageID stageId)
{
	if (stageId != m_stageId)
	{
		m_stageId= stageId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_ownerStageIdPropertyId));
	}
}

void DMXFixtureComponentDefinition::setDMXUniverse(uint16_t universe)
{
	if (m_dmxUniverse != universe)
	{
		m_dmxUniverse= universe;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_dmxUniversePropertyId));
	}
}

void DMXFixtureComponentDefinition::setDMXStartChannel(uint16_t startChannel)
{
	if (m_dmxStartChannel != startChannel)
	{
		m_dmxStartChannel= startChannel;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_dmxStartChannelPropertyId));
	}
}

void DMXFixtureComponentDefinition::setIsDisabled(bool flag)
{
	if (m_bIsDisabled != flag)
	{
		m_bIsDisabled= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_isDisabledPropertyId));
	}
}

void DMXFixtureComponentDefinition::setMaxWattage(float watts)
{
	if (m_maxWattage != watts)
	{
		m_maxWattage= watts;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_maxWattagePropertyId));
	}
}

void DMXFixtureComponentDefinition::setLumensPerWatt(float lumensPerWatt)
{
	if (m_lumensPerWatt != lumensPerWatt)
	{
		m_lumensPerWatt= lumensPerWatt;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_lumensPerWattPropertyId));
	}
}

void DMXFixtureComponentDefinition::setDMXChannelCount(uint16_t count)
{
	if (m_dmxChannelCount != count)
	{
		m_dmxChannelCount= count;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_dmxChannelCountPropertyId));
	}
}

// -- DMXFixtureComponent -----
const std::string DMXFixtureComponent::k_triangulateLightFunctionId= "triangulate_light";

DMXFixtureComponent::DMXFixtureComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

void DMXFixtureComponent::init()
{
	TransformComponent::init();

	m_dmxObjectSystem= getObjectSystemOfType<DMXObjectSystem>();
}

// -- TransformComponent ----
bool DMXFixtureComponent::canAttachToParent(TransformComponentConstPtr newParentComponent) const
{
	return std::dynamic_pointer_cast<const StageComponent>(newParentComponent) != nullptr;
}

StageComponentConstPtr DMXFixtureComponent::getOwnerStageComponent() const
{
	MikanStageID stageId= getDMXFixtureDefinition()->getOwnerStageId();

	return getObjectSystemOfType<StageObjectSystem>()->getStageById(stageId);
}

eTrackingVolumeType DMXFixtureComponent::getTrackingVolumeType() const
{
	StageComponentConstPtr ownerStage= getOwnerStageComponent();
	if (ownerStage != nullptr)
	{
		TrackingVolumeDefinitionConstPtr trackingVolume= ownerStage->getTrackingVolumeDefinitionConst();
		if (trackingVolume != nullptr)
		{
			return trackingVolume->getTrackingVolumeType();
		}
	}
	return eTrackingVolumeType::INVALID;
}

// -- IEntityAccessor --
Serialization::StructTypeHandle DMXFixtureComponent::getClientAPIValuesStructType() const
{
	return &MikanDMXFixtureComponentValues::staticGetArchetype();
}

// -- IPropertyInterface --
void DMXFixtureComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	// A fixture belongs to the stage that created it for life. Its group and
	// sequence memberships live in that stage, so moving it would leave holes in
	// them, and the id is set once at creation rather than edited.
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 DMXFixtureComponentDefinition::k_ownerStageIdPropertyId, MikanVariantType::INT)
								 ->setReadOnly()
								 ->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		DMXFixtureComponentDefinition::k_dmxUniversePropertyId, MikanVariantType::USHORT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		DMXFixtureComponentDefinition::k_dmxStartChannelPropertyId, MikanVariantType::USHORT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 DMXFixtureComponentDefinition::k_dmxChannelCountPropertyId, MikanVariantType::USHORT)
								 ->setReadOnly());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXFixtureComponentDefinition::k_isDisabledPropertyId,
																  MikanVariantType::BOOL));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(DMXFixtureComponentDefinition::k_maxWattagePropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		DMXFixtureComponentDefinition::k_lumensPerWattPropertyId, MikanVariantType::FLOAT));
}

bool DMXFixtureComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	DMXFixtureComponentDefinitionPtr def= getDMXFixtureDefinition();

	if (propertyName == DMXFixtureComponentDefinition::k_ownerStageIdPropertyId)
	{
		outValue= def->getOwnerStageId();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_dmxUniversePropertyId)
	{
		outValue= def->getDMXUniverse();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_dmxStartChannelPropertyId)
	{
		outValue= def->getDMXStartChannel();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_dmxChannelCountPropertyId)
	{
		outValue= def->getDMXChannelCount();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_isDisabledPropertyId)
	{
		outValue= def->getIsDisabled();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_maxWattagePropertyId)
	{
		outValue= def->getMaxWattage();
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_lumensPerWattPropertyId)
	{
		outValue= def->getLumensPerWatt();
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool DMXFixtureComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	DMXFixtureComponentDefinitionPtr def= getDMXFixtureDefinition();

	// No owner stage id branch: a fixture never changes stage, and the actions
	// that create one reach setOwnerStageId on the definition directly
	if (propertyName == DMXFixtureComponentDefinition::k_dmxUniversePropertyId)
	{
		def->setDMXUniverse(inValue.getUShortValue());
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_dmxStartChannelPropertyId)
	{
		def->setDMXStartChannel(inValue.getUShortValue());
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_isDisabledPropertyId)
	{
		def->setIsDisabled(inValue.getBoolValue());
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_maxWattagePropertyId)
	{
		def->setMaxWattage(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == DMXFixtureComponentDefinition::k_lumensPerWattPropertyId)
	{
		def->setLumensPerWatt(inValue.getFloatValue());
		return true;
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void DMXFixtureComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<FunctionDescriptor>(k_triangulateLightFunctionId, "Triangulate Light"));
}

bool DMXFixtureComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == DMXFixtureComponent::k_triangulateLightFunctionId)
	{
		triangulateLight();
		return true;
	}

	return TransformComponent::invokeFunction(functionName);
}

void DMXFixtureComponent::triangulateLight()
{
	AppStage* currentAppStage= getOwnerEditorWindow()->getCurrentAppStage();

	switch (getTrackingVolumeType())
	{
	case eTrackingVolumeType::vr:
	{
		AppStage* currentAppStage= getOwnerEditorWindow()->getCurrentAppStage();

		ModalDialog_SelectCamera::selectCamera(
			currentAppStage,
			[this](MikanCameraID cameraId)
			{
				// Show Light Triangulation Tool
				AppStage_LightFixtureCalibration* anchorTriangulation=
					getOwnerEditorWindow()->pushAppStageOfType<AppStage_LightFixtureCalibration>();
				CameraComponentPtr camera= getObjectSystemOfType<CameraObjectSystem>()->getCameraById(cameraId);

				anchorTriangulation->setSourceCamera(camera);
				anchorTriangulation->setTargetFixture(getSelfPtr<DMXFixtureComponent>());
			});
	}
	break;
	case eTrackingVolumeType::marker:
		ModalDialog_MessageBox::showMessageBox(currentAppStage,
											   "Unable to triangulate lights using marker based tracking.");
		break;
	default:
		ModalDialog_MessageBox::showMessageBox(
			currentAppStage,
			"Stage missing tracking volume. Please assign a tracking volume to the stage this camera is attached to.");
		break;
	}
}

// -- Lua Binding --
void DMXFixtureComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<DMXFixtureComponent, TransformComponent>(DMXFixtureComponent::k_componentClassName.c_str())
		.addProperty(
			"dmxUniverse", [](DMXFixtureComponent* c) -> int
			{ return static_cast<int>(c->getDMXFixtureDefinition()->getDMXUniverse()); },
			[](DMXFixtureComponent* c, int v)
			{ c->getDMXFixtureDefinition()->setDMXUniverse(static_cast<uint16_t>(v)); })
		.addProperty(
			"dmxStartChannel", [](DMXFixtureComponent* c) -> int
			{ return static_cast<int>(c->getDMXFixtureDefinition()->getDMXStartChannel()); },
			[](DMXFixtureComponent* c, int v)
			{ c->getDMXFixtureDefinition()->setDMXStartChannel(static_cast<uint16_t>(v)); })
		.addProperty(
			"isDisabled", [](DMXFixtureComponent* c) -> bool { return c->getDMXFixtureDefinition()->getIsDisabled(); },
			[](DMXFixtureComponent* c, bool v) { c->getDMXFixtureDefinition()->setIsDisabled(v); })
		.addProperty("dmxChannelCount", [](DMXFixtureComponent* c) -> int
					 { return static_cast<int>(c->getDMXFixtureDefinition()->getDMXChannelCount()); })
		.addProperty("ownerStageId",
					 [](DMXFixtureComponent* c) -> int { return c->getDMXFixtureDefinition()->getOwnerStageId(); })
		.addFunction("triangulateLight", [](DMXFixtureComponent* c) { c->triangulateLight(); })
		.addFunction("getOwnerStage", [](DMXFixtureComponent* c) -> StageComponent*
					 { return const_cast<StageComponent*>(c->getOwnerStageComponent().get()); })
		.endClass();
}
