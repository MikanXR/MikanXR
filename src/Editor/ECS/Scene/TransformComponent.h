#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "LuaMath.h"
#include "MikanComponent.h"
#include "MikanMathTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "Transform.h"
#include "IMkSceneRenderable.h"
#include "ObjectFwd.h"
#include "SceneFwd.h"

#include <glm/gtc/matrix_transform.hpp>

#include <vector>

using TransformComponentList= std::vector<TransformComponentWeakPtr>;
using TransformComponentVisitor= std::function<void(TransformComponent* transformComponentPtr)>;
using TransformComponentConstVisitor= std::function<void(const TransformComponent* transformComponentPtr)>;

class TransformComponentDefinition : public MikanComponentDefinition
{
public:
	TransformComponentDefinition();
	TransformComponentDefinition(int componentId);
	TransformComponentDefinition(int componentId, const std::string& componentName, const MikanTransform& xform);

	virtual bool isAutoNotifyTransformPropertyChangeDisabled() { return false; }
	void sendTransformPropertyChangeNotification();
	void sendScalePropertyChangeNotification();
	void sendRotationPropertyChangeNotification();
	void sendPositionPropertyChangeNotification();

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	const glm::mat4 getRelativeMat4() const;
	void setRelativeMat4(const glm::mat4& xform);

	const GlmTransform getRelativeTransform() const;
	void setRelativeTransform(const GlmTransform& transform);

	static const std::string k_parentTransformIdPropertyId;
	const MikanTransformID getParentTransformId() const { return m_parentTransformId; }
	void setParentTransformId(MikanTransformID parentId);

	static const std::string k_relativeScalePropertyId;
	const MikanVector3f getRelativeScale() const { return m_relativeTransform.scale; }
	void setRelativeScale(const MikanVector3f& scale);

	static const std::string k_relativeQuaternionPropertyId;
	const MikanQuatf getRelativeRotation() const { return m_relativeTransform.rotation; }
	void setRelativeRotation(const MikanQuatf& quat);

	static const std::string k_relativePositionPropertyId;
	const MikanVector3f getRelativePosition() const { return m_relativeTransform.position; }
	void setRelativePosition(const MikanVector3f& translation);

protected:
	MikanTransformID m_parentTransformId= INVALID_MIKAN_ID;
	MikanTransform m_relativeTransform;
};

class TransformComponent : public MikanComponent
{
public:
	TransformComponent(MikanObjectWeakPtr owner);

	virtual void postInit() override;
	virtual void dispose() override;

	inline static const std::string k_componentClassName= "TransformComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	virtual void setDefinition(MikanComponentDefinitionPtr config) override;
	inline TransformComponentDefinitionConstPtr getTransformComponentDefinitionConst() const
	{
		return std::static_pointer_cast<const TransformComponentDefinition>(m_definition);
	}
	inline TransformComponentDefinitionPtr getTransformComponentDefinition()
	{
		return std::static_pointer_cast<TransformComponentDefinition>(m_definition);
	}

	inline MikanTransformID getTransformId() const { return getComponentId(); }
	inline MikanTransformID getParentTransformId() const
	{
		return getTransformComponentDefinitionConst()->getParentTransformId();
	}
	inline TransformComponentPtr getParentTransformComponent() const { return m_parentComponent.lock(); }
	inline const TransformComponentList& getChildTransformComponents() const { return m_childComponents; }

	// Which parents this component type accepts. The base accepts any, so the
	// internal attachments (colliders, meshes, gizmo handles, VR device sockets)
	// are unaffected. The authored types override it to pin the scene hierarchy.
	// attachToComponent is the only enforcement point, and every path that sets a
	// parent - project load, the outliner, the property setter - routes through it.
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const { return true; }

	bool attachToComponent(TransformComponentPtr newParentComponent);

	// Reparent while leaving the component where it is in the world, by
	// recomputing the relative transform against the new parent.
	bool reparentPreservingWorldTransform(TransformComponentPtr newParentComponent);

	enum class eDetachReason : int
	{
		selfDisposed,
		parentDisposed,
		detachFromParent,
		attachToNewParent
	};
	void detachFromParent(eDetachReason reason);

	void setRelativeTransform(const GlmTransform& newRelativeXform);
	void setRelativePosition(const glm::vec3& position);
	void setRelativeRotation(const glm::quat& quat);
	void setRelativeScale(const glm::vec3& scale);

	inline const GlmTransform& getRelativeTransform() const { return m_relativeTransform; }
	inline const glm::vec3& getRelativePosition() const { return m_relativeTransform.getPosition(); }
	inline const glm::quat& getRelativeRotation() const { return m_relativeTransform.getRotation(); }
	inline const glm::vec3& getRelativeScale() const { return m_relativeTransform.getScale(); }

	void setWorldTransform(const glm::mat4& newWorldXform);
	inline const glm::mat4& getWorldTransform() const { return m_worldTransform; }
	const glm::vec3 getWorldLocation() const;

	void visitAllTransformComponents(TransformComponentVisitor visitor);
	void visitAllTransformComponentsConst(TransformComponentConstVisitor visitor) const;

	inline IMkSceneRenderablePtr getGlSceneRenderable() const { return m_sceneRenderable; }
	inline IMkSceneRenderableConstPtr getGlSceneRenderableConst() const { return m_sceneRenderable; }

	// The model matrix the attached renderable should draw with. The world transform on its own for
	// most components, but a shape bakes its authored size into the scale, so it overrides this.
	// Every write of the renderable's model matrix goes through here: a component that baked
	// something extra in one place and let the base class write the plain world transform in
	// another would flip between the two depending on which ran last that frame.
	virtual glm::mat4 computeSceneRenderableModelMatrix() const { return m_worldTransform; }
	void refreshSceneRenderableModelMatrix();

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_relativeRotationPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);

	// -- Lua Binding ----
	static void bindLuaFunctions(struct lua_State* L);

protected:
	enum class eTransformChangeType : int
	{
		recomputeWorldTransformAndPropogate,
		propogateWorldTransform,
	};

	virtual void onDetachedFromParent(TransformComponentPtr oldParent, eDetachReason reason);
	virtual void onAttachedToNewParent(TransformComponentPtr newParent);
	void propogateWorldTransformChange(eTransformChangeType reason);

	GlmTransform m_relativeTransform;
	glm::mat4 m_worldTransform;
	TransformComponentWeakPtr m_parentComponent;
	TransformComponentList m_childComponents;
	IMkSceneRenderablePtr m_sceneRenderable;
};