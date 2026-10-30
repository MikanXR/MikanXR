#pragma once

#include "DepthPreviewRenderer.h"
#include "LocText.h"
#include "MikanRendererFwd.h"
#include "MkShaderConstants.h"
#include "Node.h"

#include "glm/ext/vector_float2.hpp"

class MatteDepthRefineNodeConfig : public NodeConfig
{
public:
	MatteDepthRefineNodeConfig()
		: NodeConfig()
	{
	}
	MatteDepthRefineNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	int bandRadius= 64; // pixels at the mask's resolution, clamped to the shaders' bound
	DepthPreviewSettings depthPreviewSettings;
};

/// Refines a linear depth mask with the frame's occlusion matte.
///
/// The matte labels each pixel keep, added or removed inside a band around the
/// mesh's occluding contours. An added pixel takes the window's nearest mesh
/// depth, which is the object the matte extended; a removed pixel takes the
/// window's farthest, the surface behind or the far plane; a kept pixel keeps
/// the mask's own depth. The output keeps the input's format and convention,
/// so WriteDepthNode and the depth mask materials consume it unchanged. With
/// no matte for the pending frame the mask passes through with a warning.
class MatteDepthRefineNode : public Node
{
public:
	inline static const std::string k_nodeClassName= "MatteDepthRefineNode";
	// The GLSL loop bound; the config clamps to the same value
	inline static const int k_minBandRadius= 1;
	inline static const int k_maxBandRadius= 64;

	MatteDepthRefineNode();
	virtual ~MatteDepthRefineNode();

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	virtual std::string getClassName() const override { return k_nodeClassName; }

	virtual void setOwnerGraph(NodeGraphPtr ownerGraph) override;
	virtual bool evaluateNode(NodeEvaluator& evaluator) override;
	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual std::string editorGetTitle() const override { return locText("nodes.matteDepthRefineTitle"); }
	virtual const char* editorGetHeaderIcon() const override;

	void onGraphLoaded(bool success);
	void setDepthTexturePin(TexturePinPtr pin) { m_depthTexturePin= pin; }
	void setOutputTexturePin(TexturePinPtr pin) { m_outTexturePin= pin; }

	bool ensureResources(NodeEvaluator& evaluator, int width, int height);
	bool renderMinMaxPass(NodeEvaluator& evaluator, IMkFrameBufferPtr target, MkMaterialInstancePtr materialInstance,
						  eUniformSemantic inputSemantic, IMkTexturePtr input, const glm::vec2& size);
	bool renderRefinePass(NodeEvaluator& evaluator, IMkTexturePtr depth, IMkTexturePtr matte,
						  IMkTexturePtr windowMinMax, const glm::vec2& size);

protected:
	TexturePinPtr m_depthTexturePin;
	TexturePinPtr m_outTexturePin;

	// Passes 1 and 2 are colour buffers holding min and max; pass 3 is a
	// depth buffer so the output pin carries the same texture type as the mask
	IMkFrameBufferPtr m_minMaxHFrameBuffer;
	IMkFrameBufferPtr m_minMaxVFrameBuffer;
	IMkFrameBufferPtr m_refinedFrameBuffer;
	MkMaterialInstancePtr m_minMaxHMaterialInstance;
	MkMaterialInstancePtr m_minMaxVMaterialInstance;
	MkMaterialInstancePtr m_refineMaterialInstance;

	int m_bandRadius= 64;
	bool m_bHadMatte= false;

	// Editor preview only. The output pin always carries the untouched refined depth.
	DepthPreviewSettings m_depthPreviewSettings;
	DepthPreviewRenderer m_depthPreviewRenderer;

	friend class MatteDepthRefineNodeFactory;
};

class MatteDepthRefineNodeFactory : public TypedNodeFactory<MatteDepthRefineNode, MatteDepthRefineNodeConfig>
{
public:
	MatteDepthRefineNodeFactory()= default;

	virtual NodePtr createNode(const class NodeEditorState& editorState) const override;
};
