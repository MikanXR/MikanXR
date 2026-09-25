#include "App.h"
#include "AssetReferencePropertyMetaData.h"
#include "FunctionInterface.h"
#include "IMkGraphicsContext.h"
#include "IMkTexture.h"
#include "IEditorWindow.h"
#include "Logger.h"
#include "MikanTextureCache.h"
#include "MikanVariantTypes.h"
#include "MikanObject.h"
#include "ShapeGraphAssetReference.h"
#include "PathUtils.h"
#include "ProjectManager.h"
#include "PropertyInterface.h"
#include "ShapeComponent.h"
#include "SceneActorParenting.h"
#include "Windows/ShapeNodeEditorWindow.h"

#include "Graphs/NodeEvaluator.h"
#include "Graphs/NodeGraph.h"
#include "Graphs/ShapeNodeGraph.h"

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- ShapeComponentDefinition ------
const std::string ShapeComponentDefinition::k_shapeGraphPathPropertyId= "shape_graph_path";

ShapeComponentDefinition::ShapeComponentDefinition()
	: TransformComponentDefinition()
	, m_nodeGraphAssetRef(std::make_shared<AssetReferenceConfig>())
{
}

ShapeComponentDefinition::ShapeComponentDefinition(MikanShapeID shapeId)
	: TransformComponentDefinition(shapeId)
	, m_nodeGraphAssetRef(std::make_shared<AssetReferenceConfig>())
{
}

configuru::Config ShapeComponentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	if (m_nodeGraphAssetRef)
	{
		pt[k_shapeGraphPathPropertyId]= m_nodeGraphAssetRef->writeToJSON();
	}

	return pt;
}

void ShapeComponentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_nodeGraphAssetRef= ShapeGraphAssetReferenceFactory().allocateAssetReferenceConfig();
	if (pt.has_key(k_shapeGraphPathPropertyId))
	{
		m_nodeGraphAssetRef->readFromJSON(pt[k_shapeGraphPathPropertyId]);
	}
}

bool ShapeComponentDefinition::hasShapeGraphPath() const
{
	return m_nodeGraphAssetRef && !m_nodeGraphAssetRef->assetPath.empty();
}

std::filesystem::path ShapeComponentDefinition::getShapeGraphPath() const
{
	return m_nodeGraphAssetRef ? m_nodeGraphAssetRef->assetPath : std::filesystem::path();
}

void ShapeComponentDefinition::setShapeGraphPath(const std::filesystem::path& graphPath)
{
	const std::string stored= graphPath.empty() ? std::string() : PathUtils::makeStoredProjectPath(graphPath);

	if (!m_nodeGraphAssetRef || stored != m_nodeGraphAssetRef->assetPath)
	{
		if (!m_nodeGraphAssetRef)
			m_nodeGraphAssetRef= std::make_shared<AssetReferenceConfig>();
		m_nodeGraphAssetRef->assetPath= stored;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_shapeGraphPathPropertyId));
	}
}

// -- ShapeComponent ------
const std::string ShapeComponent::k_addNewShapeGraphFunctionId= "add_new_shape_graph";
const std::string ShapeComponent::k_editShapeGraphFunctionId= "edit_shape_graph";
const std::string ShapeComponent::k_removeShapeGraphFunctionId= "remove_shape_graph";

ShapeComponent::ShapeComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
	m_bWantsUpdate= true;
}

// -- TransformComponent ----
bool ShapeComponent::canAttachToParent(TransformComponentConstPtr newParentComponent) const
{
	return isValidSceneActorParent(newParentComponent);
}

void ShapeComponent::init()
{
	TransformComponent::init();

	m_nodeGraphAssetRef=
		std::static_pointer_cast<ShapeGraphAssetReference>(ShapeGraphAssetReferenceFactory().allocateAssetReference());

	// Listen for changes to the shape definition
	getShapeComponentDefinition()->OnPropertyChanged+= MakeDelegate(this, &ShapeComponent::onDefinitionChanged);
}

void ShapeComponent::postInit()
{
	TransformComponent::postInit();

	// Initialize the shape graph if we have one assigned
	handleShapeNodeGraphChanged(getShapeComponentDefinition()->getShapeGraphPath());
}

void ShapeComponent::dispose()
{
	getShapeComponentDefinition()->OnPropertyChanged-= MakeDelegate(this, &ShapeComponent::onDefinitionChanged);

	m_nodeGraph= nullptr;
	m_nodeGraphAssetRef= nullptr;

	TransformComponent::dispose();
}

void ShapeComponent::onDefinitionChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet)
{
	if (changedPropertySet.hasPropertyName(ShapeComponentDefinition::k_shapeGraphPathPropertyId))
	{
		handleShapeNodeGraphChanged(getShapeComponentDefinition()->getShapeGraphPath());
	}
}

IMkTexturePtr ShapeComponent::getColorTexture() const
{
	MikanTextureCache* textureCache= getOwnerEditorWindow()->getTextureCache();
	return textureCache->tryGetTextureByName(INTERNAL_MISSING_TEXTURE_RGBA);
}

void ShapeComponent::onDefinitionMarkedDirty(CommonConfigPtr configPtr,
											 const ConfigPropertyChangeSet& changedPropertySet)
{
	TransformComponent::onDefinitionMarkedDirty(configPtr, changedPropertySet);

	// Subclasses override this to respond to property changes (e.g. size change → rebuild mesh)
}

// -- Shape Node Graph ----
bool ShapeComponent::hasValidShapeGraph() const { return m_nodeGraph != nullptr; }

void ShapeComponent::setEditorShapeNodeGraph(ShapeNodeGraphPtr editorNodeGraph) { m_editorNodeGraph= editorNodeGraph; }

void ShapeComponent::addNewShapeGraph()
{
	removeShapeGraph();
	editShapeGraph();
}

void ShapeComponent::editShapeGraph()
{
	App* app= App::getInstance();

	if (!app->hasWindowOfType<ShapeNodeEditorWindow>())
	{
		app->createAppWindow<ShapeNodeEditorWindow>()->bindShapeComponent(getSelfPtr<ShapeComponent>());
	}
}

void ShapeComponent::removeShapeGraph() { setShapeGraphAssetPath(std::filesystem::path()); }

void ShapeComponent::renderShapeGraph(const glm::mat4& vpMatrix, IMkGraphicsContext* graphicsContext)
{
	// Editor graph takes priority over asset-based graph
	ShapeNodeGraphPtr nodeGraph= m_editorNodeGraph.lock();
	if (!nodeGraph)
		nodeGraph= m_nodeGraph;
	if (!nodeGraph)
		return;

	NodeEvaluator evaluator;
	evaluator.setCurrentGraphicsContext(graphicsContext);

	nodeGraph->renderShape(vpMatrix, evaluator);

	// Taken whether or not the shape rendered, because a successful render can still carry
	// warnings and the editor has to see them. An entirely clean render leaves this empty.
	m_lastNodeEvalErrors= evaluator.getErrors();
}

std::filesystem::path ShapeComponent::getShapeGraphAssetResolvedPath() const
{
	return m_nodeGraphAssetRef ? m_nodeGraphAssetRef->getResolvedAssetPath() : std::filesystem::path();
}

void ShapeComponent::setShapeGraphAssetPath(const std::filesystem::path& assetRefPath)
{
	// A file of another graph kind is refused outright rather than stored and
	// failed on every load. A path with no file behind it yet is kept.
	const std::string graphClassName= NodeGraphFactory::peekGraphClassName(assetRefPath);
	if (!graphClassName.empty() && graphClassName != ShapeNodeGraph::k_graphClassName)
	{
		MIKAN_LOG_ERROR("ShapeComponent::setShapeGraphAssetPath")
			<< assetRefPath << " is a " << graphClassName << ", not a shape graph";
		return;
	}

	handleShapeNodeGraphChanged(assetRefPath);
	getShapeComponentDefinition()->setShapeGraphPath(assetRefPath);
}

void ShapeComponent::handleShapeNodeGraphChanged(const std::filesystem::path& newAssetRefPath)
{
	if (!m_nodeGraphAssetRef)
	{
		m_nodeGraphAssetRef= std::static_pointer_cast<ShapeGraphAssetReference>(
			ShapeGraphAssetReferenceFactory().allocateAssetReference());
	}

	m_nodeGraphAssetRef->setAssetPath(newAssetRefPath);

	if (!m_nodeGraphAssetRef->isEmpty())
	{
		m_nodeGraph= std::dynamic_pointer_cast<ShapeNodeGraph>(
			NodeGraphFactory::loadNodeGraph(getOwnerEditorWindow(), newAssetRefPath, ShapeNodeGraph::k_graphClassName));

		if (m_nodeGraph)
		{
			MIKAN_LOG_INFO("ShapeComponent::handleShapeNodeGraphChanged") << "Loaded shape graph: " << newAssetRefPath;

			m_nodeGraph->bindToShapeComponent(getSelfPtr<ShapeComponent>());
		}
		else
		{
			MIKAN_LOG_ERROR("ShapeComponent::handleShapeNodeGraphChanged")
				<< "Failed to load shape graph: " << newAssetRefPath;
		}
	}
	else
	{
		m_nodeGraph= nullptr;
	}

	m_lastNodeEvalErrors.clear();
}

// -- IPropertyInterface ----
void ShapeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(ShapeComponentDefinition::k_shapeGraphPathPropertyId,
																  MikanVariantType::STRING)
								 ->addMetaData(std::make_shared<AssetReferenceFactoryMetaData>(
									 AssetReferenceFactory::createFactory<ShapeGraphAssetReferenceFactory>())));
}

bool ShapeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == ShapeComponentDefinition::k_shapeGraphPathPropertyId)
	{
		outValue= getShapeComponentDefinition()->getShapeGraphPath();
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool ShapeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == ShapeComponentDefinition::k_shapeGraphPathPropertyId)
	{
		const std::string fileString= inValue.getUtf8Value();
		const std::filesystem::path filePath(fileString);

		// Route through the asset-path setter so the live node graph
		// rebuilds along with the definition
		setShapeGraphAssetPath(filePath);
		return true;
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
void ShapeComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_addNewShapeGraphFunctionId, "Add Shape Graph")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_editShapeGraphFunctionId, "Edit Shape Graph")->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_removeShapeGraphFunctionId, "Remove Shape Graph")->setUIHidden());
}

bool ShapeComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_addNewShapeGraphFunctionId)
	{
		addNewShapeGraph();
		return true;
	}
	else if (functionName == k_editShapeGraphFunctionId)
	{
		editShapeGraph();
		return true;
	}
	else if (functionName == k_removeShapeGraphFunctionId)
	{
		removeShapeGraph();
		return true;
	}

	return TransformComponent::invokeFunction(functionName);
}

// -- Lua Binding ----
void ShapeComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L).deriveClass<ShapeComponent, TransformComponent>("ShapeComponent").endClass();
}
