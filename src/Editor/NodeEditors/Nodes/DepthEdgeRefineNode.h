#pragma once

#include "DepthPreviewRenderer.h"
#include "LocText.h"
#include "MikanRendererFwd.h"
#include "Node.h"

#include "glm/ext/vector_float2.hpp"

class DepthEdgeRefineNodeConfig : public NodeConfig
{
public:
	DepthEdgeRefineNodeConfig()
		: NodeConfig()
	{
	}
	DepthEdgeRefineNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	int bandRadius= 16;     // pixels at the mask's resolution, clamped to the shader's bound
	float colorSigma= 0.2f; // linear colour distance below which the two sides of an edge are not told apart
	float depthJump= 0.05f; // (max - min) / max over the window that marks a pixel as near an edge
	DepthPreviewSettings depthPreviewSettings;
};

/// Snaps the silhouette edges of a linear depth mask to the video frame.
///
/// The depth mask rendered from the occlusion mesh is a coarse prior: its
/// edges sit tens of pixels from the picture's. Within a band around every
/// depth discontinuity this node re-decides each pixel's side by a joint
/// bilateral vote guided by the video's colour, and gives it that side's depth,
/// so the mask's edge follows the frame while the depth values stay the mask's
/// own. The output keeps the input's format and convention, so WriteDepthNode
/// and the depth-blend materials consume it unchanged.
class DepthEdgeRefineNode : public Node
{
public:
	inline static const std::string k_nodeClassName= "DepthEdgeRefineNode";
	// The GLSL loop bound; the config clamps to the same value
	inline static const int k_minBandRadius= 1;
	inline static const int k_maxBandRadius= 32;

	DepthEdgeRefineNode();
	virtual ~DepthEdgeRefineNode();

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	virtual std::string getClassName() const override { return k_nodeClassName; }
	virtual void setOwnerGraph(NodeGraphPtr ownerGraph) override;

	virtual bool evaluateNode(NodeEvaluator& evaluator) override;

	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual std::string editorGetTitle() const override { return locText("nodes.depthEdgeRefineTitle"); }
	virtual const char* editorGetHeaderIcon() const override;

	void onGraphLoaded(bool success);
	void setDepthTexturePin(TexturePinPtr pin) { m_depthTexturePin= pin; }
	void setVideoTexturePin(TexturePinPtr pin) { m_videoTexturePin= pin; }
	void setOutputTexturePin(TexturePinPtr pin) { m_outTexturePin= pin; }

	bool ensureResources(NodeEvaluator& evaluator, int width, int height);
	// One fullscreen pass of the horizontal or vertical min/max shader
	bool renderMinMaxPass(NodeEvaluator& evaluator, IMkFrameBufferPtr target, MkMaterialInstancePtr materialInstance,
						  eUniformSemantic inputSemantic, IMkTexturePtr input, const glm::vec2& size);
	bool renderRefinePass(NodeEvaluator& evaluator, IMkTexturePtr depth, IMkTexturePtr video,
						  IMkTexturePtr windowMinMax, const glm::vec2& size);

protected:
	TexturePinPtr m_depthTexturePin;
	TexturePinPtr m_videoTexturePin;
	TexturePinPtr m_outTexturePin;

	// Passes 1 and 2 are colour buffers holding min and max; pass 3 is a
	// depth buffer so the output pin carries the same texture type as the mask
	IMkFrameBufferPtr m_minMaxHFrameBuffer;
	IMkFrameBufferPtr m_minMaxVFrameBuffer;
	IMkFrameBufferPtr m_refinedFrameBuffer;
	MkMaterialInstancePtr m_minMaxHMaterialInstance;
	MkMaterialInstancePtr m_minMaxVMaterialInstance;
	MkMaterialInstancePtr m_refineMaterialInstance;

	int m_bandRadius= 16;
	float m_colorSigma= 0.2f;
	float m_depthJump= 0.05f;

	// Editor preview only. The output pin always carries the untouched refined depth.
	DepthPreviewSettings m_depthPreviewSettings;
	DepthPreviewRenderer m_depthPreviewRenderer;

	friend class DepthEdgeRefineNodeFactory;
};

class DepthEdgeRefineNodeFactory : public TypedNodeFactory<DepthEdgeRefineNode, DepthEdgeRefineNodeConfig>
{
public:
	DepthEdgeRefineNodeFactory()= default;

	virtual NodePtr createNode(const class NodeEditorState& editorState) const override;
};
