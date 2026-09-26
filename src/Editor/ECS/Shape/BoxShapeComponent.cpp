#include "BoxShapeComponent.h"

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

// -- BoxShapeDefinition ------
const std::string BoxShapeDefinition::k_boxXSizePropertyId= "box_x_size";
const std::string BoxShapeDefinition::k_boxYSizePropertyId= "box_y_size";
const std::string BoxShapeDefinition::k_boxZSizePropertyId= "box_z_size";

BoxShapeDefinition::BoxShapeDefinition()
	: ShapeComponentDefinition()
{
}

BoxShapeDefinition::BoxShapeDefinition(MikanShapeID shapeId)
	: ShapeComponentDefinition(shapeId)
{
}

configuru::Config BoxShapeDefinition::writeToJSON()
{
	configuru::Config pt= ShapeComponentDefinition::writeToJSON();

	pt[k_boxXSizePropertyId]= m_xSize;
	pt[k_boxYSizePropertyId]= m_ySize;
	pt[k_boxZSizePropertyId]= m_zSize;

	return pt;
}

void BoxShapeDefinition::readFromJSON(const configuru::Config& pt)
{
	ShapeComponentDefinition::readFromJSON(pt);

	m_xSize= pt.get_or<float>(k_boxXSizePropertyId, m_xSize);
	m_ySize= pt.get_or<float>(k_boxYSizePropertyId, m_ySize);
	m_zSize= pt.get_or<float>(k_boxZSizePropertyId, m_zSize);
}

void BoxShapeDefinition::setBoxXSize(float size)
{
	if (m_xSize != size)
	{
		m_xSize= size;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_boxXSizePropertyId));
	}
}

void BoxShapeDefinition::setBoxYSize(float size)
{
	if (m_ySize != size)
	{
		m_ySize= size;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_boxYSizePropertyId));
	}
}

void BoxShapeDefinition::setBoxZSize(float size)
{
	if (m_zSize != size)
	{
		m_zSize= size;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_boxZSizePropertyId));
	}
}

// -- BoxShapeComponent ------
BoxShapeComponent::BoxShapeComponent(MikanObjectWeakPtr owner)
	: ShapeComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle BoxShapeComponent::getClientAPIValuesStructType() const
{
	return &MikanBoxShapeComponentValues::staticGetArchetype();
}

void BoxShapeComponent::init()
{
	ShapeComponent::init();
	openShape();
}

void BoxShapeComponent::update(float deltaSeconds)
{
	ShapeComponent::update(deltaSeconds);

	auto meshInstance= std::dynamic_pointer_cast<IMkStaticMeshInstance>(m_sceneRenderable);
	if (!meshInstance)
		return;

	// Size lives in the model matrix, so refresh it here too: a size edit changes no transform
	// and would otherwise not reach the renderable until something moved.
	refreshSceneRenderableModelMatrix();

	MkMaterialInstancePtr matInst= meshInstance->getMaterialInstance();
	IMkTexturePtr colorTexture= getColorTexture();
	if (matInst && colorTexture)
	{
		matInst->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);
	}
}

void BoxShapeComponent::dispose()
{
	closeShape();
	ShapeComponent::dispose();
}

void BoxShapeComponent::openShape()
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

	// Unit cube: 6 faces, each a quad of 4 verts with position+texcoord.
	// Each face uses a different UV orientation.
	// Vertex layout: position (xyz) + texcoord (uv)
	struct PTVertex
	{
		float x, y, z, u, v;
	};
	// 6 faces × 4 vertices = 24 vertices
	static const PTVertex vertices[24]= {
		// +X face (right)
		{0.5f, -0.5f, -0.5f, 0.f, 1.f},
		{0.5f, 0.5f, -0.5f, 1.f, 1.f},
		{0.5f, 0.5f, 0.5f, 1.f, 0.f},
		{0.5f, -0.5f, 0.5f, 0.f, 0.f},
		// -X face (left)
		{-0.5f, -0.5f, 0.5f, 0.f, 1.f},
		{-0.5f, 0.5f, 0.5f, 1.f, 1.f},
		{-0.5f, 0.5f, -0.5f, 1.f, 0.f},
		{-0.5f, -0.5f, -0.5f, 0.f, 0.f},
		// +Y face (top)
		{-0.5f, 0.5f, -0.5f, 0.f, 1.f},
		{-0.5f, 0.5f, 0.5f, 1.f, 1.f},
		{0.5f, 0.5f, 0.5f, 1.f, 0.f},
		{0.5f, 0.5f, -0.5f, 0.f, 0.f},
		// -Y face (bottom)
		{-0.5f, -0.5f, 0.5f, 0.f, 1.f},
		{-0.5f, -0.5f, -0.5f, 1.f, 1.f},
		{0.5f, -0.5f, -0.5f, 1.f, 0.f},
		{0.5f, -0.5f, 0.5f, 0.f, 0.f},
		// +Z face (front)
		{-0.5f, -0.5f, 0.5f, 0.f, 1.f},
		{0.5f, -0.5f, 0.5f, 1.f, 1.f},
		{0.5f, 0.5f, 0.5f, 1.f, 0.f},
		{-0.5f, 0.5f, 0.5f, 0.f, 0.f},
		// -Z face (back)
		{0.5f, -0.5f, -0.5f, 0.f, 1.f},
		{-0.5f, -0.5f, -0.5f, 1.f, 1.f},
		{-0.5f, 0.5f, -0.5f, 1.f, 0.f},
		{0.5f, 0.5f, -0.5f, 0.f, 0.f},
	};
	// 6 faces × 2 triangles × 3 indices = 36 indices
	static const uint16_t indices[36]= {
		0,  1,  2,  0,  2,  3,  // +X
		4,  5,  6,  4,  6,  7,  // -X
		8,  9,  10, 8,  10, 11, // +Y
		12, 13, 14, 12, 14, 15, // -Y
		16, 17, 18, 16, 18, 19, // +Z
		20, 21, 22, 20, 22, 23, // -Z
	};

	IMkTriangulatedMeshPtr boxMesh=
		createMkTriangulatedMesh(graphicsContext.get(), "box_shape", reinterpret_cast<const uint8_t*>(vertices),
								 sizeof(PTVertex), 24, reinterpret_cast<const uint8_t*>(indices), sizeof(uint16_t),
								 12,     // triangle count (6 faces × 2)
								 false); // mesh owns a copy of the vertex/index data

	if (!boxMesh)
		return;

	boxMesh->setMaterial(material);
	boxMesh->createResources();

	IMkStaticMeshInstancePtr meshInstance= createMkStaticMeshInstance("box_shape", boxMesh);
	meshInstance->setVisible(true);

	m_sceneRenderable= meshInstance;
}

void BoxShapeComponent::closeShape() { m_sceneRenderable= nullptr; }

void BoxShapeComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												const ConfigPropertyChangeSet& changedPropertySet)
{
	ShapeComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	const bool sizeChanged= changedPropertySet.hasPropertyName(BoxShapeDefinition::k_boxXSizePropertyId)
							|| changedPropertySet.hasPropertyName(BoxShapeDefinition::k_boxYSizePropertyId)
							|| changedPropertySet.hasPropertyName(BoxShapeDefinition::k_boxZSizePropertyId);

	if (sizeChanged)
	{
		updateBoxColliderExtents();
		// Model matrix update handled each frame in update()
	}
}

void BoxShapeComponent::updateBoxColliderExtents()
{
	auto collider= m_boxCollider.lock();
	if (!collider)
		return;

	const auto def= getBoxShapeDefinition();
	if (!def)
		return;

	collider->setHalfExtents(
		glm::vec3(def->getBoxXSize() * 0.5f, def->getBoxYSize() * 0.5f, def->getBoxZSize() * 0.5f));
}

// -- IPropertyInterface ----
void BoxShapeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	ShapeComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(BoxShapeDefinition::k_boxXSizePropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(1.f));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(BoxShapeDefinition::k_boxYSizePropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(1.f));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(BoxShapeDefinition::k_boxZSizePropertyId, MikanVariantType::FLOAT)
			->setDefaultValue(1.f));
}

bool BoxShapeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	const auto def= getBoxShapeDefinition();

	if (propertyName == BoxShapeDefinition::k_boxXSizePropertyId)
	{
		outValue= def->getBoxXSize();
		return true;
	}
	else if (propertyName == BoxShapeDefinition::k_boxYSizePropertyId)
	{
		outValue= def->getBoxYSize();
		return true;
	}
	else if (propertyName == BoxShapeDefinition::k_boxZSizePropertyId)
	{
		outValue= def->getBoxZSize();
		return true;
	}

	return ShapeComponent::getPropertyValue(propertyName, outValue);
}

bool BoxShapeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	const auto def= getBoxShapeDefinition();

	if (propertyName == BoxShapeDefinition::k_boxXSizePropertyId)
	{
		def->setBoxXSize(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == BoxShapeDefinition::k_boxYSizePropertyId)
	{
		def->setBoxYSize(inValue.getFloatValue());
		return true;
	}
	else if (propertyName == BoxShapeDefinition::k_boxZSizePropertyId)
	{
		def->setBoxZSize(inValue.getFloatValue());
		return true;
	}

	return ShapeComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void BoxShapeComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<BoxShapeComponent, ShapeComponent>(BoxShapeComponent::k_componentClassName.c_str())
		.addProperty(
			"boxXSize",
			[](BoxShapeComponent* component) -> float { return component->getBoxShapeDefinition()->getBoxXSize(); },
			[](BoxShapeComponent* component, float value) { component->getBoxShapeDefinition()->setBoxXSize(value); })
		.addProperty(
			"boxYSize",
			[](BoxShapeComponent* component) -> float { return component->getBoxShapeDefinition()->getBoxYSize(); },
			[](BoxShapeComponent* component, float value) { component->getBoxShapeDefinition()->setBoxYSize(value); })
		.addProperty(
			"boxZSize",
			[](BoxShapeComponent* component) -> float { return component->getBoxShapeDefinition()->getBoxZSize(); },
			[](BoxShapeComponent* component, float value) { component->getBoxShapeDefinition()->setBoxZSize(value); })
		.endClass();
}

glm::mat4 BoxShapeComponent::computeSceneRenderableModelMatrix() const
{
	const auto def= getBoxShapeDefinition();
	if (!def)
		return getWorldTransform();

	return getWorldTransform()
		   * glm::scale(glm::mat4(1.f), {def->getBoxXSize(), def->getBoxYSize(), def->getBoxZSize()});
}
