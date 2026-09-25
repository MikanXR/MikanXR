#include "CompositorNodeGraph.h"
#include "CompositorComponent.h"
#include "CameraComponent.h"
#include "ClientTextureSourceComponent.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "MkMaterial.h"
#include "IMkShader.h"
#include "MikanRenderModelResource.h"
#include "MkScopedObjectBinding.h"
#include "MikanModelResourceManager.h"
#include "MkMaterialInstance.h"
#include "MkStateStack.h"
#include "IMkTriangulatedMesh.h"
#include "MikanShaderCache.h"
#include "IMkVertexDefinition.h"
#include "IMkState.h"
#include "IEditorWindow.h"
#include "Logger.h"
#include "PathUtils.h"
#include "ModelStencilComponent.h"
#include "QuadStencilSystem.h"
#include "BoxStencilSystem.h"
#include "ModelStencilSystem.h"
#include "VideoSourceComponent.h"

// Assets References
#include "ModelAssetReference.h"
#include "CompositorMaterialAssetReference.h"
#include "TextureAssetReference.h"

// Properties
#include "Properties/GraphBoolProperty.h"
#include "Properties/GraphMaterialProperty.h"
#include "Properties/GraphModelProperty.h"
#include "Properties/GraphShapeProperty.h"
#include "Properties/GraphStencilProperty.h"
#include "Properties/GraphTextureProperty.h"

// Nodes
#include "Nodes/ApplyMaterialNode.h"
#include "Nodes/ArrayNode.h"
#include "Nodes/ColorTextureSourceNode.h"
#include "Nodes/DepthTextureSourceNode.h"
#include "Nodes/DepthMaskNode.h"
#include "Nodes/DrawLayerNode.h"
#include "Nodes/DrawShapesNode.h"
#include "Nodes/EventNode.h"
#include "Nodes/MaterialNode.h"
#include "Nodes/MousePosNode.h"
#include "Nodes/ShapeNode.h"
#include "Nodes/ShapeSelectNode.h"
#include "Nodes/StencilNode.h"
#include "Nodes/StencilSelectNode.h"
#include "Nodes/TextureNode.h"
#include "Nodes/TimeNode.h"
#include "Nodes/VideoTextureNode.h"
#include "Nodes/WriteDepthNode.h"

// Graph
#include "NodeEditorState.h"
#include "Graphs/NodeEvaluator.h"

#include <easy/profiler.h>
#include <GL/glew.h>

// -- CompositorNodeGraph -----
const std::string CompositorNodeGraph::k_compositeFrameEventName= "OnCompositeFrame";

CompositorNodeGraph::CompositorNodeGraph()
	: NodeGraph()
{
	// Assets this graph can reference
	addAssetReferenceFactory<CompositorMaterialAssetReferenceFactory>();
	addAssetReferenceFactory<TextureAssetReferenceFactory>();

	// Add property types this graph can use
	addPropertyFactory<GraphBoolPropertyFactory>();
	addPropertyFactory<GraphMaterialPropertyFactory>();
	addPropertyFactory<GraphTexturePropertyFactory>();
	addPropertyFactory<GraphShapePropertyFactory>();
	addPropertyFactory<GraphStencilPropertyFactory>();

	// Nodes this graph can spawn
	addNodeFactory<ApplyMaterialNodeFactory>();
	addNodeFactory<ArrayNodeFactory>();
	addNodeFactory<ColorTextureSourceNodeFactory>();
	addNodeFactory<DepthTextureSourceNodeFactory>();
	addNodeFactory<DrawLayerNodeFactory>();
	addNodeFactory<DrawShapesNodeFactory>();
	addNodeFactory<DepthMaskNodeFactory>();
	addNodeFactory<EventNodeFactory>();
	addNodeFactory<MousePosNodeFactory>();
	addNodeFactory<MaterialNodeFactory>();
	addNodeFactory<ShapeNodeFactory>();
	addNodeFactory<ShapeSelectNodeFactory>();
	addNodeFactory<StencilNodeFactory>();
	addNodeFactory<StencilSelectNodeFactory>();
	addNodeFactory<TextureNodeFactory>();
	addNodeFactory<TimeNodeFactory>();
	addNodeFactory<VideoTextureNodeFactory>();
	addNodeFactory<WriteDepthNodeFactory>();
}

bool CompositorNodeGraph::createResources()
{
	assert(getOwnerWindow());

	// Create the 16-bit linear working frame buffer that the node graph composites into,
	// but don't init it's resources yet. Compositing in linear color space keeps
	// multiply/blend math correct; the extra bit depth avoids banding in linear space.
	m_compositingFrameBuffer= createMkFrameBuffer("Compositing Node Graph Frame Buffer");
	m_compositingFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_compositingFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16);

	// Create the 8-bit sRGB output frame buffer that the linear result is encoded into.
	// Downstream consumers (Spout, viewport quad, PNG save) require an 8-bit RGBA texture.
	m_outputFrameBuffer= createMkFrameBuffer("Compositing Node Graph Output Frame Buffer");
	m_outputFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_outputFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA);

	// Material used for the final linear->sRGB conversion pass
	auto linearToSrgbMaterial= getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(
		INTERNAL_MATERIAL_PT_LINEAR_TO_SRGB);
	assert(linearToSrgbMaterial);
	m_linearToSrgbMaterialInstance= createMkMaterialInstance(linearToSrgbMaterial);

	// Start listening for stencil changes
	getObjectSystemOfType<QuadStencilSystem>()->getTypedDefinition()->OnPropertyChanged+=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);
	getObjectSystemOfType<BoxStencilSystem>()->getTypedDefinition()->OnPropertyChanged+=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);
	getObjectSystemOfType<ModelStencilSystem>()->getTypedDefinition()->OnPropertyChanged+=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);

	// Create triangulated mesh used to render the layer onto
	bool bSuccess= createLayerQuadMeshes();

	// Create meshes used to draw quad and box stencils and depth masks
	bSuccess&= createQuadMeshes();
	bSuccess&= createBoxMeshes();

	return bSuccess;
}

void CompositorNodeGraph::disposeResources()
{
	// Clean up the frame buffers
	m_compositingFrameBuffer= nullptr;
	m_outputFrameBuffer= nullptr;
	m_linearToSrgbMaterialInstance= nullptr;

	// Stop listening for stencil changes
	getObjectSystemOfType<QuadStencilSystem>()->getTypedDefinition()->OnPropertyChanged-=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);
	getObjectSystemOfType<BoxStencilSystem>()->getTypedDefinition()->OnPropertyChanged-=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);
	getObjectSystemOfType<ModelStencilSystem>()->getTypedDefinition()->OnPropertyChanged-=
		MakeDelegate(this, &CompositorNodeGraph::onStencilSystemConfigMarkedDirty);

	// Free rendering resources
	m_stencilBoxMesh= nullptr;
	m_stencilQuadMesh= nullptr;
	m_depthBoxMesh= nullptr;
	m_depthQuadMesh= nullptr;
	m_layerVFlippedMesh= nullptr;
	m_layerMesh= nullptr;
}

bool CompositorNodeGraph::loadFromConfig(const NodeGraphConfig& config)
{
	bool bSuccess= true;

	if (NodeGraph::loadFromConfig(config))
	{
		bSuccess= bindEventNodes();
	}
	else
	{
		MIKAN_LOG_ERROR("CompositorNodeGraph::loadFromConfig") << "Failed to parse node graph config";
		bSuccess= false;
	}

	return bSuccess;
}

bool CompositorNodeGraph::bindEventNodes()
{
	bool bSuccess= true;

	m_compositeFrameEventNode= getEventNodeByName(k_compositeFrameEventName);
	if (!m_compositeFrameEventNode)
	{
		MIKAN_LOG_ERROR("CompositorNodeGraph::bindEventNodes")
			<< "Failed to find event node: " << k_compositeFrameEventName;
		bSuccess= false;
	}

	return bSuccess;
}

bool CompositorNodeGraph::compositeFrame(NodeEvaluator& evaluator)
{
	EASY_FUNCTION();

	if (m_compositeFrameEventNode)
	{
		// Make sure the frame buffer is the correct size
		updateCompositingFrameBufferSize(evaluator);

		// Create a scoped binding for the video export framebuffer
		IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
		MkScopedObjectBinding compositorFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
														   "Compositor Framebuffer Scope", m_compositingFrameBuffer);
		if (compositorFramebufferBinding)
		{
			// Binding a COLOR framebuffer clears color only. Reset depth to far so a depth-tested draw
			// with no depth source draws everything and nothing leaks across frames. The chain runs
			// with depth off; a node that wants depth (WriteDepthNode, DrawShapesNode) enables it
			// inside its own scoped state.
			glClear(GL_DEPTH_BUFFER_BIT);
			compositorFramebufferBinding.getMkState()->disableFlag(eMkStateFlagType::depthTest);

			// Evaluate the composite frame nodes (composites into the 16-bit linear working buffer)
			evaluator.evaluateFlowPinChain(m_compositeFrameEventNode);
		}
		else
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Broken frame buffer"));
		}

		// Final step: convert the linear composited result back to sRGB into the 8-bit output buffer
		encodeLinearFrameToSRGB(evaluator);
	}
	else
	{
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::invalidNode, "Missing compositeFrame event node"));
	}

	return !evaluator.hasErrors();
}

void CompositorNodeGraph::encodeLinearFrameToSRGB(NodeEvaluator& evaluator)
{
	if (!m_outputFrameBuffer || !m_linearToSrgbMaterialInstance || !m_compositingFrameBuffer)
		return;

	IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
	MkScopedObjectBinding outputFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
												   "Compositor Output Framebuffer Scope", m_outputFrameBuffer);
	if (outputFramebufferBinding)
	{
		outputFramebufferBinding.getMkState()->disableFlag(eMkStateFlagType::depthTest);

		MkMaterialConstPtr material= m_linearToSrgbMaterialInstance->getMaterial();
		MkScopedMaterialBinding materialBinding= material->bindMaterial();
		if (materialBinding)
		{
			m_linearToSrgbMaterialInstance->setTextureBySemantic(eUniformSemantic::rgbTexture,
																 m_compositingFrameBuffer->getColorTexture());

			MkScopedMaterialInstanceBinding materialInstanceBinding=
				m_linearToSrgbMaterialInstance->bindMaterialInstance(materialBinding);
			if (materialInstanceBinding)
			{
				m_layerMesh->drawElements();
			}
		}
	}
	else
	{
		evaluator.addError(
			NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Broken output frame buffer"));
	}
}

IMkTextureConstPtr CompositorNodeGraph::getCompositedFrameTexture() const
{
	// Return the 8-bit sRGB output buffer (the linear working buffer is internal only)
	return m_outputFrameBuffer ? m_outputFrameBuffer->getColorTexture() : IMkTextureConstPtr();
}

bool CompositorNodeGraph::bindToCompositorComponent(CompositorComponentPtr compositorComponent)
{
	if (compositorComponent)
	{
		m_boundCompositorComponent= compositorComponent;
		return true;
	}

	return false;
}

void CompositorNodeGraph::unbindFromCompositorComponent(CompositorComponentPtr compositorComponent)
{
	if (getBoundCompositorComponent() == compositorComponent)
	{
		m_boundCompositorComponent.reset();
	}
}

CompositorComponentPtr CompositorNodeGraph::getBoundCompositorComponent() const
{
	return m_boundCompositorComponent.lock();
}

CameraComponentPtr CompositorNodeGraph::getBoundCameraComponent() const
{
	CompositorComponentPtr compositorComponent= getBoundCompositorComponent();
	if (compositorComponent)
	{
		return compositorComponent->getCameraComponent();
	}

	return CameraComponentPtr();
}

VideoSourceComponentPtr CompositorNodeGraph::getBoundVideoSourceComponent() const
{
	CameraComponentPtr cameraComponent= getBoundCameraComponent();
	if (cameraComponent)
	{
		return cameraComponent->getVideoSourceComponent();
	}

	return VideoSourceComponentPtr();
}

void CompositorNodeGraph::gatherAllReferencedClientSourceIDs(std::set<std::string>& outClientSourceIds) const
{
	visitAllNodes(
		[&outClientSourceIds](NodeConstPtr node)
		{
			// Check if this node is a texture source node
			TextureSourceComponentPtr textureSourceComponent;
			if (auto clientSourceNode= std::dynamic_pointer_cast<const ColorTextureSourceNode>(node))
			{
				textureSourceComponent= clientSourceNode->getTextureSourceComponent();
			}
			else if (auto clientDepthNode= std::dynamic_pointer_cast<const DepthTextureSourceNode>(node))
			{
				textureSourceComponent= clientDepthNode->getTextureSourceComponent();
			}

			// If it is, see if it's a client texture source component and gather the client source ID
			if (textureSourceComponent)
			{
				auto clientTextureSourceComponent=
					std::dynamic_pointer_cast<ClientTextureSourceComponent>(textureSourceComponent);

				if (clientTextureSourceComponent)
				{
					outClientSourceIds.insert(clientTextureSourceComponent->getClientSourceName());
				}
			}
		});
}

MikanRenderModelResourcePtr CompositorNodeGraph::getOrLoadStencilRenderModel(
	ModelStencilDefinitionPtr stencilDefinition)
{
	MikanStencilID stencilId= stencilDefinition->getComponentId();
	auto it= m_stencilMeshCache.find(stencilId);

	if (it != m_stencilMeshCache.end())
	{
		return it->second;
	}
	else
	{
		IEditorWindow* ownerWindow= getOwnerWindow();

		// Load the stencil model and render it using the flat textured material
		auto stencilMaterial=
			ownerWindow->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED);
		// Model paths are project relative, the same resolve the stencil's own viewport mesh takes
		auto renderModelPtr= ownerWindow->getModelResourceManager()->fetchRenderModel(
			PathUtils::resolveProjectResource(stencilDefinition->getModelPath()), stencilMaterial);

		if (renderModelPtr)
		{
			m_stencilMeshCache.insert({stencilId, renderModelPtr});
		}

		return renderModelPtr;
	}
}

MikanRenderModelResourcePtr CompositorNodeGraph::getOrLoadDepthRenderModel(ModelStencilDefinitionPtr stencilDefinition)
{
	MikanStencilID stencilId= stencilDefinition->getComponentId();
	auto it= m_depthMeshCache.find(stencilId);

	if (it != m_depthMeshCache.end())
	{
		return it->second;
	}
	else
	{
		IEditorWindow* ownerWindow= getOwnerWindow();

		// Load the depth model and render it using the simple depth material
		auto depthMaterial=
			ownerWindow->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_LINEAR_DEPTH);
		auto renderModelPtr= ownerWindow->getModelResourceManager()->fetchRenderModel(
			PathUtils::resolveProjectResource(stencilDefinition->getModelPath()), depthMaterial);

		if (renderModelPtr)
		{
			m_depthMeshCache.insert({stencilId, renderModelPtr});
		}

		return renderModelPtr;
	}
}

void CompositorNodeGraph::flushStencilRenderModel(MikanStencilID stencilId)
{
	{
		auto it= m_stencilMeshCache.find(stencilId);
		if (it != m_stencilMeshCache.end())
		{
			// Remove the entry from the stencil id -> stencil render resource table
			m_stencilMeshCache.erase(it);
		}
	}

	{
		auto it= m_depthMeshCache.find(stencilId);
		if (it != m_depthMeshCache.end())
		{
			// Remove the entry from the stencil id -> depth render resource table
			m_depthMeshCache.erase(it);
		}
	}
}

void CompositorNodeGraph::onStencilSystemConfigMarkedDirty(CommonConfigPtr configPtr,
														   const ConfigPropertyChangeSet& changedPropertySet)
{
	ModelStencilDefinitionPtr modelStencilConfig= std::dynamic_pointer_cast<ModelStencilDefinition>(configPtr);

	if (modelStencilConfig != nullptr)
	{
		if (changedPropertySet.hasPropertyName(ModelStencilDefinition::k_modelStencilObjPathPropertyId))
		{
			// Flush the models we have loaded for the given stencil.
			// We'll reload it next time the compositor renders the stencil.
			flushStencilRenderModel(modelStencilConfig->getComponentId());
		}
	}
}

bool CompositorNodeGraph::createLayerQuadMeshes()
{
	static uint16_t x_indices[]= {0, 1, 2, 0, 2, 3};

	auto material= getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(
		INTERNAL_MATERIAL_PT_FULLSCREEN_RGB_TEXTURE);
	assert(material);

	struct QuadVertex
	{
		glm::vec2 aPos;
		glm::vec2 aTexCoords;
	};
	size_t vertexSize= sizeof(QuadVertex);

	// Create triangulated quad mesh to draw the layer on
	{
		static QuadVertex x_vertices[]= {
			//   positions                texCoords
			{glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 1.0f)},
			{glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 0.0f)},
			{glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 0.0f)},
			{glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 1.0f)},
		};

		m_layerMesh= createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "layer_quad_mesh",
											  (const uint8_t*)x_vertices, vertexSize,
											  4, // 4 verts
											  (const uint8_t*)x_indices,
											  sizeof(uint16_t), // 2 bytes per index
											  2,                // 2 tris
											  false);           // mesh doesn't own quad vert data

		if (!m_layerMesh->setMaterial(material) || !m_layerMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createLayerQuadMeshes()") << "Failed to create layer mesh";
			return false;
		}
	}

	// Create triangulated quad mesh to draw the layer on with flipped v texel coordinates
	// (this is usually used for video sources where we need to flip the image vertically)
	{
		static QuadVertex x_vertices[]= {
			//   positions                texCoords (flipped v coordinated)
			{glm::vec2(-1.0f, 1.0f), glm::vec2(0.0f, 0.0f)},
			{glm::vec2(-1.0f, -1.0f), glm::vec2(0.0f, 1.0f)},
			{glm::vec2(1.0f, -1.0f), glm::vec2(1.0f, 1.0f)},
			{glm::vec2(1.0f, 1.0f), glm::vec2(1.0f, 0.0f)},
		};

		m_layerVFlippedMesh=
			createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "layer_vflipped_quad_mesh",
									 (const uint8_t*)x_vertices, vertexSize,
									 4, // 4 verts
									 (const uint8_t*)x_indices,
									 sizeof(uint16_t), // 2 bytes per index
									 2,                // 2 tris
									 false);           // mesh doesn't own quad vert data

		if (!m_layerVFlippedMesh->setMaterial(material) || !m_layerVFlippedMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createLayerQuadMeshes()") << "Failed to create vflipped layer mesh";
			return false;
		}
	}

	return true;
}

bool CompositorNodeGraph::createQuadMeshes()
{
	auto stencilMaterial=
		getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_SOLID_COLOR);
	assert(stencilMaterial);
	auto depthMaterial=
		getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_LINEAR_DEPTH);
	assert(depthMaterial);

	struct QuadlVertex
	{
		glm::vec3 aPos;
	};
	size_t vertexSize= sizeof(QuadlVertex);

	// Create triangulated quad mesh to draw the quad stencils
	{
		static QuadlVertex x_vertices[]= {
			//   positions
			{glm::vec3(-0.5f, 0.5f, 0.0f)},
			{glm::vec3(-0.5f, -0.5f, 0.0f)},
			{glm::vec3(0.5f, -0.5f, 0.0f)},
			{glm::vec3(0.5f, 0.5f, 0.0f)},
		};
		static uint16_t x_indices[]= {0, 1, 2, 0, 2, 3};

		m_stencilQuadMesh= createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "quad_stencil_mesh",
													(const uint8_t*)x_vertices, vertexSize,
													4, // 4 verts in a quad
													(const uint8_t*)x_indices,
													sizeof(uint16_t), // 2 bytes per index
													2,                // 2 tris in a quad
													false);           // mesh doesn't own quad vertex data

		if (!m_stencilQuadMesh->setMaterial(stencilMaterial) || !m_stencilQuadMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createQuadMeshes()") << "Failed to create quad stencil mesh";
			return false;
		}

		m_depthQuadMesh= createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "quad_depth_mesh",
												  (const uint8_t*)x_vertices, vertexSize,
												  4, // 4 verts in a quad
												  (const uint8_t*)x_indices,
												  sizeof(uint16_t), // 2 bytes per index
												  2,                // 2 tris in a quad
												  false);           // mesh doesn't own quad vertex data

		if (!m_depthQuadMesh->setMaterial(depthMaterial) || !m_depthQuadMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createQuadMeshes()") << "Failed to create quad depth mesh";
			return false;
		}
	}

	return true;
}

bool CompositorNodeGraph::createBoxMeshes()
{
	auto stencilMaterial=
		getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_SOLID_COLOR);
	assert(stencilMaterial);
	auto depthMaterial=
		getOwnerWindow()->getGraphicsContext()->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_LINEAR_DEPTH);
	assert(depthMaterial);

	struct BoxVertex
	{
		glm::vec3 aPos;
	};
	size_t vertexSize= sizeof(BoxVertex);

	// Create triangulated box mesh to draw the box stencils
	{
		static BoxVertex x_vertices[]= {
			//   positions
			{glm::vec3(-0.5f, 0.5f, -0.5f)}, {glm::vec3(-0.5f, 0.5f, 0.5f)},   {glm::vec3(0.5f, 0.5f, 0.5f)},
			{glm::vec3(0.5f, 0.5f, -0.5f)},  {glm::vec3(-0.5f, -0.5f, -0.5f)}, {glm::vec3(-0.5f, -0.5f, 0.5f)},
			{glm::vec3(0.5f, -0.5f, 0.5f)},  {glm::vec3(0.5f, -0.5f, -0.5f)},
		};
		static uint16_t x_indices[]= {
			0, 4, 1, 1, 4, 5, // -X Face
			1, 5, 2, 2, 5, 6, // +Z Face
			2, 6, 3, 3, 6, 7, // +X Face
			0, 3, 7, 7, 4, 0, // -Z Face
			5, 4, 6, 6, 4, 7, // -Y Face
			3, 0, 1, 1, 2, 3  // +Y Face
		};

		m_stencilBoxMesh= createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "box_stencil_mesh",
												   (const uint8_t*)x_vertices, vertexSize,
												   8, // 8 verts in a cube
												   (const uint8_t*)x_indices,
												   sizeof(uint16_t), // 2 bytes per index
												   12,               // 12 tris in a cube (6 faces * 2 tris/face)
												   false);           // mesh doesn't own box vertex data
		if (!m_stencilBoxMesh->setMaterial(stencilMaterial) || !m_stencilBoxMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createBoxMeshes()") << "Failed to create box stencil mesh";
			return false;
		}

		m_depthBoxMesh= createMkTriangulatedMesh(getOwnerWindow()->getGraphicsContext().get(), "box_depth_mesh",
												 (const uint8_t*)x_vertices, vertexSize,
												 8, // 8 verts in a cube
												 (const uint8_t*)x_indices,
												 sizeof(uint16_t), // 2 bytes per index
												 12,               // 12 tris in a cube (6 faces * 2 tris/face)
												 false);           // mesh doesn't own box vertex data
		if (!m_depthBoxMesh->setMaterial(depthMaterial) || !m_depthBoxMesh->createResources())
		{
			MIKAN_LOG_ERROR("CompositorNodeGraph::createBoxMeshes()") << "Failed to create box stencil mesh";
			return false;
		}
	}

	return true;
}

void CompositorNodeGraph::updateCompositingFrameBufferSize(NodeEvaluator& evaluator)
{
	// Use the current video source's frame size
	VideoSourceComponentPtr videoSource= getBoundVideoSourceComponent();
	if (int frameWidth, frameHeight; videoSource && videoSource->getVideoPixelDimensions(frameWidth, frameHeight))
	{
		// Does nothing if the frame buffer is already the correct size
		m_compositingFrameBuffer->setSize(frameWidth, frameHeight);
		m_outputFrameBuffer->setSize(frameWidth, frameHeight);
	}

	// (Re)Initialize the frame buffers if they're in an invalid state
	if (!m_compositingFrameBuffer->isValid())
	{
		m_compositingFrameBuffer->createResources();
	}
	if (!m_outputFrameBuffer->isValid())
	{
		m_outputFrameBuffer->createResources();
	}
}

// -- CompositorNodeGraphFactory ----
NodeGraphPtr CompositorNodeGraphFactory::initialCreateNodeGraph(IEditorWindow* ownerWindow) const
{
	auto nodeGraph= NodeGraphFactory::initialCreateNodeGraph(ownerWindow);
	if (!nodeGraph)
	{
		// Something happened with GL resource allocation?
		return NodeGraphPtr();
	}

	auto compositorNodeGraph= std::static_pointer_cast<CompositorNodeGraph>(nodeGraph);

	// Add default nodes
	NodeEditorState editorState;
	editorState.nodeGraph= compositorNodeGraph;

	auto compositeFrameNode= compositorNodeGraph->createTypedNode<EventNode>(editorState);
	compositeFrameNode->setName(CompositorNodeGraph::k_compositeFrameEventName);

	// Store off pointers to the event nodes we manually created
	compositorNodeGraph->bindEventNodes();

	return compositorNodeGraph;
}