#include "TransformComponent.h"
#include "Logger.h"
#include "MikanObject.h"
#include "MathGLM.h"
#include "MathTypeConversion.h"
#include "MikanTransformTypes.h"
#include "Transform.h"

#include <glm/gtx/matrix_decompose.hpp>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include <queue>

// -- ModelStencilConfig -----
const std::string TransformComponentDefinition::k_parentTransformIdPropertyId= "parent_transform_id";
const std::string TransformComponentDefinition::k_relativeScalePropertyId= "relative_scale";
const std::string TransformComponentDefinition::k_relativeQuaternionPropertyId= "relative_quaternion";
const std::string TransformComponentDefinition::k_relativePositionPropertyId= "relative_position";

TransformComponentDefinition::TransformComponentDefinition()
	: MikanComponentDefinition()
{
	m_relativeTransform.scale= {1.f, 1.f, 1.f};
	m_relativeTransform.rotation= {1.f, 0.f, 0.f, 0.f};
	m_relativeTransform.position= {0.f, 0.f, 0.f};
}

TransformComponentDefinition::TransformComponentDefinition(int componentId)
	: MikanComponentDefinition(componentId, "")
{
	m_relativeTransform.scale= {1.f, 1.f, 1.f};
	m_relativeTransform.rotation= {1.f, 0.f, 0.f, 0.f};
	m_relativeTransform.position= {0.f, 0.f, 0.f};
}

TransformComponentDefinition::TransformComponentDefinition(int componentId, const std::string& componentName,
														   const MikanTransform& xform)
	: MikanComponentDefinition(componentId, componentName)
{
	m_relativeTransform= xform;
}

configuru::Config TransformComponentDefinition::writeToJSON()
{
	configuru::Config pt= MikanComponentDefinition::writeToJSON();

	pt[k_parentTransformIdPropertyId]= m_parentTransformId;
	writeVector3f(pt, k_relativeScalePropertyId.c_str(), m_relativeTransform.scale);
	writeQuatf(pt, k_relativeQuaternionPropertyId.c_str(), m_relativeTransform.rotation);
	writeVector3f(pt, k_relativePositionPropertyId.c_str(), m_relativeTransform.position);

	return pt;
}

void TransformComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	MikanComponentDefinition::readFromJSON(pt);

	m_parentTransformId= pt.get_or<int>(k_parentTransformIdPropertyId, -1);
	m_relativeTransform.scale= {1.f, 1.f, 1.f};
	m_relativeTransform.rotation= {1.f, 0.f, 0.f, 0.f};
	m_relativeTransform.position= {0.f, 0.f, 0.f};

	readVector3f(pt, k_relativeScalePropertyId.c_str(), m_relativeTransform.scale);
	readQuatf(pt, k_relativeQuaternionPropertyId.c_str(), m_relativeTransform.rotation);
	readVector3f(pt, k_relativePositionPropertyId.c_str(), m_relativeTransform.position);
}

bool TransformComponentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													  const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!MikanComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanTransformComponentValues>();
	if (componentValues)
	{
		m_parentTransformId= componentValues->parent_transform_id;
		m_relativeTransform.scale= componentValues->relative_scale;
		m_relativeTransform.position= componentValues->relative_position;
		m_relativeTransform.rotation= componentValues->relative_quaternion;
	}

	return true;
}

const glm::mat4 TransformComponentDefinition::getRelativeMat4() const { return getRelativeTransform().getMat4(); }

void TransformComponentDefinition::setRelativeMat4(const glm::mat4& mat4) { setRelativeTransform(GlmTransform(mat4)); }

const GlmTransform TransformComponentDefinition::getRelativeTransform() const
{
	return GlmTransform(MikanVector3f_to_glm_vec3(m_relativeTransform.position),
						MikanQuatf_to_glm_quat(m_relativeTransform.rotation),
						MikanVector3f_to_glm_vec3(m_relativeTransform.scale));
}

void TransformComponentDefinition::sendTransformPropertyChangeNotification()
{
	notifyPropertyChanged(ConfigPropertyChangeSet()
							  .addPropertyName(k_relativeScalePropertyId)
							  .addPropertyName(k_relativeQuaternionPropertyId)
							  .addPropertyName(k_relativePositionPropertyId));
}

void TransformComponentDefinition::sendScalePropertyChangeNotification()
{
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_relativeScalePropertyId));
}

void TransformComponentDefinition::sendRotationPropertyChangeNotification()
{
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_relativeQuaternionPropertyId));
}

void TransformComponentDefinition::sendPositionPropertyChangeNotification()
{
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_relativePositionPropertyId));
}

void TransformComponentDefinition::setRelativeTransform(const GlmTransform& transform)
{
	glm::mat4 xform= transform.getMat4();

	m_relativeTransform.position= glm_vec3_to_MikanVector3f(transform.getPosition());
	m_relativeTransform.rotation= glm_quat_to_MikanQuatf(transform.getRotation());
	m_relativeTransform.scale= glm_vec3_to_MikanVector3f(transform.getScale());

	if (!isAutoNotifyTransformPropertyChangeDisabled())
	{
		sendTransformPropertyChangeNotification();
	}
}

void TransformComponentDefinition::setParentTransformId(MikanTransformID parentId)
{
	if (m_parentTransformId != parentId)
	{
		m_parentTransformId= parentId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_parentTransformIdPropertyId));
	}
}

void TransformComponentDefinition::setRelativeScale(const MikanVector3f& scale)
{
	m_relativeTransform.scale= scale;

	if (!isAutoNotifyTransformPropertyChangeDisabled())
	{
		sendScalePropertyChangeNotification();
	}
}

void TransformComponentDefinition::setRelativeRotation(const MikanQuatf& quat)
{
	m_relativeTransform.rotation= quat;

	if (!isAutoNotifyTransformPropertyChangeDisabled())
	{
		sendRotationPropertyChangeNotification();
	}
}

void TransformComponentDefinition::setRelativePosition(const MikanVector3f& translation)
{
	m_relativeTransform.position= translation;

	if (!isAutoNotifyTransformPropertyChangeDisabled())
	{
		sendPositionPropertyChangeNotification();
	}
}

// -- Scene Component -----
TransformComponent::TransformComponent(MikanObjectWeakPtr owner)
	: MikanComponent(owner)
	, m_relativeTransform()
	, m_worldTransform(glm::mat4(1.f))
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle TransformComponent::getClientAPIValuesStructType() const
{
	return &MikanTransformComponentValues::staticGetArchetype();
}

void TransformComponent::postInit()
{
	MikanComponent::postInit();

	// If we have a parent transform ID, try to attach to it
	TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();
	if (definitionPtr)
	{
		MikanTransformID parentId= definitionPtr->getParentTransformId();
		if (parentId != INVALID_MIKAN_ID)
		{
			TransformComponentPtr parentComponent=
				getOwnerProjectManager()->getTypedComponentById<TransformComponent>(parentId);

			if (parentComponent)
			{
				attachToComponent(parentComponent);
			}
			else
			{
				MIKAN_LOG_ERROR("TransformComponent::postInit") << "TransformComponent with ID " << getComponentId()
																<< " has invalid parent transform ID " << parentId;
			}
		}
	}
}

void TransformComponent::dispose()
{
	// Capture grandparent before any mutations clear m_parentComponent
	TransformComponentPtr grandparent= m_parentComponent.lock();

	// Reparent all children to grandparent instead of orphaning them
	while (m_childComponents.size() > 0)
	{
		TransformComponentPtr childComponent= m_childComponents[0].lock();

		if (childComponent)
		{
			// reparentPreservingWorldTransform() internally detaches the child from us
			// (removes it from m_childComponents) before attaching to the grandparent,
			// which is what keeps this while-loop terminating.
			//
			// The grandparent is not always a legal parent for the child: deleting a
			// scene leaves its anchors and stencils facing the stage above it, which
			// they may not attach to. Those children detach to root level instead,
			// where the outliner lists them for a manual home. The detach has to
			// happen either way, or the child stays in the list and this loop spins.
			if (!grandparent || !childComponent->reparentPreservingWorldTransform(grandparent))
			{
				childComponent->detachFromParent(eDetachReason::parentDisposed);
			}
		}
		else
		{
			// Stale weak ptr — remove manually to avoid infinite loop
			m_childComponents.erase(m_childComponents.begin());
		}
	}

	// Detach self from parent
	if (grandparent)
	{
		detachFromParent(eDetachReason::selfDisposed);
	}

	MikanComponent::dispose();
}

void TransformComponent::setDefinition(MikanComponentDefinitionPtr config)
{
	MikanComponent::setDefinition(config);

	auto transformComponentConfigPtr= std::static_pointer_cast<TransformComponentDefinition>(config);

	// Initially the model component isn't attached to anything
	assert(m_parentComponent.lock() == nullptr);
	m_worldTransform= transformComponentConfigPtr->getRelativeMat4();
	m_relativeTransform= GlmTransform(m_worldTransform);
}

bool TransformComponent::attachToComponent(TransformComponentPtr newParentComponent)
{
	TransformComponentPtr oldParentComponent= m_parentComponent.lock();

	if (!newParentComponent || newParentComponent == oldParentComponent)
		return false;

	// Refuse an attachment to our own subtree, which would orphan the cycle from
	// the root and spin propogateWorldTransformChange forever
	for (TransformComponentPtr ancestor= newParentComponent; ancestor;
		 ancestor= ancestor->getParentTransformComponent())
	{
		if (ancestor.get() == this)
		{
			MIKAN_LOG_ERROR("TransformComponent::attachToComponent")
				<< "Refusing to attach component " << getComponentId() << " to component "
				<< newParentComponent->getComponentId() << ", which is inside its own subtree";
			return false;
		}
	}

	if (!canAttachToParent(newParentComponent))
	{
		MIKAN_LOG_ERROR("TransformComponent::attachToComponent")
			<< "Refusing to attach " << getComponentClassName() << " " << getComponentId() << " to "
			<< newParentComponent->getComponentClassName() << " " << newParentComponent->getComponentId()
			<< ": not an allowed parent type";
		return false;
	}

	// Detach from old parent
	if (oldParentComponent)
	{
		detachFromParent(eDetachReason::detachFromParent);
	}

	// Attach to new parent
	TransformComponentList& parentChildList= newParentComponent->m_childComponents;

	parentChildList.push_back(getSelfWeakPtr<TransformComponent>());
	m_parentComponent= newParentComponent;

	// Refresh world transform
	setRelativeTransform(m_relativeTransform);

	// Let any derived classes know that we've been attached to a new parent
	onAttachedToNewParent(newParentComponent);

	return true;
}

bool TransformComponent::reparentPreservingWorldTransform(TransformComponentPtr newParentComponent)
{
	const glm::mat4 savedWorldTransform= m_worldTransform;

	if (!attachToComponent(newParentComponent))
		return false;

	// attachToComponent recomputes the world transform from the retained relative
	// one, which moves the component. Put it back and let the relative transform
	// be the thing that changes.
	setWorldTransform(savedWorldTransform);

	return true;
}

void TransformComponent::detachFromParent(eDetachReason reason)
{
	// Remove ourselves from our current parent's child list
	TransformComponentPtr parent= m_parentComponent.lock();
	if (parent != nullptr)
	{
		TransformComponentList& parentChildList= parent->m_childComponents;

		for (auto it= parentChildList.begin(); it != parentChildList.end(); ++it)
		{
			TransformComponentPtr transformComponent= it->lock();

			if (transformComponent.get() == this)
			{
				parentChildList.erase(it);
				break;
			}
		}
	}

	// Forget about our parent
	m_parentComponent= TransformComponentPtr();

	// World transform is now the same as relative transform
	m_worldTransform= m_relativeTransform.getMat4();

	// Only bother propagating our new transform to our children
	// if we just doing a simple detach from our current parent
	if (reason == eDetachReason::detachFromParent)
	{
		propogateWorldTransformChange(eTransformChangeType::propogateWorldTransform);
	}

	// Let any derived classes know that we've been detached from our parent, and why
	onDetachedFromParent(parent, reason);
}

void TransformComponent::setRelativeTransform(const GlmTransform& newRelativeXform)
{
	// Set the new relative transform directly
	m_relativeTransform= newRelativeXform;
	propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);

	if (m_bWasInitialized)
	{
		TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();
		if (definitionPtr)
			definitionPtr->setRelativeTransform(newRelativeXform);
	}
}

void TransformComponent::setRelativePosition(const glm::vec3& position)
{
	m_relativeTransform.setPosition(position);
	propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);

	if (m_bWasInitialized)
	{
		TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();
		if (definitionPtr)
			definitionPtr->setRelativePosition(glm_vec3_to_MikanVector3f(position));
	}
}

void TransformComponent::setRelativeRotation(const glm::quat& quat)
{
	m_relativeTransform.setRotation(quat);
	propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);

	if (m_bWasInitialized)
	{
		TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();
		if (definitionPtr)
			definitionPtr->setRelativeRotation(glm_quat_to_MikanQuatf(quat));
	}
}

void TransformComponent::setRelativeScale(const glm::vec3& scale)
{
	m_relativeTransform.setScale(scale);
	propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);

	if (m_bWasInitialized)
	{
		TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();
		if (definitionPtr)
			definitionPtr->setRelativeScale(glm_vec3_to_MikanVector3f(scale));
	}
}

void TransformComponent::setWorldTransform(const glm::mat4& newWorldXform)
{
	// Set the new world transform directly
	m_worldTransform= newWorldXform;

	// Subtract our new world transform from out parent to compute new relative transform
	glm::mat4 invParentXform= glm::mat4(1.f);
	TransformComponentPtr parent= m_parentComponent.lock();
	if (parent != nullptr)
	{
		invParentXform= glm::inverse(parent->getWorldTransform());
	}
	const glm::mat4 relativeXform= glm_composite_xform(m_worldTransform, invParentXform);
	m_relativeTransform= GlmTransform(relativeXform);

	propogateWorldTransformChange(eTransformChangeType::propogateWorldTransform);

	// If this component has an associated definition, update it too
	if (m_bWasInitialized)
	{
		TransformComponentDefinitionPtr definitionPtr= getTransformComponentDefinition();

		if (definitionPtr)
		{
			definitionPtr->setRelativeTransform(m_relativeTransform);
		}
	}
}

const glm::vec3 TransformComponent::getWorldLocation() const { return glm_mat4_get_position(m_worldTransform); }

void TransformComponent::onDetachedFromParent(TransformComponentPtr oldParent, eDetachReason reason)
{
	TransformComponentDefinitionPtr definition= getTransformComponentDefinition();

	if (definition)
	{
		definition->setParentTransformId(INVALID_MIKAN_ID);
	}
}

void TransformComponent::onAttachedToNewParent(TransformComponentPtr newParent)
{
	if (newParent)
	{
		TransformComponentDefinitionPtr definition= getTransformComponentDefinition();

		if (definition)
		{
			definition->setParentTransformId(newParent->getTransformId());
		}
	}
}

void TransformComponent::refreshSceneRenderableModelMatrix()
{
	if (m_sceneRenderable != nullptr)
	{
		m_sceneRenderable->setModelMatrix(computeSceneRenderableModelMatrix());
	}
}

void TransformComponent::propogateWorldTransformChange(eTransformChangeType reason)
{
	// Recompute our world transform, if requested
	if (reason == eTransformChangeType::recomputeWorldTransformAndPropogate)
	{
		// Concat our transform to our parent's world transform
		TransformComponentPtr parent= m_parentComponent.lock();
		if (parent != nullptr)
		{
			m_worldTransform= glm_composite_xform(m_relativeTransform.getMat4(), parent->getWorldTransform());
		}
		else
		{
			m_worldTransform= m_relativeTransform.getMat4();
		}
	}

	// Update the model matrix on the attached IGlSceneRenderable
	refreshSceneRenderableModelMatrix();

	// Propagate our updated world transform to our children
	for (TransformComponentWeakPtr childComponentWeakPtr : m_childComponents)
	{
		TransformComponentPtr childComponent= childComponentWeakPtr.lock();

		if (childComponent != nullptr)
		{
			// Ask all children to recompute their world transform
			childComponent->propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);
		}
	}
}

template <typename t_component_type>
void visitAllTransformComponentsHelper(t_component_type* rootTransformComponent,
									   std::function<void(t_component_type* transformComponentPtr)> visitor)
{
	std::queue<t_component_type*> transformComponentStack;
	transformComponentStack.push(rootTransformComponent);

	while (transformComponentStack.size() > 0)
	{
		t_component_type* currentTransformComponent= transformComponentStack.front();
		transformComponentStack.pop();

		visitor(currentTransformComponent);

		for (TransformComponentWeakPtr childTransformWeakPtr : currentTransformComponent->getChildTransformComponents())
		{
			TransformComponentPtr childTransformPtr= childTransformWeakPtr.lock();
			if (childTransformPtr)
			{
				transformComponentStack.push(childTransformPtr.get());
			}
		}
	}
}

void TransformComponent::visitAllTransformComponents(TransformComponentVisitor visitor)
{
	visitAllTransformComponentsHelper(this, visitor);
}

void TransformComponent::visitAllTransformComponentsConst(TransformComponentConstVisitor visitor) const
{
	visitAllTransformComponentsHelper(this, visitor);
}

// -- IPropertyInterface ----
const std::string TransformComponent::k_relativeRotationPropertyId= "relative_rotation";
void TransformComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 TransformComponentDefinition::k_parentTransformIdPropertyId, MikanVariantType::INT)
								 ->setDefaultValue(INVALID_MIKAN_ID)
								 ->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 TransformComponentDefinition::k_relativeScalePropertyId, MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(1.f)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(TransformComponentDefinition::k_relativeQuaternionPropertyId,
											 MikanVariantType::QUATERNIONF)
			->setDefaultValue(MikanQuatf(1.f, 0.f, 0.f, 0.f))
			->setUIHidden());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 TransformComponentDefinition::k_relativePositionPropertyId, MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(0.f)));
	// Virtual Property for Euler angles (not actually stored, but get/set translates to relativeQuaternion)
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(TransformComponent::k_relativeRotationPropertyId,
																  MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(0.f))
								 ->setClientAPIHidden());
}

bool TransformComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == TransformComponentDefinition::k_parentTransformIdPropertyId)
	{
		MikanTransformID parentId= getTransformComponentDefinitionConst()->getParentTransformId();
		outValue= parentId;
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_relativeScalePropertyId)
	{
		const glm::vec3& scale= getRelativeScale();
		outValue= glm_vec3_to_MikanVector3f(scale);
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_relativeQuaternionPropertyId)
	{
		const glm::quat& quat= getRelativeRotation();
		outValue= glm_quat_to_MikanQuatf(quat);
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_relativePositionPropertyId)
	{
		const glm::vec3& pos= getRelativePosition();
		outValue= glm_vec3_to_MikanVector3f(pos);
		return true;
	}
	// Virtual Property for Euler angles (not actually stored, but get/set translates to relativeQuaternion)
	else if (propertyName == TransformComponent::k_relativeRotationPropertyId)
	{
		const glm::quat& model_orientation= getRelativeRotation();

		float angles[3]{};
		glm_quat_to_euler_angles(model_orientation, angles[0], angles[1], angles[2]);
		angles[0]*= k_radians_to_degrees;
		angles[1]*= k_radians_to_degrees;
		angles[2]*= k_radians_to_degrees;

		outValue= MikanVector3f(angles[0], angles[1], angles[2]);
		return true;
	}

	return MikanComponent::getPropertyValue(propertyName, outValue);
}

bool TransformComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == TransformComponentDefinition::k_relativeScalePropertyId)
	{
		MikanVector3f scale= inValue.getVector3fValue();

		setRelativeScale(glm::vec3(scale.x, scale.y, scale.z));
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_relativeQuaternionPropertyId)
	{
		glm::quat quat= MikanQuatf_to_glm_quat(inValue.getQuaternionfValue());

		setRelativeRotation(quat);
		return true;
	}
	else if (propertyName == TransformComponent::k_relativeRotationPropertyId)
	{
		MikanVector3f angles= inValue.getVector3fValue();

		glm::quat quat;
		glm_euler_angles_to_quat(angles.x * k_degrees_to_radians, angles.y * k_degrees_to_radians,
								 angles.z * k_degrees_to_radians, quat);
		setRelativeRotation(quat);
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_relativePositionPropertyId)
	{
		MikanVector3f pos= inValue.getVector3fValue();

		setRelativePosition(glm::vec3(pos.x, pos.y, pos.z));
		return true;
	}
	else if (propertyName == TransformComponentDefinition::k_parentTransformIdPropertyId)
	{
		// Reparenting routes through attach/detach so the runtime
		// parent/child links stay in sync with the definition
		const int newParentId= inValue.getIntValue();
		if (newParentId == INVALID_MIKAN_ID)
		{
			detachFromParent(eDetachReason::detachFromParent);
			return true;
		}

		TransformComponentPtr newParentComponent=
			getOwnerProjectManager()->getTypedComponentById<TransformComponent>(newParentId);
		if (!newParentComponent)
			return false;

		return attachToComponent(newParentComponent);
	}

	return MikanComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void TransformComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	MikanComponent::getFunctionDescriptors(outDescriptors);
}

// -- Lua Binding ----
void TransformComponent::bindLuaFunctions(struct lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<TransformComponent, MikanComponent>("TransformComponent")
		.addProperty(
			"relativePosition",
			[](TransformComponent* component) -> LuaVec3f { return LuaVec3f(component->getRelativePosition()); },
			[](TransformComponent* component, const LuaVec3f& position)
			{ component->setRelativePosition(position.toGlmVec3f()); })
		.addProperty(
			"relativeRotation", [](TransformComponent* component) -> LuaVec3f
			{ return LuaVec3f(glm_quat_to_MikanRotator3f(component->getRelativeRotation())); },
			[](TransformComponent* component, const LuaVec3f& rotator)
			{ component->setRelativeRotation(MikanRotator3f_to_glm_quat(rotator.toMikanRotator3f())); })
		.addProperty(
			"relativeQuaternion",
			[](TransformComponent* component) -> LuaQuatf { return LuaQuatf(component->getRelativeRotation()); },
			[](TransformComponent* component, const LuaQuatf& quat)
			{ component->setRelativeRotation(quat.toGlmQuat()); })
		.addProperty(
			"relativeScale",
			[](TransformComponent* component) -> LuaVec3f { return LuaVec3f(component->getRelativeScale()); },
			[](TransformComponent* component, const LuaVec3f& scale)
			{ component->setRelativeScale(scale.toGlmVec3f()); })
		.endClass();
}