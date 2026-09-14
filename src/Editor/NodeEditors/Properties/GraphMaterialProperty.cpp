#include "GraphMaterialProperty.h"
#include "MkMaterial.h"
#include "IMkVertexDefinition.h"
#include "Graphs/NodeGraph.h"
#include "Logger.h"
#include "CompositorMaterialAssetReference.h"
#include "MaterialAssetReference.h"
#include "ShapeMaterialAssetReference.h"
#include "MikanModelResourceManager.h"
#include "LocText.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "NodeEditorState.h"
#include "Nodes/MaterialNode.h"
#include "Pins/FloatPin.h"
#include "Pins/TexturePin.h"
#include "IEditorWindow.h"
#include "MikanShaderCache.h"
#include "MikanShaderConfig.h"

#include "imgui.h"
#include "IconsForkAwesome.h"

// -- MaterialAssetComboDataSource ---
class MaterialAssetComboDataSource : public MkGui::ComboBoxDataSource
{
public:
	MaterialAssetComboDataSource(GraphMaterialPropertyPtr ownerProperty)
	{
		auto ownerGraph= ownerProperty->getOwnerGraph();
		int listIndex= 0;

		currentAssetRef= ownerProperty->getMaterialAssetReference();

		for (AssetReferencePtr assetRef : ownerGraph->getAssetReferences())
		{
			auto matAssetRef= std::dynamic_pointer_cast<MaterialAssetReference>(assetRef);

			if (matAssetRef)
			{
				if (matAssetRef == currentAssetRef)
				{
					selectedAssetRefIndex= listIndex;
				}

				const std::string entryString=
					matAssetRef ? assetRef->getShortName() : locText("graphProperties.noAssetRef");
				ComboEntry entry= {matAssetRef, entryString};

				comboEntries.push_back(entry);
				listIndex++;
			}
		}
	}

	inline int getCurrentAssetIndex() const { return selectedAssetRefIndex; }

	inline MaterialAssetReferencePtr getEntryAssetRef(int index) { return comboEntries[index].assetReference; }

	virtual int getEntryCount() const override { return (int)comboEntries.size(); }

	virtual const std::string& getEntryDisplayString(int index) const override
	{
		return comboEntries[index].entryString;
	}

private:
	struct ComboEntry
	{
		MaterialAssetReferencePtr assetReference;
		std::string entryString;
	};

	MaterialAssetReferencePtr currentAssetRef;
	std::vector<ComboEntry> comboEntries;
	int selectedAssetRefIndex= -1;
};

// -- GraphMaterialPropertyConfig -----
configuru::Config GraphMaterialPropertyConfig::writeToJSON()
{
	configuru::Config pt= GraphPropertyConfig::writeToJSON();

	pt["asset_ref_index"]= assetRefIndex;

	return pt;
}

void GraphMaterialPropertyConfig::readFromJSON(const configuru::Config& pt)
{
	assetRefIndex= pt.get_or<int>("asset_ref_index", -1);

	GraphPropertyConfig::readFromJSON(pt);
}

// -- GraphMaterialProperty -----
GraphMaterialProperty::~GraphMaterialProperty() { unbindMaterialReloadListener(); }

bool GraphMaterialProperty::loadFromConfig(GraphPropertyConfigConstPtr propConfig, const NodeGraphConfig& graphConfig)
{
	if (GraphProperty::loadFromConfig(propConfig, graphConfig))
	{
		const auto& matPropConfig= std::static_pointer_cast<const GraphMaterialPropertyConfig>(propConfig);
		if (matPropConfig->assetRefIndex != -1)
		{
			auto assetRef= getOwnerGraph()->getAssetReferenceByIndex(matPropConfig->assetRefIndex);
			auto materialAssetRef= std::dynamic_pointer_cast<MaterialAssetReference>(assetRef);
			if (materialAssetRef)
			{
				setMaterialAssetReference(materialAssetRef);
				return true;
			}
			else
			{
				MIKAN_LOG_ERROR("GraphMaterialProperty::loadFromConfig")
					<< "Invalid material asset reference: " << matPropConfig->assetRefIndex;
				setMaterialAssetReference(MaterialAssetReferencePtr());
			}
		}
		else
		{
			// Config says the property had an empty asset reference
			setMaterialAssetReference(MaterialAssetReferencePtr());
			return true;
		}
	}

	return false;
}

void GraphMaterialProperty::saveToConfig(GraphPropertyConfigPtr config) const
{
	auto propConfig= std::static_pointer_cast<GraphMaterialPropertyConfig>(config);

	// Default asset ref to invalid
	propConfig->assetRefIndex= -1;

	// If we have a valid asset ref, look up the index in the graph
	if (m_materialAssetRef)
	{
		propConfig->assetRefIndex= getOwnerGraph()->getAssetReferenceIndex(m_materialAssetRef);

		if (propConfig->assetRefIndex == -1)
		{
			MIKAN_LOG_ERROR("GraphMaterialProperty::saveToConfig")
				<< "Material property has orphaned asset reference: " << m_materialAssetRef->getInternalAssetPath();
		}
	}

	GraphProperty::saveToConfig(config);
}

void GraphMaterialProperty::setMaterialAssetReference(MaterialAssetReferencePtr inAssetRef)
{
	if (m_materialAssetRef != inAssetRef)
	{
		m_materialAssetRef= inAssetRef;

		// Drop the old resource before loading the new one
		unbindMaterialReloadListener();
		m_materialResource= MkMaterialPtr();
		m_domain= eMaterialDomain::INVALID;

		// re-create a material from the asset reference
		MikanShaderCache* shaderCache= getShaderCache();
		if (m_materialAssetRef && !m_materialAssetRef->isEmpty() && shaderCache != nullptr)
		{
			MikanShaderConfig materialConfig;
			m_materialResource= shaderCache->loadMaterialAssetReference(m_materialAssetRef, &materialConfig);

			if (m_materialResource)
			{
				m_domain= MaterialDomainUtils::resolveMaterialDomain(materialConfig);

				// Follow the cache's reloads so consumers rebuild against the recompiled program
				m_listenedShaderCache= shaderCache;
				m_listenedShaderCache->OnMaterialReloaded+=
					MakeDelegate(this, &GraphMaterialProperty::onMaterialReloaded);
			}
		}
	}
}

bool GraphMaterialProperty::isCompatibleWithDomain(eMaterialDomain domain) const
{
	return m_domain == domain || m_domain == eMaterialDomain::INVALID;
}

const std::string& GraphMaterialProperty::getUniformPinClassName(eUniformDataType dataType)
{
	static const std::string k_unsupported;

	switch (dataType)
	{
	case eUniformDataType::datatype_float:
		return FloatPin::k_pinClassName;
	case eUniformDataType::datatype_float2:
		return Float2Pin::k_pinClassName;
	case eUniformDataType::datatype_float3:
		return Float3Pin::k_pinClassName;
	case eUniformDataType::datatype_float4:
		return Float4Pin::k_pinClassName;
	case eUniformDataType::datatype_texture:
		return TexturePin::k_pinClassName;
	default:
		return k_unsupported;
	}
}

void GraphMaterialProperty::initDynamicPinFromMaterialDefault(NodePinPtr pin, MkMaterialConstPtr material)
{
	if (!pin || !material)
		return;

	const std::string& uniformName= pin->getName();
	if (auto floatPin= std::dynamic_pointer_cast<FloatPin>(pin))
	{
		float value;
		if (material->getFloatByUniformName(uniformName, value))
			floatPin->setValue(value);
	}
	else if (auto float2Pin= std::dynamic_pointer_cast<Float2Pin>(pin))
	{
		glm::vec2 value;
		if (material->getVec2ByUniformName(uniformName, value))
			float2Pin->setValue({value.x, value.y});
	}
	else if (auto float3Pin= std::dynamic_pointer_cast<Float3Pin>(pin))
	{
		glm::vec3 value;
		if (material->getVec3ByUniformName(uniformName, value))
			float3Pin->setValue({value.x, value.y, value.z});
	}
	else if (auto float4Pin= std::dynamic_pointer_cast<Float4Pin>(pin))
	{
		glm::vec4 value;
		if (material->getVec4ByUniformName(uniformName, value))
			float4Pin->setValue({value.x, value.y, value.z, value.w});
	}
}

MikanShaderCache* GraphMaterialProperty::getShaderCache() const
{
	IEditorWindow* ownerWindow= m_ownerGraph ? m_ownerGraph->getOwnerWindow() : nullptr;
	MikanModelResourceManager* resourceManager= ownerWindow ? ownerWindow->getModelResourceManager() : nullptr;

	return resourceManager ? resourceManager->getShaderCache() : nullptr;
}

void GraphMaterialProperty::unbindMaterialReloadListener()
{
	if (m_listenedShaderCache != nullptr)
	{
		m_listenedShaderCache->OnMaterialReloaded-= MakeDelegate(this, &GraphMaterialProperty::onMaterialReloaded);
		m_listenedShaderCache= nullptr;
	}
}

void GraphMaterialProperty::onMaterialReloaded(MkMaterialPtr material)
{
	if (!material || material != m_materialResource)
		return;

	// The recompile may have moved the material to another domain
	MikanShaderConfig materialConfig;
	if (materialConfig.load(m_materialAssetRef->getInternalAssetPath()))
	{
		m_domain= MaterialDomainUtils::resolveMaterialDomain(materialConfig);
	}

	notifyPropertyModified();
}

bool GraphMaterialProperty::editorOpenSourceGraph() const
{
	if (!m_materialAssetRef || !m_materialAssetRef->editorCanOpen())
	{
		MIKAN_LOG_INFO("GraphMaterialProperty::editorOpenSourceGraph")
			<< "Material has no source graph to open: "
			<< (m_materialAssetRef ? m_materialAssetRef->getInternalAssetPath().string() : std::string("<none>"));
		return false;
	}

	m_materialAssetRef->editorOpen();
	return true;
}

void GraphMaterialProperty::editorHandleMainFrameDragDrop(const NodeEditorState& editorState)
{
	auto materialNode= m_ownerGraph->createTypedNode<MaterialNode>(editorState);

	// Set this as the source model property for the new node
	auto self= std::static_pointer_cast<GraphMaterialProperty>(shared_from_this());
	materialNode->setMaterialSource(self);
}

void GraphMaterialProperty::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locLabel("graphProperties.materialHeader")))
	{
		// Name: the material file, not its full path
		const std::string name= m_materialAssetRef ? m_materialAssetRef->getShortName() : std::string();
		MkGui::drawStaticTextProperty(propertyStyle, locText("graphProperties.name"), name);

		// Material Asset
		MaterialAssetComboDataSource dataSource(std::static_pointer_cast<GraphMaterialProperty>(shared_from_this()));
		int selectedIndex= dataSource.getCurrentAssetIndex();
		if (MkGui::drawComboBoxProperty(propertyStyle, "materialSelection", locText("graphProperties.material"),
										&dataSource, selectedIndex))
		{
			setMaterialAssetReference(dataSource.getEntryAssetRef(selectedIndex));
		}

		// Drag-Drop Handling: the payload carries the project catalog's instance,
		// while this property binds to the graph's own reference by index
		// Either domain's material class may arrive; the graph accepts only the
		// class its own factories registered
		auto droppedAssetRef= MkGui::receiveTypedDragDropPayload<MaterialAssetReference>(
			CompositorMaterialAssetReference::k_assetClassName);
		if (!droppedAssetRef)
		{
			droppedAssetRef= MkGui::receiveTypedDragDropPayload<MaterialAssetReference>(
				ShapeMaterialAssetReference::k_assetClassName);
		}
		NodeGraphPtr ownerGraph= getOwnerGraph();
		if (droppedAssetRef && ownerGraph)
		{
			AssetReferencePtr graphAssetRef= ownerGraph->findOrAddAssetReference(
				droppedAssetRef->getClassName(), droppedAssetRef->getInternalAssetPath());
			if (graphAssetRef)
			{
				setMaterialAssetReference(std::static_pointer_cast<MaterialAssetReference>(graphAssetRef));
			}
		}
	}
}