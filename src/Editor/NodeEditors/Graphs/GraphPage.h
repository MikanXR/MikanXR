#pragma once

#include "NodeFwd.h"
#include "CommonConfig.h"

#include <memory>
#include <string>

class GraphPageConfig : public CommonConfig
{
public:
	GraphPageConfig()= default;

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	std::string className;
	t_graph_page_id id= -1;
	std::string name;
};

// A named canvas inside one graph. The root page (id 0) is implicit and holds
// every node of a graph that has no pages; other pages are created through a
// page factory the graph subclass registers, and each node records the page it
// sits on. Pages share the graph's id space, undo history, and file.
class GraphPage : public std::enable_shared_from_this<GraphPage>
{
public:
	GraphPage()= default;
	virtual ~GraphPage()= default;

	inline static const std::string k_pageClassName= "GraphPage";
	virtual std::string getClassName() const { return k_pageClassName; }

	virtual bool loadFromConfig(GraphPageConfigConstPtr config);
	virtual void saveToConfig(GraphPageConfigPtr config) const;

	inline void setOwnerGraph(NodeGraphPtr ownerGraph) { m_ownerGraph= ownerGraph; }
	inline NodeGraphPtr getOwnerGraph() const { return m_ownerGraph; }

	inline void setId(t_graph_page_id id) { m_id= id; }
	inline t_graph_page_id getId() const { return m_id; }

	inline void setName(const std::string& name) { m_name= name; }
	inline const std::string& getName() const { return m_name; }

	// Called once after the page is created in the editor (never on load), so a
	// page can seed the nodes it needs
	virtual void onPageCreated(const class NodeEditorState& editorState) {}
	// Called after the whole graph has loaded, so a page can find its nodes
	virtual void onGraphLoaded() {}

	virtual std::string editorGetTitle() const { return m_name; }
	virtual const char* editorGetIcon() const;
	virtual void editorRenderPropertySheet(const class NodeEditorState& editorState) {}
	// Whether the Pages panel offers an inline rename, and the rename itself. A
	// page kind that validates its name overrides both; the base accepts nothing.
	virtual bool editorCanRename() const { return false; }
	virtual bool editorRename(const std::string& name) { return false; }

protected:
	NodeGraphPtr m_ownerGraph;
	t_graph_page_id m_id= -1;
	std::string m_name;
};

class GraphPageFactory
{
public:
	GraphPageFactory()= default;
	virtual ~GraphPageFactory()= default;

	inline GraphPageConstPtr getPageDefaultObject() const { return m_pageDefaultObject; }
	inline std::string getPageClassName() const { return m_pageDefaultObject->getClassName(); }

	virtual GraphPageConfigPtr allocatePageConfig() const { return std::make_shared<GraphPageConfig>(); }
	virtual GraphPagePtr allocatePage() const= 0;

	// The Pages panel's add button label
	virtual std::string editorGetCreateLabel() const;

	template <class t_factory_class>
	static GraphPageFactoryPtr createFactory()
	{
		auto factory= std::make_shared<t_factory_class>();

		// The default object answers questions about the class without a
		// graph, and cannot be built in the constructor where virtuals are unsafe
		factory->m_pageDefaultObject= factory->allocatePage();

		return factory;
	}

protected:
	GraphPagePtr m_pageDefaultObject;
};

template <class t_page_class, class t_page_config_class>
class TypedGraphPageFactory : public GraphPageFactory
{
public:
	TypedGraphPageFactory()= default;

	virtual GraphPageConfigPtr allocatePageConfig() const override { return std::make_shared<t_page_config_class>(); }
	virtual GraphPagePtr allocatePage() const override { return std::make_shared<t_page_class>(); }
};
