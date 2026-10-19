#pragma once

#include "CompositorConstants.h"
#include "LocText.h"
#include "MikanRendererFwd.h"
#include "MkShaderConstants.h"
#include "Node.h"

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float4.hpp"

class DepthOfFieldNodeConfig : public NodeConfig
{
public:
	DepthOfFieldNodeConfig()
		: NodeConfig()
	{
	}
	DepthOfFieldNodeConfig(const std::string& nodeName)
		: NodeConfig(nodeName)
	{
	}

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	int maxRadius= 16; // pixels at the color's resolution, clamped to the shaders' bound
	eColorTextureResolveAlphaMode alphaMode= eColorTextureResolveAlphaMode::straight;
};

/// Blurs a client's color layer the way the physical lens blurs the video.
///
/// The blur radius of each pixel comes from the client's depth through the thin
/// lens model, with the focus distance and aperture of the bound camera and the
/// focal length of its intrinsics. A scatter-as-gather disc blur then spreads
/// each pixel by its radius in premultiplied color, so an out of focus silhouette
/// turns partially transparent over the video rather than stopping at its edge.
/// With no aperture set the node passes its color through untouched.
class DepthOfFieldNode : public Node
{
public:
	inline static const std::string k_nodeClassName= "DepthOfFieldNode";
	// The GLSL dilation loop bound; the config clamps to the same value
	inline static const int k_minRadius= 1;
	inline static const int k_maxRadius= 32;

	DepthOfFieldNode();
	virtual ~DepthOfFieldNode();

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	virtual std::string getClassName() const override { return k_nodeClassName; }
	virtual void setOwnerGraph(NodeGraphPtr ownerGraph) override;

	virtual bool evaluateNode(NodeEvaluator& evaluator) override;

	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	virtual std::string editorGetTitle() const override { return locText("nodes.depthOfFieldTitle"); }
	virtual const char* editorGetHeaderIcon() const override;

	void onGraphLoaded(bool success);
	void setColorTexturePin(TexturePinPtr pin) { m_colorTexturePin= pin; }
	void setDepthTexturePin(TexturePinPtr pin) { m_depthTexturePin= pin; }
	void setOutputTexturePin(TexturePinPtr pin) { m_outTexturePin= pin; }

	// The lens uniform the passes share: zNear, zFar, the circle of confusion scale for a
	// texture this wide, and 1 / focus distance. False when the bound camera has no aperture
	// or no intrinsics, which is the pass-through case.
	bool computeLensParams(float textureWidth, glm::vec4& outLensParams) const;
	bool ensureResources(NodeEvaluator& evaluator, int width, int height);
	bool renderCoCPass(NodeEvaluator& evaluator, IMkTexturePtr depth, const glm::vec4& lensParams);
	bool renderDilatePass(NodeEvaluator& evaluator, IMkFrameBufferPtr target, MkMaterialInstancePtr materialInstance,
						  eUniformSemantic inputSemantic, IMkTexturePtr input, const glm::vec2& size);
	bool renderGatherPass(NodeEvaluator& evaluator, IMkTexturePtr color, IMkTexturePtr depth, IMkTexturePtr reach,
						  const glm::vec4& lensParams, const glm::vec2& size);

protected:
	TexturePinPtr m_colorTexturePin;
	TexturePinPtr m_depthTexturePin;
	TexturePinPtr m_outTexturePin;

	// The circle of confusion, its separable max over the radius bound, and the blurred color
	IMkFrameBufferPtr m_cocFrameBuffer;
	IMkFrameBufferPtr m_dilateHFrameBuffer;
	IMkFrameBufferPtr m_dilateVFrameBuffer;
	IMkFrameBufferPtr m_gatherFrameBuffer;
	MkMaterialInstancePtr m_cocMaterialInstance;
	MkMaterialInstancePtr m_dilateHMaterialInstance;
	MkMaterialInstancePtr m_dilateVMaterialInstance;
	MkMaterialInstancePtr m_gatherMaterialInstance;

	int m_maxRadius= 16;
	eColorTextureResolveAlphaMode m_alphaMode= eColorTextureResolveAlphaMode::straight;

	friend class DepthOfFieldNodeFactory;
};

class DepthOfFieldNodeFactory : public TypedNodeFactory<DepthOfFieldNode, DepthOfFieldNodeConfig>
{
public:
	DepthOfFieldNodeFactory()= default;

	virtual NodePtr createNode(const class NodeEditorState& editorState) const override;
};
