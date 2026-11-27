#pragma once

#include "NodeGraph.h"
#include "ComponentFwd.h"
#include "MikanRendererFwd.h"

#include <map>
#include <set>
#include <string>

typedef int32_t MikanStencilID;

// The scene objects a compositor graph pulls into its composite, by component id
struct CompositorGraphReferences
{
	std::set<int> stencilIds;
	std::set<int> shapeIds;
	// Read by the graph, or by the shape graph of a shape it pulls
	std::set<int> textureSourceIds;
	// Texture source nodes naming a source that no longer resolves
	int unresolvedTextureSourceCount= 0;
	// A texture source node reads whichever client publishes first for the camera
	bool bUsesFirstClientSource= false;
};

class CompositorNodeGraph : public NodeGraph
{
public:
	CompositorNodeGraph();

	virtual bool createResources() override;
	virtual void disposeResources() override;

	static const std::string k_compositeFrameEventName;

	inline static const std::string k_graphClassName= "CompositorNodeGraph";
	virtual std::string getClassName() const override { return k_graphClassName; }
	virtual bool loadFromConfig(const NodeGraphConfig& config) override;
	bool compositeFrame(NodeEvaluator& evaluator);
	IMkTextureConstPtr getCompositedFrameTexture() const;

	// Compositor Component
	bool bindToCompositorComponent(CompositorComponentPtr compositorComponent);
	void unbindFromCompositorComponent(CompositorComponentPtr compositorComponent);

	CompositorComponentPtr getBoundCompositorComponent() const;
	CameraComponentPtr getBoundCameraComponent() const;
	VideoSourceComponentPtr getBoundVideoSourceComponent() const;

	// Sources

	// What the OnCompositeFrame chain pulls, found by walking the graph rather than
	// evaluating it. A select node contributes everything of its enabled types the
	// bound compositor owns, ignoring camera facing and the disabled flag, which
	// change per frame.
	void gatherReferencedSceneObjects(CompositorGraphReferences& outReferences) const;

	// Stencil Models
	MikanRenderModelResourcePtr getOrLoadStencilRenderModel(ModelStencilDefinitionPtr stencilDefinition);
	MikanRenderModelResourcePtr getOrLoadDepthRenderModel(ModelStencilDefinitionPtr stencilDefinition);
	void flushStencilRenderModel(MikanStencilID stencilId);
	inline IMkTriangulatedMeshPtr getStencilQuadMesh() const { return m_stencilQuadMesh; }
	inline IMkTriangulatedMeshPtr getStencilBoxMesh() const { return m_stencilBoxMesh; }
	inline IMkTriangulatedMeshPtr getDepthQuadMesh() const { return m_depthQuadMesh; }
	inline IMkTriangulatedMeshPtr getDepthBoxMesh() const { return m_depthBoxMesh; }
	inline IMkTriangulatedMeshPtr getLayerVFlippedMesh() const { return m_layerVFlippedMesh; }
	inline IMkTriangulatedMeshPtr getLayerMesh() const { return m_layerMesh; }

protected:
	bool bindEventNodes();

	bool createLayerQuadMeshes();
	bool createQuadMeshes();
	bool createBoxMeshes();
	void updateCompositingFrameBufferSize(NodeEvaluator& evaluator);
	void encodeLinearFrameToSRGB(class NodeEvaluator& evaluator);

	// Stencil System Events
	void onStencilSystemConfigMarkedDirty(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

protected:
	CompositorComponentWeakPtr m_boundCompositorComponent;
	IMkFrameBufferPtr m_compositingFrameBuffer; // 16-bit linear working buffer
	IMkFrameBufferPtr m_outputFrameBuffer;      // 8-bit sRGB final output buffer
	MkMaterialInstancePtr m_linearToSrgbMaterialInstance;
	IMkShaderPtr m_vertexOnlyStencilShader;
	IMkTriangulatedMeshPtr m_stencilQuadMesh;
	IMkTriangulatedMeshPtr m_stencilBoxMesh;
	IMkTriangulatedMeshPtr m_depthQuadMesh;
	IMkTriangulatedMeshPtr m_depthBoxMesh;
	IMkTriangulatedMeshPtr m_layerVFlippedMesh;
	IMkTriangulatedMeshPtr m_layerMesh;
	std::map<MikanStencilID, MikanRenderModelResourcePtr> m_stencilMeshCache;
	std::map<MikanStencilID, MikanRenderModelResourcePtr> m_depthMeshCache;
	NodePtr m_compositeFrameEventNode;

	friend class CompositorNodeGraphFactory;
};

class CompositorNodeGraphFactory : public TypedNodeGraphFactory<CompositorNodeGraph>
{
public:
	CompositorNodeGraphFactory()= default;

	virtual NodeGraphPtr initialCreateNodeGraph(class IEditorWindow* ownerWindow) const override;
};
