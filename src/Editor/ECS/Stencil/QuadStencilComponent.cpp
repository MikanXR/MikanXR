#include "AnchorObjectSystem.h"
#include "Colors.h"
#include "IEditorWindow.h"
#include "MikanLineRenderer.h"
#include "MikanTextRenderer.h"
#include "AnchorComponent.h"
#include "TransformComponent.h"
#include "BoxColliderComponent.h"
#include "MathGLM.h"
#include "EditorObjectSystem.h"
#include "MathTypeConversion.h"
#include "MikanObject.h"
#include "MikanStencilTypes.h"
#include "QuadStencilComponent.h"
#include "SelectionComponent.h"
#include "QuadStencilSystem.h"
#include "StringUtils.h"
#include "TextStyle.h"

#include "glm/ext/vector_float4.hpp"
#include "glm/ext/matrix_float4x4_precision.hpp"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- QuadConfig -----
const std::string QuadStencilDefinition::k_quadStencilWidthPropertyId= "quad_width";
const std::string QuadStencilDefinition::k_quadStencilHeightPropertyId= "quad_height";
const std::string QuadStencilDefinition::k_quadStencilDoubleSidedPropertyId= "is_double_sided";

QuadStencilDefinition::QuadStencilDefinition()
	: StencilComponentDefinition()
	, m_quadWidth(0.25f)
	, m_quadHeight(0.25f)
	, m_bIsDoubleSided(true)
{
}

QuadStencilDefinition::QuadStencilDefinition(MikanStencilID stencilId)
	: StencilComponentDefinition(stencilId, "", MikanTransform())
	, m_quadWidth(0.25f)
	, m_quadHeight(0.25f)
	, m_bIsDoubleSided(true)
{
}

configuru::Config QuadStencilDefinition::writeToJSON()
{
	configuru::Config pt= StencilComponentDefinition::writeToJSON();

	pt["quad_width"]= m_quadWidth;
	pt["quad_height"]= m_quadHeight;
	pt["is_double_sided"]= m_bIsDoubleSided;

	return pt;
}

void QuadStencilDefinition::readFromJSON(const configuru::Config& pt)
{
	StencilComponentDefinition::readFromJSON(pt);

	m_quadWidth= pt.get_or<float>("quad_width", 0.25f);
	m_quadHeight= pt.get_or<float>("quad_height", 0.25f);
	m_bIsDoubleSided= pt.get_or<bool>("is_double_sided", false);
}

bool QuadStencilDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
											   const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!StencilComponentDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanQuadStencilComponentValues>();
	if (componentValues)
	{
		m_quadWidth= componentValues->quad_width;
		m_quadHeight= componentValues->quad_height;
		m_bIsDoubleSided= componentValues->is_double_sided;
	}

	return true;
}

void QuadStencilDefinition::setQuadWidth(float width)
{
	m_quadWidth= width;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadStencilWidthPropertyId));
}

void QuadStencilDefinition::setQuadHeight(float height)
{
	m_quadHeight= height;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadStencilHeightPropertyId));
}

void QuadStencilDefinition::setQuadSize(float width, float height)
{
	m_quadWidth= width;
	m_quadHeight= height;
	notifyPropertyChanged(ConfigPropertyChangeSet()
							  .addPropertyName(k_quadStencilWidthPropertyId)
							  .addPropertyName(k_quadStencilHeightPropertyId));
}

void QuadStencilDefinition::setIsDoubleSided(bool flag)
{
	m_bIsDoubleSided= flag;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadStencilDoubleSidedPropertyId));
}

// -- QuadStencilComponent -----
QuadStencilComponent::QuadStencilComponent(MikanObjectWeakPtr owner)
	: StencilComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle QuadStencilComponent::getClientAPIValuesStructType() const
{
	return &MikanQuadStencilComponentValues::staticGetArchetype();
}

void QuadStencilComponent::init()
{
	StencilComponent::init();

	m_boxCollider= getOwnerObject()->getComponentOfType<BoxColliderComponent>();
	m_selectionComponent= getOwnerObject()->getComponentOfType<SelectionComponent>();

	updateBoxColliderExtents();

	// Push our world transform to all child scene components
	propogateWorldTransformChange(eTransformChangeType::propogateWorldTransform);
}

void QuadStencilComponent::customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	QuadStencilDefinitionPtr quadDefinition= getQuadStencilDefinition();

	if (!quadDefinition->getIsDisabled())
	{
		TextStyle style= getDefaultTextStyle();

		const glm::mat4 xform= getWorldTransform();
		const glm::vec3 position= glm::vec3(xform[3]);

		glm::vec3 color= Colors::DarkGray;
		SelectionComponentPtr selectionComponent= m_selectionComponent.lock();
		if (selectionComponent)
		{
			if (selectionComponent->getIsSelected())
				color= Colors::Yellow;
			else if (selectionComponent->getIsHovered())
				color= Colors::LightGray;
		}

		drawTransformedQuad(graphicsContext, xform, quadDefinition->getQuadWidth(), quadDefinition->getQuadHeight(),
							color);
		if (!selectionComponent || !selectionComponent->getIsSelected())
			drawTransformedAxes(graphicsContext, xform, 0.1f, 0.1f, 0.1f);
		if (getObjectSystemOfType<EditorObjectSystem>()->getEditorSettings().bRenderComponentNames)
		{
			wchar_t wszName[256];
			StringUtils::convertMbsToWcs(quadDefinition->getComponentName().c_str(), wszName, sizeof(wszName));
			drawTextAtWorldPosition(graphicsContext, style, position, L"%s", wszName);
		}
	}
}

void QuadStencilComponent::updateBoxColliderExtents()
{
	QuadStencilDefinitionPtr quadDefinition= getQuadStencilDefinition();
	BoxColliderComponentPtr boxCollider= m_boxCollider.lock();
	if (boxCollider)
	{
		boxCollider->setHalfExtents(glm::vec3(quadDefinition->getQuadWidth(), quadDefinition->getQuadHeight(), 0.01f)
									* 0.5f);
	}
}

// -- IPropertyInterface ----
void QuadStencilComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	StencilComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(QuadStencilDefinition::k_quadStencilWidthPropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(QuadStencilDefinition::k_quadStencilHeightPropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
		QuadStencilDefinition::k_quadStencilDoubleSidedPropertyId, MikanVariantType::BOOL));
}

bool QuadStencilComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == QuadStencilDefinition::k_quadStencilWidthPropertyId)
	{
		outValue= getQuadStencilDefinition()->getQuadWidth();
		return true;
	}
	else if (propertyName == QuadStencilDefinition::k_quadStencilHeightPropertyId)
	{
		outValue= getQuadStencilDefinition()->getQuadHeight();
		return true;
	}
	else if (propertyName == QuadStencilDefinition::k_quadStencilDoubleSidedPropertyId)
	{
		outValue= getQuadStencilDefinition()->getIsDoubleSided();
		return true;
	}

	return StencilComponent::getPropertyValue(propertyName, outValue);
}

bool QuadStencilComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == QuadStencilDefinition::k_quadStencilWidthPropertyId)
	{
		float width= inValue.getFloatValue();

		getQuadStencilDefinition()->setQuadWidth(width);
		return true;
	}
	else if (propertyName == QuadStencilDefinition::k_quadStencilHeightPropertyId)
	{
		float height= inValue.getFloatValue();

		getQuadStencilDefinition()->setQuadHeight(height);
		return true;
	}
	else if (propertyName == QuadStencilDefinition::k_quadStencilDoubleSidedPropertyId)
	{
		bool isDoubleSided= inValue.getBoolValue();

		getQuadStencilDefinition()->setIsDoubleSided(isDoubleSided);
		return true;
	}

	return StencilComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void QuadStencilComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<QuadStencilComponent, StencilComponent>(QuadStencilComponent::k_componentClassName.c_str())
		.addProperty(
			"quadWidth", [](QuadStencilComponent* component) -> float
			{ return component->getQuadStencilDefinition()->getQuadWidth(); },
			[](QuadStencilComponent* component, float value)
			{ component->getQuadStencilDefinition()->setQuadWidth(value); })
		.addProperty(
			"quadHeight", [](QuadStencilComponent* component) -> float
			{ return component->getQuadStencilDefinition()->getQuadHeight(); },
			[](QuadStencilComponent* component, float value)
			{ component->getQuadStencilDefinition()->setQuadHeight(value); })
		.addProperty(
			"isDoubleSided", [](QuadStencilComponent* component) -> bool
			{ return component->getQuadStencilDefinition()->getIsDoubleSided(); },
			[](QuadStencilComponent* component, bool isDoubleSided)
			{ component->getQuadStencilDefinition()->setIsDoubleSided(isDoubleSided); })
		.endClass();
}