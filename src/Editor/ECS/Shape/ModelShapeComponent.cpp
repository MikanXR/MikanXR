#include "ModelShapeComponent.h"

#include "MikanShapeTypes.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

#include "AssetReference.h"
#include "AssetReferencePropertyMetaData.h"
#include "ModelAssetReference.h"
#include "PathUtils.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkStaticMeshInstance.h"
#include "IMkTriangulatedMesh.h"
#include "MikanModelResourceManager.h"
#include "MikanObject.h"
#include "MikanRenderModelResource.h"
#include "MikanVariantTypes.h"
#include "MeshColliderComponent.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkShaderConstants.h"
#include "PropertyInterface.h"
#include "SelectionComponent.h"
#include "StaticMeshComponent.h"
#include "TransformComponent.h"

// -- ModelShapeDefinition ------
const std::string ModelShapeDefinition::k_modelPathPropertyId= "model_path";

ModelShapeDefinition::ModelShapeDefinition()
	: ShapeComponentDefinition()
{
}

ModelShapeDefinition::ModelShapeDefinition(MikanShapeID shapeId)
	: ShapeComponentDefinition(shapeId)
{
}

configuru::Config ModelShapeDefinition::writeToJSON()
{
	configuru::Config pt= ShapeComponentDefinition::writeToJSON();

	if (m_modelAssetRefConfig)
	{
		pt[k_modelPathPropertyId]= m_modelAssetRefConfig->assetPath;
	}

	return pt;
}

void ModelShapeDefinition::readFromJSON(const configuru::Config& pt)
{
	ShapeComponentDefinition::readFromJSON(pt);

	const std::string pathStr= pt.get_or<std::string>(k_modelPathPropertyId, "");
	if (!pathStr.empty())
	{
		if (!m_modelAssetRefConfig)
			m_modelAssetRefConfig= std::make_shared<AssetReferenceConfig>();
		m_modelAssetRefConfig->assetPath= pathStr;
	}
}

bool ModelShapeDefinition::hasModelPath() const
{
	return m_modelAssetRefConfig && !m_modelAssetRefConfig->assetPath.empty();
}

const std::filesystem::path ModelShapeDefinition::getModelPath() const
{
	if (m_modelAssetRefConfig)
		return m_modelAssetRefConfig->assetPath;
	return {};
}

void ModelShapeDefinition::setModelPath(const std::filesystem::path& path, bool bForceDirty)
{
	if (!m_modelAssetRefConfig)
		m_modelAssetRefConfig= std::make_shared<AssetReferenceConfig>();

	const std::string stored= path.empty() ? std::string() : PathUtils::makeStoredProjectPath(path);

	if (m_modelAssetRefConfig->assetPath != stored || bForceDirty)
	{
		m_modelAssetRefConfig->assetPath= stored;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_modelPathPropertyId));
	}
}

// -- ModelShapeComponent ------
ModelShapeComponent::ModelShapeComponent(MikanObjectWeakPtr owner)
	: ShapeComponent(owner)
{
}

void ModelShapeComponent::init()
{
	ShapeComponent::init();

	rebuildMeshComponents();
}

void ModelShapeComponent::update(float deltaSeconds)
{
	ShapeComponent::update(deltaSeconds);

	// Bind the texture from the texture source to all mesh material instances
	IMkTexturePtr colorTexture= getColorTexture();
	if (colorTexture)
	{
		for (StaticMeshComponentPtr meshComp : m_triMeshComponents)
		{
			auto meshInstance= meshComp->getStaticMesh();
			if (meshInstance)
			{
				MkMaterialInstancePtr matInst= meshInstance->getMaterialInstance();
				if (matInst)
				{
					matInst->setTextureBySemantic(eUniformSemantic::rgbTexture, colorTexture);
				}
			}
		}
	}
}

void ModelShapeComponent::dispose()
{
	disposeMeshComponents();
	ShapeComponent::dispose();
}

void ModelShapeComponent::setModelPath(const std::filesystem::path& path)
{
	getModelShapeDefinition()->setModelPath(path);
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle ModelShapeComponent::getClientAPIValuesStructType() const
{
	return &MikanModelShapeComponentValues::staticGetArchetype();
}

void ModelShapeComponent::disposeMeshComponents()
{
	while (m_meshComponents.size() > 0)
	{
		TransformComponentPtr componentPtr= m_meshComponents.back();
		componentPtr->dispose();
		m_meshComponents.pop_back();
	}

	m_colliderComponents.clear();
	m_triMeshComponents.clear();
	m_modelResource.reset();
}

void ModelShapeComponent::rebuildMeshComponents()
{
	ModelShapeDefinitionPtr modelDef= getModelShapeDefinition();
	MikanObjectPtr ownerObject= getOwnerObject();
	ShapeComponentPtr shapeComponentPtr= getSelfPtr<ShapeComponent>();

	disposeMeshComponents();

	if (!modelDef || !modelDef->hasModelPath())
		return;

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	if (!ownerWindow)
		return;

	MikanModelResourceManager* modelResourceManager= ownerWindow->getModelResourceManager();
	MkMaterialConstPtr shapeMaterial=
		ownerWindow->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED);
	MikanRenderModelResourcePtr modelResourcePtr= modelResourceManager->fetchRenderModel(
		PathUtils::resolveProjectResource(modelDef->getModelPath()), shapeMaterial);

	// Held onto so the client render geometry request can reach the resource's cached payload
	// without re-resolving the model path and material.
	m_modelResource= modelResourcePtr;

	if (!modelResourcePtr)
		return;

	for (int meshIndex= 0; meshIndex < modelResourcePtr->getTriangulatedMeshCount(); ++meshIndex)
	{
		IMkTriangulatedMeshPtr triMeshPtr= modelResourcePtr->getTriangulatedMesh(meshIndex);

		IMkStaticMeshInstancePtr triMeshInstancePtr= createMkStaticMeshInstance(triMeshPtr->getName(), triMeshPtr);
		triMeshInstancePtr->setVisible(true);

		StaticMeshComponentPtr meshComponentPtr= ownerObject->addComponent<StaticMeshComponent>();
		meshComponentPtr->setName(triMeshPtr->getName());
		meshComponentPtr->setStaticMesh(triMeshInstancePtr);
		meshComponentPtr->attachToComponent(shapeComponentPtr);
		m_meshComponents.push_back(meshComponentPtr);
		m_triMeshComponents.push_back(meshComponentPtr);

		MeshColliderComponentPtr colliderPtr= ownerObject->addComponent<MeshColliderComponent>();
		colliderPtr->setName(triMeshPtr->getName());
		colliderPtr->setStaticMeshComponent(meshComponentPtr);
		colliderPtr->attachToComponent(shapeComponentPtr);
		m_colliderComponents.push_back(colliderPtr);
		m_meshComponents.push_back(colliderPtr);
	}
}

void ModelShapeComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
												  const ConfigPropertyChangeSet& changedPropertySet)
{
	ShapeComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	if (changedPropertySet.hasPropertyName(ModelShapeDefinition::k_modelPathPropertyId))
	{
		rebuildMeshComponents();
	}
}

// -- IPropertyInterface ----
void ModelShapeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	ShapeComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(ModelShapeDefinition::k_modelPathPropertyId, MikanVariantType::STRING)
			->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
				AssetReferenceFactory::createFactory<ModelAssetReferenceFactory>())));
}

bool ModelShapeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	const auto def= getModelShapeDefinition();

	if (propertyName == ModelShapeDefinition::k_modelPathPropertyId)
	{
		outValue= def->getModelPath();
		return true;
	}

	return ShapeComponent::getPropertyValue(propertyName, outValue);
}

bool ModelShapeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	const auto def= getModelShapeDefinition();

	if (propertyName == ModelShapeDefinition::k_modelPathPropertyId)
	{
		def->setModelPath(inValue.getUtf8Value());
		return true;
	}

	return ShapeComponent::setPropertyValue(propertyName, inValue);
}

// -- Lua Binding ----
void ModelShapeComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<ModelShapeComponent, ShapeComponent>(ModelShapeComponent::k_componentClassName.c_str())
		.addProperty("modelPath", [](ModelShapeComponent* component) -> std::string
					 { return component->getModelShapeDefinition()->getModelPath().string(); })
		.endClass();
}
