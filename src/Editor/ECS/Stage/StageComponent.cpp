#include "Colors.h"
#include "ColliderComponent.h"
#include "StageComponent.h"
#include "IMkScene.h"
#include "LuaMath.h"
#include "MainWindow.h"
#include "MathTypeConversion.h"
#include "MathUtility.h"
#include "MikanLineRenderer.h"
#include "MikanObject.h"
#include "MikanStageTypes.h"
#include "ProjectManager.h"
#include "SelectionComponent.h"
#include "TrackingVolumeQueries.h"
#include "TransformComponent.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <queue>

// -- StageComponentDefinition -----
const std::string StageComponentDefinition::k_trackingVolumeIdPropertyId= "tracking_volume_id";
const std::string StageComponentDefinition::k_stageBoundsMinPropertyId= "stage_bounds_min";
const std::string StageComponentDefinition::k_stageBoundsMaxPropertyId= "stage_bounds_max";

StageComponentDefinition::StageComponentDefinition(MikanStageID sceneId)
	: TransformComponentDefinition(sceneId, "", glm_transform_to_MikanTransform(GlmTransform()))
	, m_trackingVolumeId(INVALID_MIKAN_ID)
	, m_stageBoundsMinMM(MikanVector3f(0.0f))
	, m_stageBoundsMaxMM(MikanVector3f(0.0f))
{
}

configuru::Config StageComponentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	pt[k_trackingVolumeIdPropertyId]= m_trackingVolumeId;
	writeVector3f(pt, k_stageBoundsMinPropertyId.c_str(), m_stageBoundsMinMM);
	writeVector3f(pt, k_stageBoundsMaxPropertyId.c_str(), m_stageBoundsMaxMM);

	return pt;
}

void StageComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_trackingVolumeId= pt.get_or<int>(k_trackingVolumeIdPropertyId, INVALID_MIKAN_ID);
	readVector3f(pt, k_stageBoundsMinPropertyId.c_str(), m_stageBoundsMinMM);
	readVector3f(pt, k_stageBoundsMaxPropertyId.c_str(), m_stageBoundsMaxMM);
}

bool StageComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TransformComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanStageComponentValues>();
	if (componentValues)
	{
		m_trackingVolumeId= componentValues->tracking_volume_id;
	}

	return true;
}

void StageComponentDefinition::setTrackingVolumeId(MikanTrackingVolumeID trackingVolumeId)
{
	if (m_trackingVolumeId != trackingVolumeId)
	{
		m_trackingVolumeId= trackingVolumeId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_trackingVolumeIdPropertyId));
	}
}

void StageComponentDefinition::setStageBoundsMinMM(const MikanVector3f& minMM)
{
	m_stageBoundsMinMM= minMM;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_stageBoundsMinPropertyId));
}

void StageComponentDefinition::setStageBoundsMaxMM(const MikanVector3f& maxMM)
{
	m_stageBoundsMaxMM= maxMM;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_stageBoundsMaxPropertyId));
}

// -- StageComponent -----
StageComponent::StageComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

Serialization::StructTypeHandle StageComponent::getClientAPIValuesStructType() const
{
	return &MikanStageComponentValues::staticGetArchetype();
}

void StageComponent::setDefinition(MikanComponentDefinitionPtr definition)
{
	TransformComponent::setDefinition(definition);
}

void StageComponent::init() { TransformComponent::init(); }

void StageComponent::dispose() { TransformComponent::dispose(); }

void StageComponent::renderStageBounds(IMkGraphicsContext* graphicsContext, const glm::mat4& transform) const
{
	StageComponentDefinitionConstPtr definition= getStageComponentDefinitionConst();

	const glm::vec3 box_min= MikanVector3f_to_glm_vec3(definition->getStageBoundsMinMM()) * k_centimeters_to_meters;
	const glm::vec3 box_max= MikanVector3f_to_glm_vec3(definition->getStageBoundsMaxMM()) * k_centimeters_to_meters;

	drawTransformedBox(graphicsContext, transform, box_min, box_max, Colors::Yellow);
}

void StageComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 StageComponentDefinition::k_trackingVolumeIdPropertyId, MikanVariantType::INT)
								 ->setDefaultValue(-1));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(StageComponentDefinition::k_stageBoundsMinPropertyId,
																  MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(0, 0, 0)));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(StageComponentDefinition::k_stageBoundsMaxPropertyId,
																  MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(0, 0, 0)));
}

bool StageComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == StageComponentDefinition::k_trackingVolumeIdPropertyId)
	{
		outValue= (int)getStageComponentDefinitionConst()->getTrackingVolumeId();
		return true;
	}
	else if (propertyName == StageComponentDefinition::k_stageBoundsMinPropertyId)
	{
		outValue= getStageComponentDefinitionConst()->getStageBoundsMinMM();
		return true;
	}
	else if (propertyName == StageComponentDefinition::k_stageBoundsMaxPropertyId)
	{
		outValue= getStageComponentDefinitionConst()->getStageBoundsMaxMM();
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool StageComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == StageComponentDefinition::k_trackingVolumeIdPropertyId)
	{
		getStageComponentDefinition()->setTrackingVolumeId((MikanTrackingVolumeID)inValue.getIntValue());
		return true;
	}
	else if (propertyName == StageComponentDefinition::k_stageBoundsMinPropertyId)
	{
		getStageComponentDefinition()->setStageBoundsMinMM(inValue.getVector3fValue());
		return true;
	}
	else if (propertyName == StageComponentDefinition::k_stageBoundsMaxPropertyId)
	{
		getStageComponentDefinition()->setStageBoundsMaxMM(inValue.getVector3fValue());
		return true;
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

TrackingVolumeComponentConstPtr StageComponent::getTrackingVolumeConst() const
{
	MikanTrackingVolumeID trackingVolumeId= getStageComponentDefinitionConst()->getTrackingVolumeId();
	if (trackingVolumeId != INVALID_MIKAN_ID)
	{
		ProjectManagerPtr projectManager= getOwnerProjectManager();
		return TrackingVolumeQueries::getTrackingVolumeById(projectManager, trackingVolumeId);
	}

	return TrackingVolumeComponentConstPtr();
}

TrackingVolumeDefinitionConstPtr StageComponent::getTrackingVolumeDefinitionConst() const
{
	TrackingVolumeComponentConstPtr trackingVolume= getTrackingVolumeConst();
	if (trackingVolume)
	{
		return trackingVolume->getTrackingVolumeDefinition();
	}

	return TrackingVolumeDefinitionConstPtr();
}

MikanStageID StageComponent::getStageId() const { return getStageComponentDefinitionConst()->getComponentId(); }

void StageComponent::setTrackingVolumeId(MikanTrackingVolumeID volumeId)
{
	getStageComponentDefinition()->setTrackingVolumeId(volumeId);
}

// -- Lua Binding ----
void StageComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<StageComponent, TransformComponent>(StageComponent::k_componentClassName.c_str())
		.addProperty("stageId", [](StageComponent* c) -> int { return c->getStageId(); })
		.addProperty(
			"trackingVolumeId",
			[](StageComponent* c) -> int { return c->getStageComponentDefinitionConst()->getTrackingVolumeId(); },
			[](StageComponent* c, int v) { c->setTrackingVolumeId(static_cast<MikanTrackingVolumeID>(v)); })
		.addProperty("stageBoundsMin", [](StageComponent* c) -> LuaVec3f
					 { return LuaVec3f(c->getStageComponentDefinitionConst()->getStageBoundsMinMM()); })
		.addProperty("stageBoundsMax", [](StageComponent* c) -> LuaVec3f
					 { return LuaVec3f(c->getStageComponentDefinitionConst()->getStageBoundsMaxMM()); })
		.endClass();
}