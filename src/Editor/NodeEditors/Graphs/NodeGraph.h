#pragma once

#include "AssetFwd.h"
#include "AssetReference.h"
#include "CommonConfig.h"
#include "NodeFwd.h"
#include "Graphs/GraphPage.h"
#include "Nodes/Node.h"
#include "ObjectSystemFwd.h"
#include "Pins/NodePin.h"
#include "Pins/NodePinConstants.h"
#include "Properties/GraphProperty.h"
#include "MulticastDelegate.h"
#include "StringUtils.h"

#include <filesystem>
#include <map>
#include <string>

class NodeGraphConfig : public CommonConfig
{
public:
	NodeGraphConfig();
	NodeGraphConfig(const std::string& graphName);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	bool postReadFromJSON(NodeGraphPtr graph);

	std::string className;
	int nextId= -1;

	// Graph-level settings a subclass persists (a material graph's domain and
	// vertex preset). Read and written by the subclass's loadFromConfig/saveToConfig.
	configuru::Config settings= configuru::Config::object();

	std::vector<AssetReferenceConfigPtr> assetRefConfigs;
	std::vector<GraphPageConfigPtr> pageConfigs;
	std::map<t_graph_property_id, GraphPropertyConfigPtr> propertyConfigMap;
	std::vector<NodeConfigPtr> nodeConfigs;
	std::vector<NodePinConfigPtr> pinConfigs;
	std::vector<NodeLinkConfigPtr> linkConfigs;

protected:
	configuru::Config _assetRefsConfigObject;
	configuru::Config _pagesConfigObject;
	configuru::Config _propertiesConfigObject;
	configuru::Config _nodesConfigObject;
	configuru::Config _pinsConfigObject;
};

class NodeGraph : public std::enable_shared_from_this<NodeGraph>
{
public:
	NodeGraph();
	virtual ~NodeGraph();

	virtual std::string getClassName() const { return "NodeGraph"; }

	inline void setOwnerWindow(class IEditorWindow* window) { m_ownerWindow= window; }
	inline class IEditorWindow* getOwnerWindow() const { return m_ownerWindow; }

	ProjectManagerPtr getOwnerProject() const;
	template <class t_object_system_type>
	std::shared_ptr<t_object_system_type> getObjectSystemOfType() const
	{
		return getOwnerProject()->getSystemOfType<t_object_system_type>();
	}

	virtual bool createResources() { return true; }
	virtual void disposeResources() {}

	// Generates a unique ID for each node object newly created in the editor
	int allocateId();

	virtual void editorRender(const class NodeEditorState& editorState);
	// The Details panel content when nothing in the graph is selected
	virtual void editorRenderGraphPropertySheet(const class NodeEditorState& editorState) {}

	// -- Loading -----

	virtual bool loadFromConfig(const NodeGraphConfig& config);
	virtual GraphPropertyPtr loadGraphPropertyFromConfig(GraphPropertyConfigPtr propConfig,
														 const NodeGraphConfig& graphConfig);
	virtual bool loadAssetRefFromConfig(AssetReferenceConfigPtr assetRefConfig);
	virtual bool loadPageFromConfig(GraphPageConfigPtr pageConfig);
	virtual bool allocateNodeFromConfig(NodeConfigPtr nodeConfig);
	virtual bool loadNodeFromConfig(NodeConfigPtr nodeConfig);
	virtual bool allocatePinFromConfig(NodePinConfigPtr pinConfig);
	virtual bool loadPinFromConfig(NodePinConfigPtr pinConfig);
	virtual bool allocateLinkFromConfig(NodeLinkConfigPtr linkConfig);
	virtual bool loadLinkFromConfig(NodeLinkConfigPtr linkConfig);

	MulticastDelegate<void(bool success)> OnGraphLoaded;

	// -- Saving -----

	// Serialize the whole graph to JSON (the undo snapshot format)
	configuru::Config saveToSnapshotConfig() const;
	std::string saveToSnapshotString() const;

	virtual void saveToConfig(NodeGraphConfig& config) const;
	virtual void saveGraphPropertyToConfig(GraphPropertyConstPtr prop, NodeGraphConfig& graphConfig) const;
	virtual void saveAssetRefToConfig(AssetReferenceConstPtr assetRef, NodeGraphConfig& graphConfig) const;
	virtual void savePageToConfig(GraphPageConstPtr page, NodeGraphConfig& graphConfig) const;
	virtual void saveNodeToConfig(NodeConstPtr node, NodeGraphConfig& graphConfig) const;
	virtual void savePinToConfig(NodePinConstPtr pin, NodeGraphConfig& graphConfig) const;
	virtual void saveLinkToConfig(NodeLinkConstPtr link, NodeGraphConfig& graphConfig) const;

	// -- Assets References -----

	virtual std::vector<AssetReferenceFactoryPtr> editorGetValidAssetRefFactories(
		const class NodeEditorState& editorState) const;

	template <class t_asset_factory>
	void addAssetReferenceFactory()
	{
		auto factory= AssetReferenceFactory::createFactory<t_asset_factory>();
		std::string className= factory->getAssetRefClassName();

		m_assetRefFactories.insert({className, factory});
	}

	inline const std::map<std::string, AssetReferenceFactoryPtr>& getAssetReferenceFactories() const
	{
		return m_assetRefFactories;
	}

	AssetReferenceFactoryPtr getAssetReferenceFactory(const std::string assetRefClassName) const
	{
		auto it= m_assetRefFactories.find(assetRefClassName);

		return (it != m_assetRefFactories.end()) ? it->second : AssetReferenceFactoryPtr();
	}

	int getAssetReferenceIndex(AssetReferencePtr assetRef) const;

	inline AssetReferencePtr getAssetReferenceByIndex(int index) const
	{
		return (index >= 0 && index < (int)m_assetReferences.size()) ? m_assetReferences[index] : AssetReferencePtr();
	}

	inline const std::vector<AssetReferencePtr>& getAssetReferences() const { return m_assetReferences; }

	inline std::vector<AssetReferencePtr>& getAssetReferencesMutable() { return m_assetReferences; }

	// template <class t_asset_ref_type>
	// std::shared_ptr<t_asset_ref_type> addTypedAssetReference()
	//{
	//	auto assetRef = std::make_shared<t_asset_ref_type>(shared_from_this());

	//	m_assetReferences.push_back(assetRef);

	//	if (OnAssetReferenceCreated)
	//		OnAssetReferenceCreated(assetRef->getId());

	//	return assetRef;
	//}

	// The graph's own reference to the given asset, appended when the graph does
	// not already hold one. Graph properties bind to the graph's instance by index
	// at save time, so an asset arriving from the project catalog is mapped through
	// this before anything binds to it.
	AssetReferencePtr findOrAddAssetReference(const std::string& className, const std::filesystem::path& assetPath);

	bool deleteAssetReference(AssetReferencePtr assetRef);

	MulticastDelegate<void(AssetReferencePtr assetRef)> OnAssetReferenceCreated;
	MulticastDelegate<void(AssetReferencePtr assetRef)> OnAssetReferenceDeleted;

	// -- Pages -----

	// The implicit page every graph has; nodes of a graph without pages all sit on it
	inline static const t_graph_page_id k_rootPageId= 0;

	template <class t_page_factory>
	void addPageFactory()
	{
		auto factory= GraphPageFactory::createFactory<t_page_factory>();

		m_pageFactories.insert({factory->getPageClassName(), factory});
	}

	GraphPageFactoryPtr getPageFactory(const std::string& pageClassName) const
	{
		auto it= m_pageFactories.find(pageClassName);

		return (it != m_pageFactories.end()) ? it->second : GraphPageFactoryPtr();
	}

	inline const std::map<std::string, GraphPageFactoryPtr>& getPageFactories() const { return m_pageFactories; }
	inline bool hasPageFactories() const { return !m_pageFactories.empty(); }

	// Creates a page and lets it seed its nodes with the editor state pointed at the new page
	GraphPagePtr createPage(GraphPageFactoryPtr pageFactory, const class NodeEditorState& editorState);

	template <class t_page_class>
	std::shared_ptr<t_page_class> createTypedPage(const class NodeEditorState& editorState)
	{
		auto pageFactory= getPageFactory(t_page_class::k_pageClassName);
		if (pageFactory)
		{
			return std::static_pointer_cast<t_page_class>(createPage(pageFactory, editorState));
		}

		return std::shared_ptr<t_page_class>();
	}

	// Deletes the page and every node on it (the root page cannot be deleted)
	bool deletePage(t_graph_page_id id);
	GraphPagePtr getPageById(t_graph_page_id id) const;
	// True for the root page and every created page
	bool hasPage(t_graph_page_id id) const;
	inline const std::map<t_graph_page_id, GraphPagePtr>& getPages() const { return m_pages; }
	std::vector<NodePtr> getNodesOnPage(t_graph_page_id id) const;
	// Raised by a page's property sheet after an edit that its dependents need to see
	void notifyPageModified(t_graph_page_id id);

	MulticastDelegate<void(t_graph_page_id id)> OnPageCreated;
	MulticastDelegate<void(t_graph_page_id id)> OnPageModified;
	MulticastDelegate<void(t_graph_page_id id)> OnPageDeleted;

	// -- Properties -----

	virtual std::vector<GraphPropertyFactoryPtr> editorGetValidPropertyFactories(
		const class NodeEditorState& editorState) const;

	template <class t_property_factory>
	void addPropertyFactory()
	{
		auto factory= GraphPropertyFactory::createFactory<t_property_factory>();
		std::string className= factory->getGraphPropertyClassName();

		m_propertyFactories.insert({className, factory});
	}

	GraphPropertyFactoryPtr getPropertyFactory(const std::string propertyClassName) const
	{
		auto it= m_propertyFactories.find(propertyClassName);

		return (it != m_propertyFactories.end()) ? it->second : GraphPropertyFactoryPtr();
	}

	GraphPropertyPtr getPropertyById(t_graph_property_id id) const;
	GraphPropertyPtr getPropertyByName(const std::string& name) const;

	inline const std::map<t_graph_property_id, GraphPropertyPtr>& getPropertyMap() const { return m_properties; }

	template <class t_property_type>
	std::shared_ptr<t_property_type> getTypedPropertyById(t_graph_property_id id) const
	{
		return std::dynamic_pointer_cast<t_property_type>(getPropertyById(id));
	}

	template <class t_property_type>
	std::shared_ptr<t_property_type> getTypedPropertyByName(const std::string& name) const
	{
		return std::dynamic_pointer_cast<t_property_type>(getPropertyByName(name));
	}

	template <class t_property_type>
	std::shared_ptr<t_property_type> createTypedProperty()
	{
		auto property= std::make_shared<t_property_type>();

		initNewProperty(property);
		addProperty(property);

		return property;
	}

	GraphPropertyPtr createProperty(GraphPropertyFactoryPtr propertyFactory);
	void addProperty(GraphPropertyPtr property);
	bool deletePropertyById(t_graph_property_id id);

	// Appends a numeric suffix when baseName is already taken by a property
	// other than the excluded one (the property being renamed to it)
	std::string makeUniquePropertyName(const std::string& baseName, t_graph_property_id excludePropertyId= -1) const;
	// The one rename path for a property: the requested name is made unique
	// against the others, so every rename surface agrees. True when the name changed.
	bool renameProperty(t_graph_property_id propertyId, const std::string& requestedName);

	// Moves a property to the target's slot in the variable list ordering,
	// renumbering every property's sort order to match the result
	bool reorderPropertyBefore(t_graph_property_id movedId, t_graph_property_id targetId);

	// Properties in variable list order (sort order, then id for legacy graphs)
	std::vector<GraphPropertyPtr> getPropertiesInSortOrder() const;

	MulticastDelegate<void(t_graph_property_id id)> OnPropertyCreated;
	MulticastDelegate<void(t_graph_property_id id)> OnPropertyModifed;
	MulticastDelegate<void(t_graph_property_id id)> OnPropertyDeleted;

	// -- Nodes -----

	virtual std::vector<NodeFactoryPtr> editorGetValidNodeFactories(const class NodeEditorState& editorState) const;

	template <class t_node_factory, class... t_args>
	void addNodeFactory(t_args&&... args)
	{
		addNodeFactory(NodeFactory::createFactory<t_node_factory>(std::forward<t_args>(args)...));
	}

	// Factories are keyed by NodeFactory::getFactoryKey, so several variant
	// factories can share one node class. Loading and saving resolve by class
	// name to the first factory registered for that class.
	void addNodeFactory(NodeFactoryPtr factory)
	{
		m_nodeFactories.insert({factory->getFactoryKey(), factory});
		m_nodeFactoriesByClassName.insert({factory->getNodeClassName(), factory});
	}

	// Removes one factory by key; the class stays loadable while another factory of it remains
	void removeNodeFactory(const std::string& factoryKey);

	// Accepts a factory key or a node class name
	NodeFactoryPtr getNodeFactory(const std::string nodeClassName) const
	{
		auto it= m_nodeFactories.find(nodeClassName);
		if (it != m_nodeFactories.end())
			return it->second;

		auto classIt= m_nodeFactoriesByClassName.find(nodeClassName);
		return (classIt != m_nodeFactoriesByClassName.end()) ? classIt->second : NodeFactoryPtr();
	}

	template <class _Pr>
	NodePtr getNodeByPredicate(_Pr predicate) const
	{
		auto it= std::find_if(m_Nodes.begin(), m_Nodes.end(), predicate);
		if (it != m_Nodes.end())
		{
			return it->second;
		}

		return NodePtr();
	}
	NodePtr getNodeById(t_node_id id) const;
	NodePtr getEventNodeByName(const std::string& eventName) const;
	const std::map<t_node_id, NodePtr>& getNodesMap() const { return m_Nodes; }

	using NodeConstVisitor= std::function<void(NodeConstPtr nodePtr)>;
	void visitAllNodes(const NodeConstVisitor& visitor) const;

	template <class t_node_class>
	std::shared_ptr<t_node_class> createTypedNode(const NodeEditorState& nodeEditorState)
	{
		auto nodeFactory= getNodeFactory(t_node_class::k_nodeClassName);
		if (nodeFactory)
		{
			return std::static_pointer_cast<t_node_class>(createNode(nodeFactory, nodeEditorState));
		}

		return std::shared_ptr<t_node_class>();
	}

	NodePtr createNode(NodeFactoryPtr nodeFactory, const NodeEditorState& nodeEditorState);
	MulticastDelegate<void(t_node_id id)> OnNodeCreated;

	void addNode(NodePtr node);
	bool deleteNodeById(t_node_id id);
	MulticastDelegate<void(t_node_id id)> OnNodeDeleted;

	// -- Pins -----

	template <class t_pin_class, class t_pin_config_class>
	void addPinFactory()
	{
		auto factory= NodePinFactory::createFactory<TypedNodePinFactory<t_pin_class, t_pin_config_class>>();
		std::string pinClassName= factory->getPinClassName();

		m_pinFactories.insert({pinClassName, factory});
	}

	NodePinFactoryPtr getPinFactory(const std::string pinClassName) const
	{
		auto it= m_pinFactories.find(pinClassName);

		return (it != m_pinFactories.end()) ? it->second : NodePinFactoryPtr();
	}

	NodePinPtr getPinById(t_node_pin_id id) const;
	const std::map<t_node_pin_id, NodePinPtr>& getPinsMap() const { return m_Pins; }

	void addPin(NodePinPtr newPin);
	MulticastDelegate<void(t_node_pin_id id)> OnPinCreated;

	bool deletePinById(t_node_pin_id id);
	MulticastDelegate<void(t_node_pin_id id)> OnPinDeleted;

	// -- Links -----

	NodeLinkPtr getLinkById(t_node_link_id id) const;
	const std::map<t_node_link_id, NodeLinkPtr>& getLinksMap() const { return m_Links; }

	NodeLinkPtr createLink(t_node_pin_id startPinId, t_node_pin_id endPinId);
	MulticastDelegate<void(t_node_link_id id)> OnLinkCreated;

	bool deleteLinkById(t_node_link_id id);
	MulticastDelegate<void(t_node_link_id id)> OnLinkDeleted;

protected:
	// Assigns the owner graph, a fresh id, and the default display name shared
	// by every property creation path
	void initNewProperty(GraphPropertyPtr property);

	// The window that created this node graph
	class IEditorWindow* m_ownerWindow= nullptr;

	// Defines all of the asset references that nodes in this graph can use
	std::map<std::string, AssetReferenceFactoryPtr> m_assetRefFactories;

	// Defines all of the property types that this graph can have
	std::map<std::string, GraphPropertyFactoryPtr> m_propertyFactories;

	// Defines the page types this graph can hold (none means a single root page)
	std::map<std::string, GraphPageFactoryPtr> m_pageFactories;

	// Defines all of the node types that this node graph can use
	std::map<std::string, NodeFactoryPtr> m_nodeFactories;
	std::map<std::string, NodeFactoryPtr> m_nodeFactoriesByClassName;

	// Defines all of the pin types that this node graph can use
	std::map<std::string, NodePinFactoryPtr> m_pinFactories;

	//	List of asset references used by this graph
	std::vector<AssetReferencePtr> m_assetReferences;

	// Properties assigned to this node graph
	std::map<t_graph_property_id, GraphPropertyPtr> m_properties;

	// Pages other than the implicit root
	std::map<t_graph_page_id, GraphPagePtr> m_pages;

	// Nodes, pins and links that make up the graph
	std::map<t_node_id, NodePtr> m_Nodes;
	std::map<t_node_pin_id, NodePinPtr> m_Pins;
	std::map<t_node_link_id, NodeLinkPtr> m_Links;

	int m_nextId= 0;
};

class NodeGraphFactory
{
public:
	NodeGraphFactory()= default;

	inline NodeGraphConstPtr getNodeDefaultObject() const { return m_nodeGraphDefaultObject; }
	inline std::string getNodeClassName() const { return m_nodeGraphDefaultObject->getClassName(); }

	virtual NodeGraphPtr allocateNodeGraph() const;
	virtual NodeGraphPtr initialCreateNodeGraph(class IEditorWindow* ownerWindow) const;

	// Loads the graph the file records. With an expected class name, a file of
	// another kind is refused rather than loaded, since the kinds are not
	// interchangeable (a shape graph cannot drive a compositor).
	static NodeGraphPtr loadNodeGraph(class IEditorWindow* ownerWindow, const std::filesystem::path& path,
									  const std::string& expectedClassName= std::string());
	// The class name a graph file records, without loading it. Empty when the
	// file cannot be read.
	static std::string peekGraphClassName(const std::filesystem::path& path);
	static NodeGraphPtr loadNodeGraphFromConfig(class IEditorWindow* ownerWindow, NodeGraphConfig& config);
	static NodeGraphPtr loadNodeGraphFromSnapshotString(class IEditorWindow* ownerWindow, const std::string& snapshot);
	static void saveNodeGraph(const std::filesystem::path& path, NodeGraphConstPtr nodeGraph);

	template <class t_node_factory_class>
	static void registerFactory()
	{
		auto factory= std::make_shared<t_node_factory_class>();

		// Create a single "NodeGraph default object" for the factory.
		// This is used to ask questions about NodeGraph without having to create one first.
		// We have to do this work outside of the NodeGraphFactory constructor,
		// because virtual functions aren't safe to call in constructor.
		factory->m_nodeGraphDefaultObject= factory->allocateNodeGraph();

		s_factoryMap.insert({factory->m_nodeGraphDefaultObject->getClassName(), factory});
	}

private:
	NodeGraphPtr m_nodeGraphDefaultObject;

	static std::map<std::string, NodeGraphFactoryPtr> s_factoryMap;
};

template <class t_node_graph_class>
class TypedNodeGraphFactory : public NodeGraphFactory
{
public:
	TypedNodeGraphFactory()= default;

	virtual NodeGraphPtr allocateNodeGraph() const override { return std::make_shared<t_node_graph_class>(); }
};