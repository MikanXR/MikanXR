#pragma once

#include "LocText.h"
#include "Node.h"
#include "ComponentFwd.h"

class StencilSelectNodeConfig : public NodeConfig
{
public:
	StencilSelectNodeConfig()
		: NodeConfig()
	{
	}
	StencilSelectNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	bool bEnableQuadStencils= true;
	bool bEnableBoxStencils= true;
	bool bEnableModelStencils= true;
};
using StencilSelectNodeConfigPtr= std::shared_ptr<StencilSelectNodeConfig>;
using StencilSelectNodeConfigConstPtr= std::shared_ptr<const StencilSelectNodeConfig>;

class StencilSelectNode : public Node
{
public:
	StencilSelectNode()= default;

	inline static const std::string k_nodeClassName= "StencilSelectNode";
	virtual std::string getClassName() const override { return k_nodeClassName; }

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	inline bool getEnableQuadStencils() const { return m_bEnableQuadStencils; }
	inline bool getEnableBoxStencils() const { return m_bEnableBoxStencils; }
	inline bool getEnableModelStencils() const { return m_bEnableModelStencils; }

	virtual bool evaluateNode(NodeEvaluator& evaluator) override;
	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual ImVec4 editorGetHeaderColor() const override;
	virtual const char* editorGetHeaderIcon() const override;
	virtual std::string editorGetTitle() const override { return locText("nodes.stencilSelectTitle"); }

protected:
	ArrayPinPtr m_stencilsOutPin;

	bool m_bEnableQuadStencils= true;
	bool m_bEnableBoxStencils= true;
	bool m_bEnableModelStencils= true;

	friend class StencilSelectNodeFactory;
};

class StencilSelectNodeFactory : public TypedNodeFactory<StencilSelectNode, StencilSelectNodeConfig>
{
public:
	StencilSelectNodeFactory()= default;

	virtual NodePtr createNode(const NodeEditorState& editorState) const override;
};
