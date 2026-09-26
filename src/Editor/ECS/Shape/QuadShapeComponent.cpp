#include "QuadShapeComponent.h"

#include "MikanShapeTypes.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include "BoxColliderComponent.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkStaticMeshInstance.h"
#include "IMkTriangulatedMesh.h"
#include "MkMaterialInstance.h"
#include "MkMaterial.h"
#include "MikanObject.h"
#include "MikanVariantTypes.h"
#include "PropertyInterface.h"
#include "SelectionComponent.h"
#include "MkShaderConstants.h"

#include "glm/gtc/matrix_transform.hpp"

// -- QuadShapeDefinition ------
const std::string QuadShapeDefinition::k_quadWidthPropertyId= "quad_width";
const std::string QuadShapeDefinition::k_quadHeightPropertyId= "quad_height";
const std::string QuadShapeDefinition::k_quadDoubleSidedPropertyId= "is_double_sided";

QuadShapeDefinition::QuadShapeDefinition()
	: ShapeComponentDefinition()
{
}

QuadShapeDefinition::QuadShapeDefinition(MikanShapeID shapeId)
	: ShapeComponentDefinition(shapeId)
{
}

configuru::Config QuadShapeDefinition::writeToJSON()
{
	configuru::Config pt= ShapeComponentDefinition::writeToJSON();

	pt[k_quadWidthPropertyId]= m_quadWidth;
	pt[k_quadHeightPropertyId]= m_quadHeight;
	pt[k_quadDoubleSidedPropertyId]= m_bIsDoubleSided;

	return pt;
}

void QuadShapeDefinition::readFromJSON(const configuru::Config& pt)
{
	ShapeComponentDefinition::readFromJSON(pt);

	m_quadWidth= pt.get_or<float>(k_quadWidthPropertyId, m_quadWidth);
	m_quadHeight= pt.get_or<float>(k_quadHeightPropertyId, m_quadHeight);
	m_bIsDoubleSided= pt.get_or<bool>(k_quadDoubleSidedPropertyId, m_bIsDoubleSided);
}

void QuadShapeDefinition::setQuadWidth(float width)
{
	if (m_quadWidth != width)
	{
		m_quadWidth= width;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadWidthPropertyId));
	}
}

void QuadShapeDefinition::setQuadHeight(float height)
{
	if (m_quadHeight != height)
	{
		m_quadHeight= height;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadHeightPropertyId));
	}
}

void QuadShapeDefinition::setIsDoubleSided(bool flag)
{
	if (m_bIsDoubleSided != flag)
	{
		m_bIsDoubleSided= flag;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_quadDoubleSidedPropertyId));
	}
}

// -- QuadShapeComponent ------
QuadShapeComponent::QuadShapeComponent(MikanObjectWeakPtr owner)
	: ShapeComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle QuadShapeComponent::getClientAPIValuesStructType() const
{
	return &MikanQuadShapeComponentValues::staticGetArchetype();
}

void QuadShapeComponent::init()
{
	ShapeComponent::init();
	openShape();
}

void QuadShapeComponent::update(float deltaSeconds)
{
	ShapeComponent::update(deltaSeconds);

	auto meshInstance= std::dynamic_pointer_cast<IMkStaticMeshInstance>(m_sceneRenderable);
	if (!meshInstance)
		return;

	// Size lives in the model matrix, so refresh it here too: a width or height edit changes
	// no transform and would otherwise not reach the renderable until something moved.
	refreshSceneRenderableModelMatrix();

	// Bind texture to material instance
	MkMaterialInstancePtr matInst= meshInstance->getMaterialInstance();
	IMkTexturePtr colorTexture= getColorTexture();
	if (matInst && colorTexture)
	{
		matInst->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);
	}
}

void QuadShapeComponent::dispose()
{
	closeShape();
	ShapeComponent::dispose();
}

void QuadShapeComponent::openShape()
{
	closeShape();

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	if (!ownerWindow)
		return;

	IMkGraphicsContextPtr graphicsContext= ownerWindow->getGraphicsContext();
	if (!graphicsContext)
		return;

	MkMaterialConstPtr material= graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PT_TEXTURED);
	if (!material)
		return;

	// Unit quad in the XY plane: verts at ±0.5, UVs 0..1
	// Vertex layout: position (xyz) + texcoord (uv)
	struct PTVertex
	{
		float x, y, z, u, v;
	};
	static const PTVertex vertices[4]= {
		{-0.5f, -0.5f, 0.f, 0.f, 1.f},
		{0.5f, -0.5f, 0.f, 1.f, 1.f},
		{0.5f, 0.5f, 0.f, 1.f, 0.f},
		{-0.5f, 0.5f, 0.f, 0.f, 0.f},
	};
	static const uint16_t indices[6]= {0, 1, 2, 0, 2, 3};

	IMkTriangulatedMeshPtr quadMesh=
		createMkTriangulatedMesh(graphicsContext.get(), "quad_shape", reinterpret_cast<const uint8_t*>(vertices),
								 sizeof(PTVertex), 4, reinterpret_cast<const uint8_t*>(indices), sizeof(uint16_t),
								 2,      // triangle count
								 false); // mesh does NOT own the vertex/index data (static array above)

	if (!quadMesh)
		return;

	quadMesh->setMaterial(material);
	quadMesh->createResources();

	IMkStaticMeshInstancePtr meshInstance= createMkStaticMeshInstance("quad_shape", quadMesh);
	meshInstance->setVisible(true);

	// Store in m_sceneRenderable so addAllRenderablesToMkScene picks it up
	m_sceneRenderable= meshInstance;
}

void QuadShapeComponent::closeShape() { m_sceneRenderable= nullptr; }

void QuadShapeComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												 const ConfigPropertyChangeSet& changedPropertySet)
{
	ShapeComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	const bool sizeChanged= changedPropertySet.hasPropertyName(QuadShapeDefinition::k_quadWidthPropertyId)
							|| changedPropertySet.hasPropertyName(QuadShapeDefinition::k_quadHeightPropertyId);

	if (sizeChanged)
	{
		updateBoxColliderExtents();
		// Model matrix update handled in update() each frame
	}
}

void QuadShapeComponent::updateBoxColliderExtents()
{
	auto collider= m_boxCollider.lock();
	if (!collider)
		return;

	const auto def= getQuadShapeDefinition();
	if (!def)
		return;

	collider->setHalfExtents(glm::vec3(def->getQuadWidth() * 0.5f, def->getQuadHeight() * 0.5f, 0.01f));
}

// -- IPropertyInterface ----
void QuadShapeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	ShapeComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(QuadShapeDefinition::k_quadWidthPropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(1.f));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(QuadShapeDefinition::k_quadHeightPropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(1.f));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(QuadShapeDefinition::k_quadDoubleSidedPropertyId, MikanVariantType::BOOL)
			->setDefaultValue(true));
}

bool QuadShapeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	const auto def= getQuadShapeDefinition();

	if (propertyName == QuadShapeDefinition::k_quadWidthPropertyId)
	{
		outValue= def->getQuadWidth();
		return true;
	}
	else if (propertyName == QuadShapeDefinition::k_quadHeightPropertyId)
	{
		outValue= def->getQuadHeight();
		return true;
	}
	else if (propertyName == QuadShapeDefinition::k_quadDoubleSidedPropertyId)
	{
		outValue= def->getIsDoubleSided();
		return true;
	}

	return ShapeComponent::getPropertyValue(propertyName, outValue);
}

bool QuadShapeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	const auto def= getQuadShapeDefinition();

	if (propertyName == QuadShapeDefinition::k_quadWidthPropertyId)
	{
		def->setQuadWidth(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == QuadShapeDefinition::k_quadHeightPropertyId)
	{
		def->setQuadHeight(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == QuadShapeDefinition::k_quadDoubleSidedPropertyId)
	{
		def->setIsDoubleSided(inValue.getBoolValue());
		return true;
	}

	return ShapeComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void QuadShapeComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<QuadShapeComponent, ShapeComponent>(QuadShapeComponent::k_componentClassName.c_str())
		.addProperty(
			"quadWidth",
			[](QuadShapeComponent* component) -> float { return component->getQuadShapeDefinition()->getQuadWidth(); },
			[](QuadShapeComponent* component, float value)
			{ component->getQuadShapeDefinition()->setQuadWidth(value); })
		.addProperty(
			"quadHeight",
			[](QuadShapeComponent* component) -> float { return component->getQuadShapeDefinition()->getQuadHeight(); },
			[](QuadShapeComponent* component, float value)
			{ component->getQuadShapeDefinition()->setQuadHeight(value); })
		.addProperty(
			"isDoubleSided", [](QuadShapeComponent* component) -> bool
			{ return component->getQuadShapeDefinition()->getIsDoubleSided(); },
			[](QuadShapeComponent* component, bool value)
			{ component->getQuadShapeDefinition()->setIsDoubleSided(value); })
		.endClass();
}

glm::mat4 QuadShapeComponent::computeSceneRenderableModelMatrix() const
{
	const auto def= getQuadShapeDefinition();
	if (!def)
		return getWorldTransform();

	return getWorldTransform() * glm::scale(glm::mat4(1.f), {def->getQuadWidth(), def->getQuadHeight(), 1.f});
}
