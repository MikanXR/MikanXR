#pragma once

#include "ComponentFwd.h"
#include "ColliderQuery.h"
#include "ObjectFwd.h"
#include "MikanStageTypes.h"
#include "MikanRendererFwd.h"
#include "ProjectConfigConstants.h"
#include "SceneFwd.h"
#include "TransformComponent.h"

#include <functional>

class StageComponentDefinition : public TransformComponentDefinition
{
public:
	StageComponentDefinition()= default;
	StageComponentDefinition(MikanStageID sceneId);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_trackingVolumeIdPropertyId;
	MikanTrackingVolumeID getTrackingVolumeId() const { return m_trackingVolumeId; }
	void setTrackingVolumeId(MikanTrackingVolumeID volumeId);

	static const std::string k_stageBoundsMinPropertyId;
	MikanVector3f getStageBoundsMinMM() const { return m_stageBoundsMinMM; }
	void setStageBoundsMinMM(const MikanVector3f& minMM);

	static const std::string k_stageBoundsMaxPropertyId;
	MikanVector3f getStageBoundsMaxMM() const { return m_stageBoundsMaxMM; }
	void setStageBoundsMaxMM(const MikanVector3f& maxMM);

protected:
	MikanTrackingVolumeID m_trackingVolumeId= INVALID_MIKAN_ID;
	MikanVector3f m_stageBoundsMinMM;
	MikanVector3f m_stageBoundsMaxMM;
};

class StageComponent final : public TransformComponent
{
public:
	StageComponent(MikanObjectWeakPtr owner);

	inline StageComponentDefinitionConstPtr getStageComponentDefinitionConst() const
	{
		return std::static_pointer_cast<const StageComponentDefinition>(m_definition);
	}
	inline StageComponentDefinitionPtr getStageComponentDefinition()
	{
		return std::static_pointer_cast<StageComponentDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "StageComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	// -- TransformComponent ----
	// A stage is a root: its transform is what defines stage space against the
	// editor's world space, so it has nothing to hang from
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override { return false; }

	TrackingVolumeComponentConstPtr getTrackingVolumeConst() const;
	TrackingVolumeDefinitionConstPtr getTrackingVolumeDefinitionConst() const;

	// -- MikanComponent ----
	virtual void setDefinition(MikanComponentDefinitionPtr definition) override;
	virtual void init() override;
	virtual void dispose() override;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- StageComponent ----
	MikanStageID getStageId() const;
	void setTrackingVolumeId(MikanTrackingVolumeID volumeId);
	void renderStageBounds(class IMkGraphicsContext* graphicsContext, const glm::mat4& transform) const;

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);
};