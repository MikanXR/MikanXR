#pragma once

#include "CompositorConstants.h"
#include "ComponentFwd.h"
#include "Node.h"
#include "MikanTypeFwd.h"
#include "MikanRendererFwd.h"

class ColorTextureSourceNodeConfig : public NodeConfig
{
public:
	ColorTextureSourceNodeConfig()= default;

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);

	eTextureSourceColorType textureSourceColorType;
	eColorTextureFallbackMode fallbackMode= eColorTextureFallbackMode::autoByType;
	eColorTextureResolveAlphaMode resolveAlphaMode= eColorTextureResolveAlphaMode::none;
	MikanTextureSourceID textureSourceId;
	bool bVerticalFlip;
};

class ColorTextureSourceNode : public Node
{
public:
	ColorTextureSourceNode()= default;

	inline static const std::string k_nodeClassName= "ColorTextureSourceNode";
	virtual std::string getClassName() const override { return k_nodeClassName; }

	virtual bool loadFromConfig(NodeConfigConstPtr nodeConfig) override;
	virtual void saveToConfig(NodeConfigPtr nodeConfig) const override;

	TextureSourceComponentPtr getTextureSourceComponent() const;
	IMkTexturePtr getTextureResource() const;

	virtual bool evaluateNode(NodeEvaluator& evaluator) override;
	virtual void editorRenderNode(const NodeEditorState& editorState) override;
	virtual void editorRenderPropertySheet(const NodeEditorState& editorState) override;

protected:
	IMkTexturePtr getColorSourceTexture() const;

	// The size the composite consumes this texture at, which is the video resolution.
	// A client rendering above it (client_color_render_scale) leaves more samples than
	// one bilinear tap can resolve, so the node filters them down itself.
	bool getResolveTargetSize(int& outWidth, int& outHeight) const;

	void updateColorFrameBuffer(NodeEvaluator& evaluator, IMkTexturePtr clientTexture);
	void evaluateColorTexture(IMkState* glState, IMkTexturePtr colorTexture);

	virtual ImVec4 editorGetHeaderColor() const override;
	virtual std::string editorGetTitle() const override;

protected:
	IMkFrameBufferPtr m_colorFrameBuffer;
	MkMaterialInstancePtr m_colorMaterialInstance;
	eTextureSourceColorType m_clientTextureType= eTextureSourceColorType::colorRGB;
	eColorTextureFallbackMode m_fallbackMode= eColorTextureFallbackMode::autoByType;
	eColorTextureResolveAlphaMode m_resolveAlphaMode= eColorTextureResolveAlphaMode::none;
	TextureSourceComponentWeakPtr m_textureSourceComponent;
	bool m_bVerticalFlip= false;
	// True while the framebuffer pass is downsampling rather than only flipping, which
	// selects the material and keeps the instance from being reused across the two
	bool m_bIsResolving= false;
};

class ColorTextureSourceNodeFactory : public TypedNodeFactory<ColorTextureSourceNode, ColorTextureSourceNodeConfig>
{
public:
	ColorTextureSourceNodeFactory()= default;

	virtual NodePtr createNode(const NodeEditorState& editorState) const override;
};