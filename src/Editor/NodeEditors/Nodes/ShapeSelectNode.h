#pragma once

#include "LocText.h"
#include "Node.h"
#include "ComponentFwd.h"

class ShapeSelectNodeConfig : public NodeConfig
{
public:
	ShapeSelectNodeConfig()
		: NodeConfig()
	{
	}
	ShapeSelectNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	bool bEnableQuadShapes= true;
	bool bEnableBoxShapes= true;
	bool bEnableModelShapes= true;
};
using ShapeSelectNodeConfigPtr= std::shared_ptr<ShapeSelectNodeConfig>;
using ShapeSelectNodeConfigConstPtr= std::shared_ptr<const ShapeSelectNodeConfig>;

class ShapeSelectNode : public Node
{
public:
	ShapeSelectNode()= default;

	inline static const std::string k_nodeClassName= "ShapeSelectNode";
	virtual std::string getClassName() const override { return k_nodeClassName; }

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	inline bool getEnableQuadShapes() const { return m_bEnableQuadShapes; }
	inline bool getEnableBoxShapes() const { return m_bEnableBoxShapes; }
	inline bool getEnableModelShapes() const { return m_bEnableModelShapes; }

	virtual bool evaluateNode(NodeEvaluator& evaluator) override;
	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual ImVec4 editorGetHeaderColor() const override;
	virtual const char* editorGetHeaderIcon() const override;
	virtual std::string editorGetTitle() const override { return locText("nodes.shapeSelectTitle"); }

protected:
	ArrayPinPtr m_shapesOutPin;

	bool m_bEnableQuadShapes= true;
	bool m_bEnableBoxShapes= true;
	bool m_bEnableModelShapes= true;

	friend class ShapeSelectNodeFactory;
};

class ShapeSelectNodeFactory : public TypedNodeFactory<ShapeSelectNode, ShapeSelectNodeConfig>
{
public:
	ShapeSelectNodeFactory()= default;

	virtual NodePtr createNode(const NodeEditorState& editorState) const override;
};
