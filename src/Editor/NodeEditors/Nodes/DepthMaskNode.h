#pragma once

#include "ComponentFwd.h"
#include "CompositorConstants.h"
#include "DepthPreviewRenderer.h"
#include "LocText.h"
#include "Node.h"
#include "MikanRendererFwd.h"

/// The ID of a stencil
typedef int32_t MikanStencilID;

class DepthMaskNodeConfig : public NodeConfig
{
public:
	DepthMaskNodeConfig()
		: NodeConfig()
	{
	}
	DepthMaskNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	bool bDisableQuadStencils= false;
	bool bDisableBoxStencils= false;
	bool bDisableModelStencils= false;
	DepthPreviewSettings depthPreviewSettings;
};

class DepthMaskNode : public Node
{
public:
	DepthMaskNode();
	virtual ~DepthMaskNode();

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig);
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const;

	inline static const std::string k_nodeClassName= "DepthMaskNode";
	virtual std::string getClassName() const override { return k_nodeClassName; }
	virtual void setOwnerGraph(NodeGraphPtr ownerGraph) override;

	virtual bool evaluateNode(NodeEvaluator& evaluator);

	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	void evaluateQuadDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState);
	void evaluateBoxDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState);
	void evaluateModelDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState);

	virtual std::string editorGetTitle() const override { return locText("nodes.depthMaskTitle"); }
	virtual const char* editorGetHeaderIcon() const override;

	void onGraphLoaded(bool success);
	virtual void onLinkConnected(NodeLinkPtr link, NodePinPtr pin) override;
	virtual void onLinkDisconnected(NodeLinkPtr link, NodePinPtr pin) override;

	void setStencilsPin(ArrayPinPtr inPin);
	void setDepthTextureOutPin(TexturePinPtr outPin);
	void rebuildDepthMaskLists();

protected:
	ArrayPinPtr m_stencilsPin;
	std::vector<MikanStencilID> m_quadStencilIds;
	std::vector<MikanStencilID> m_boxStencilIds;
	std::vector<MikanStencilID> m_modelStencilIds;
	TexturePinPtr m_outDepthTexturePin;

	MkMaterialInstancePtr m_depthMaterialInstance;
	IMkFrameBufferPtr m_linearDepthFrameBuffer;

	bool m_bDisableQuadStencil= false;
	bool m_bDisableBoxStencil= false;
	bool m_bDisableModelStencil= false;

	// Editor preview only. The output pin always carries the untouched linear depth.
	DepthPreviewSettings m_depthPreviewSettings;
	DepthPreviewRenderer m_depthPreviewRenderer;

	friend class DepthMaskNodeFactory;
};

class DepthMaskNodeFactory : public TypedNodeFactory<DepthMaskNode, DepthMaskNodeConfig>
{
public:
	DepthMaskNodeFactory()= default;

	virtual NodePtr createNode(const class NodeEditorState& editorState) const override;
};