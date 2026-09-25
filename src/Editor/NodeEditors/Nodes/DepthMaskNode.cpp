#include "CameraComponent.h"
#include "IconsForkAwesome.h"
#include "DepthMaskNode.h"
#include "IMkFrameBuffer.h"
#include "MkMaterial.h"
#include "MikanRenderModelResource.h"
#include "MikanModelResourceManager.h"
#include "IMkShader.h"
#include "MkScopedObjectBinding.h"
#include "MikanShaderCache.h"
#include "MkStateStack.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "MkMaterialInstance.h"
#include "IMkGraphicsContext.h"
#include "LocText.h"
#include "Logger.h"
#include "MainWindow.h"
#include "NodeEditorState.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "VideoSourceComponent.h"

#include "QuadStencilComponent.h"
#include "BoxStencilComponent.h"
#include "ModelStencilComponent.h"
#include "QuadStencilSystem.h"
#include "BoxStencilSystem.h"
#include "ModelStencilSystem.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "Pins/ArrayPin.h"
#include "Pins/TexturePin.h"

#include "Properties/GraphMaterialProperty.h"
#include "Properties/GraphStencilProperty.h"
#include "Properties/GraphTextureProperty.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

#include <glm/gtc/type_ptr.hpp>

#include <typeinfo>
#include <GL/glew.h>

#include <easy/profiler.h>

// -- DepthMaskNodeConfig -----
configuru::Config DepthMaskNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	pt["disable_quad_stencils"]= bDisableQuadStencils;
	pt["disable_box_stencils"]= bDisableBoxStencils;
	pt["disable_model_stencils"]= bDisableModelStencils;

	return pt;
}

void DepthMaskNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	bDisableQuadStencils= pt.get_or<bool>("disable_quad_stencils", false);
	bDisableBoxStencils= pt.get_or<bool>("disable_box_stencils", false);
	bDisableModelStencils= pt.get_or<bool>("disable_model_stencils", false);
}

// -- DepthMaskNode -----
DepthMaskNode::DepthMaskNode()
	: Node()
{
	// Create the frame buffer, but don't init its internals yet.
	// Wait until evaluation to get the right output texture size.
	m_linearDepthFrameBuffer= createMkFrameBuffer("DepthMask Frame Buffer");
	m_linearDepthFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR_AND_DEPTH);
	m_linearDepthFrameBuffer->setClearColor(glm::vec4(1.f, 1.f, 1.f, 1.f));
}

DepthMaskNode::~DepthMaskNode()
{
	// Clean up the frame buffer
	m_linearDepthFrameBuffer= nullptr;

	// Free pin references
	m_stencilsPin= nullptr;

	// Stop listening to events from owner graph
	setOwnerGraph(NodeGraphPtr());
}

bool DepthMaskNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto depthMaskNodeConfig= std::static_pointer_cast<const DepthMaskNodeConfig>(nodeConfig);

		m_bDisableQuadStencil= depthMaskNodeConfig->bDisableQuadStencils;
		m_bDisableBoxStencil= depthMaskNodeConfig->bDisableBoxStencils;
		m_bDisableModelStencil= depthMaskNodeConfig->bDisableModelStencils;

		return true;
	}

	return false;
}

void DepthMaskNode::onGraphLoaded(bool success)
{
	// Don't bother with setup if the graph load failed
	if (!success)
		return;

	// Optionally bind a stencil input pin
	ArrayPinPtr stencilInPin= getFirstPinOfType<ArrayPin>(eNodePinDirection::INPUT);
	if (stencilInPin && stencilInPin->getElementClassName() == GraphStencilProperty::k_propertyClassName)
	{
		setStencilsPin(stencilInPin);
	}

	// Make sure we have a texture output pin
	setDepthTextureOutPin(getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT));
}

void DepthMaskNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto depthMaskNodeConfig= std::static_pointer_cast<DepthMaskNodeConfig>(nodeConfig);

	depthMaskNodeConfig->bDisableQuadStencils= m_bDisableQuadStencil;
	depthMaskNodeConfig->bDisableBoxStencils= m_bDisableBoxStencil;
	depthMaskNodeConfig->bDisableModelStencils= m_bDisableModelStencil;

	Node::saveToConfig(nodeConfig);
}

void DepthMaskNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnGraphLoaded-= MakeDelegate(this, &DepthMaskNode::onGraphLoaded);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnGraphLoaded+= MakeDelegate(this, &DepthMaskNode::onGraphLoaded);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

void DepthMaskNode::setStencilsPin(ArrayPinPtr inPin) { m_stencilsPin= inPin; }

void DepthMaskNode::setDepthTextureOutPin(TexturePinPtr outPin) { m_outDepthTexturePin= outPin; }

bool DepthMaskNode::evaluateNode(NodeEvaluator& evaluator)
{
	bool bSuccess= true;

	if (bSuccess && !evaluateInputs(evaluator))
	{
		bSuccess= false;
	}

	int frameWidth= 0;
	int frameHeight= 0;
	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	CameraComponentPtr cameraComponent= compositorGraph->getBoundCameraComponent();
	if (bSuccess)
	{
		bSuccess= cameraComponent && cameraComponent->getAperturePixelDimensions(frameWidth, frameHeight);
	}

	// The flags have to be read after the rebuild, or the frame a stencil first arrives
	// is decided against the previous frame's lists
	bool bAnyQuadStencils= false;
	bool bAnyBoxStencils= false;
	bool bAnyModelStencils= false;
	if (bSuccess)
	{
		rebuildDepthMaskLists();

		bAnyQuadStencils= !m_quadStencilIds.empty();
		bAnyBoxStencils= !m_boxStencilIds.empty();
		bAnyModelStencils= !m_modelStencilIds.empty();

		if (!bAnyQuadStencils && !bAnyBoxStencils && !bAnyModelStencils)
		{
			// Nothing to occlude anything, which is a legitimate authoring state with a
			// well defined answer: a mask entirely at the far plane, which is what the
			// frame buffer's clear already produces. Warn rather than error, since an
			// error here would stop the whole composite chain at this node.
			evaluator.addWarning(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "No stencils", this,
													 nullptr, eNodeEvaluationSeverity::warning));
		}
	}

	if (bSuccess)
	{
		// Use the current video source's frame size as the depth texture size
		// Does nothing if the frame buffer is already the correct size
		// Invalidates the frame buffer if it's not the correct size
		m_linearDepthFrameBuffer->setSize(frameWidth, frameHeight);

		// (Re)Initialize the frame buffer if it's in an invalid state
		if (!m_linearDepthFrameBuffer->isValid())
		{
			if (m_linearDepthFrameBuffer->createResources())
			{
				// Bind the frame buffer's texture to the output texture pin
				m_outDepthTexturePin->setValue(m_linearDepthFrameBuffer->getDepthTexture());
			}
			else
			{
				m_outDepthTexturePin->setValue(IMkTexturePtr());
				evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
													   "Unable to create depth frame buffer", this));
				bSuccess= false;
			}
		}
	}

	// Bind unconditionally, because binding is what clears the mask to the far plane.
	// With no stencils that clear is the whole result.
	if (bSuccess)
	{
		// Bind the depth frame buffer
		IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
		MkScopedObjectBinding depthFramebufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
													  "Depth Mask Framebuffer Scope", m_linearDepthFrameBuffer);
		if (depthFramebufferBinding)
		{
			IMkState* glState= depthFramebufferBinding.getMkState();

			// Apply any Stencils assigned to the node
			if (bAnyQuadStencils)
				evaluateQuadDepthMasks(cameraComponent, glState);

			if (bAnyBoxStencils)
				evaluateBoxDepthMasks(cameraComponent, glState);

			if (bAnyModelStencils)
				evaluateModelDepthMasks(cameraComponent, glState);
		}
		else
		{
			evaluator.addError(
				NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Broken frame buffer", this));
		}
	}

	return bSuccess;
}

void DepthMaskNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));

	// Inputs
	editorRenderInputPins(editorState);

	// Texture Preview
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr colorTexture=
		m_linearDepthFrameBuffer ? m_linearDepthFrameBuffer->getColorTexture() : IMkTexturePtr();
	MkGui::drawImage(colorTexture, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void DepthMaskNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	// title bar
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.depthMaskHeader")))
	{
		MkGuiStyleConstPtr propertyStyle= editorState.styleManager->getStyle("node_editor_property_value");

		MkGui::drawCheckBoxProperty(propertyStyle, "DepthMaskNodeDisableQuadStencil", locText("nodes.disableQuads"),
									m_bDisableQuadStencil);
		MkGui::drawCheckBoxProperty(propertyStyle, "DepthMaskNodeDisableBoxStencil", locText("nodes.disableBoxes"),
									m_bDisableBoxStencil);
		MkGui::drawCheckBoxProperty(propertyStyle, "DepthMaskNodeDisableModelStencil", locText("nodes.disableModels"),
									m_bDisableModelStencil);
	}
}

void DepthMaskNode::onLinkConnected(NodeLinkPtr link, NodePinPtr pin)
{
	if (pin == m_stencilsPin)
	{
		m_stencilsPin->copyValueFromSourcePin();

		rebuildDepthMaskLists();
	}
}

void DepthMaskNode::onLinkDisconnected(NodeLinkPtr link, NodePinPtr pin)
{
	if (pin == m_stencilsPin)
	{
		m_stencilsPin->clearArray();

		rebuildDepthMaskLists();
	}
}

void DepthMaskNode::rebuildDepthMaskLists()
{
	m_quadStencilIds.clear();
	m_boxStencilIds.clear();
	m_modelStencilIds.clear();

	if (m_stencilsPin)
	{
		for (GraphPropertyPtr property : m_stencilsPin->getArray())
		{
			if (!property)
				continue;

			if (property->getClassName() != GraphStencilProperty::k_propertyClassName)
				continue;

			auto stencilProperty= std::static_pointer_cast<GraphStencilProperty>(property);
			StencilComponentPtr stencilComponent= stencilProperty->getStencilComponent();
			if (!stencilComponent)
				continue;

			if (auto quadStencil= std::dynamic_pointer_cast<QuadStencilComponent>(stencilComponent))
			{
				m_quadStencilIds.push_back(quadStencil->getStencilComponentDefinition()->getComponentId());
			}
			else if (auto boxStencil= std::dynamic_pointer_cast<BoxStencilComponent>(stencilComponent))
			{
				m_boxStencilIds.push_back(boxStencil->getStencilComponentDefinition()->getComponentId());
			}
			else if (auto modelStencil= std::dynamic_pointer_cast<ModelStencilComponent>(stencilComponent))
			{
				m_modelStencilIds.push_back(modelStencil->getStencilComponentDefinition()->getComponentId());
			}
		}
	}
}

void DepthMaskNode::evaluateQuadDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState)
{
	EASY_FUNCTION();

	if (m_bDisableQuadStencil)
		return;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	IMkTriangulatedMeshPtr depthQuadMesh= compositorGraph->getDepthQuadMesh();

	// Get the camera pose matrix for the current tracked video source.
	// World space: the stencils this pose culls against carry world transforms.
	glm::mat4 cameraXform;
	if (!cameraComponent->getWorldSpaceAperturePose(cameraXform))
		return;

	// Collect stencil in view of the tracked camera
	const glm::vec3 cameraForward(cameraXform[2] * -1.f); // Camera forward is along negative z-axis
	const glm::vec3 cameraPosition(cameraXform[3]);

	std::vector<QuadStencilComponentPtr> quadStencilList;
	getObjectSystemOfType<QuadStencilSystem>()->getRelevantQuadStencilList(&m_quadStencilIds, cameraPosition,
																		   cameraForward, quadStencilList);

	if (quadStencilList.size() == 0)
		return;

	MkMaterialInstancePtr materialInstance= depthQuadMesh->getMaterialInstance();
	MkMaterialConstPtr material= materialInstance->getMaterial();

	if (auto materialBinding= material->bindMaterial())
	{
		// Also get the the view matrix for the tracked video source
		glm::mat4 viewMatrix;
		if (!cameraComponent->getApertureViewMatrix(viewMatrix))
			return;

		// Also get the the projection matrix for the tracked video source
		glm::mat4 projectionMatrix;
		if (!cameraComponent->getApertureProjectionMatrix(projectionMatrix, true))
			return;

		// Draw stencil quads first
		for (QuadStencilComponentPtr stencil : quadStencilList)
		{
			// Set the model matrix of stencil quad
			auto stencilConfig= stencil->getQuadStencilDefinition();
			const glm::mat4 xform= stencil->getWorldTransform();
			const glm::vec3 x_axis= glm::vec3(xform[0]) * stencilConfig->getQuadWidth();
			const glm::vec3 y_axis= glm::vec3(xform[1]) * stencilConfig->getQuadHeight();
			const glm::vec3 z_axis= glm::vec3(xform[2]);
			const glm::vec3 position= glm::vec3(xform[3]);
			const glm::mat4 modelMatrix= glm::mat4(glm::vec4(x_axis, 0.f), glm::vec4(y_axis, 0.f),
												   glm::vec4(z_axis, 0.f), glm::vec4(position, 1.f));

			// Set the model-view-projection matrix on the stencil shader
			materialInstance->setMat4BySemantic(eUniformSemantic::modelMatrix, modelMatrix);
			materialInstance->setMat4BySemantic(eUniformSemantic::viewMatrix, viewMatrix);
			materialInstance->setMat4BySemantic(eUniformSemantic::projectionMatrix, projectionMatrix);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				// Draw the quad
				depthQuadMesh->drawElements();
			}
		}
	}
}

void DepthMaskNode::evaluateBoxDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState)
{
	EASY_FUNCTION();

	if (m_bDisableBoxStencil)
		return;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());
	IMkTriangulatedMeshPtr stencilBoxMesh= compositorGraph->getDepthBoxMesh();

	// Get the camera pose matrix for the current tracked video source.
	// World space: the stencils this pose culls against carry world transforms.
	glm::mat4 cameraXform;
	if (!cameraComponent->getWorldSpaceAperturePose(cameraXform))
		return;

	// Collect stencil in view of the tracked camera
	const glm::vec3 cameraForward(cameraXform[2] * -1.f); // Camera forward is along negative z-axis
	const glm::vec3 cameraPosition(cameraXform[3]);

	std::vector<BoxStencilComponentPtr> boxStencilList;
	getObjectSystemOfType<BoxStencilSystem>()->getRelevantBoxStencilList(&m_boxStencilIds, cameraPosition,
																		 cameraForward, boxStencilList);

	if (boxStencilList.size() == 0)
		return;

	MkMaterialInstancePtr materialInstance= stencilBoxMesh->getMaterialInstance();
	MkMaterialConstPtr material= materialInstance->getMaterial();

	// Then draw stencil boxes ...
	if (auto materialBinding= material->bindMaterial())
	{
		// Also get the the view matrix for the tracked video source
		glm::mat4 viewMatrix;
		if (!cameraComponent->getApertureViewMatrix(viewMatrix))
			return;

		// Also get the the projection matrix for the tracked video source
		glm::mat4 projectionMatrix;
		if (!cameraComponent->getApertureProjectionMatrix(projectionMatrix, true))
			return;

		for (BoxStencilComponentPtr stencil : boxStencilList)
		{
			// Set the model matrix of stencil quad
			auto stencilConfig= stencil->getBoxStencilDefinition();
			const glm::mat4 xform= stencil->getWorldTransform();
			const glm::vec3 x_axis= glm::vec3(xform[0]) * stencilConfig->getBoxXSize();
			const glm::vec3 y_axis= glm::vec3(xform[1]) * stencilConfig->getBoxYSize();
			const glm::vec3 z_axis= glm::vec3(xform[2]) * stencilConfig->getBoxZSize();
			const glm::vec3 position= glm::vec3(xform[3]);
			const glm::mat4 modelMatrix= glm::mat4(glm::vec4(x_axis, 0.f), glm::vec4(y_axis, 0.f),
												   glm::vec4(z_axis, 0.f), glm::vec4(position, 1.f));

			// Set the model, view, and projection matrices on the depth mask shader
			materialInstance->setMat4BySemantic(eUniformSemantic::modelMatrix, modelMatrix);
			materialInstance->setMat4BySemantic(eUniformSemantic::viewMatrix, viewMatrix);
			materialInstance->setMat4BySemantic(eUniformSemantic::projectionMatrix, projectionMatrix);

			if (auto materialInstanceBinding= materialInstance->bindMaterialInstance(materialBinding))
			{
				// Draw the box
				stencilBoxMesh->drawElements();
			}
		}
	}
}

void DepthMaskNode::evaluateModelDepthMasks(CameraComponentPtr cameraComponent, IMkState* glState)
{
	EASY_FUNCTION();

	if (m_bDisableModelStencil)
		return;

	auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());

	// Get the camera pose matrix for the current tracked video source.
	// World space: the stencils this pose culls against carry world transforms.
	glm::mat4 cameraXform;
	if (!cameraComponent->getWorldSpaceAperturePose(cameraXform))
		return;

	// Collect stencil in view of the tracked camera
	const glm::vec3 cameraForward(cameraXform[2] * -1.f); // Camera forward is along negative z-axis
	const glm::vec3 cameraPosition(cameraXform[3]);

	std::vector<ModelStencilComponentPtr> modelStencilList;
	getObjectSystemOfType<ModelStencilSystem>()->getRelevantModelStencilList(&m_modelStencilIds, cameraPosition,
																			 cameraForward, modelStencilList);

	if (modelStencilList.size() == 0)
		return;

	// Also get the the view matrix for the tracked video source
	glm::mat4 viewMatrix;
	if (!cameraComponent->getApertureViewMatrix(viewMatrix))
		return;

	// Also get the the projection matrix for the tracked video source
	glm::mat4 projectionMatrix;
	if (!cameraComponent->getApertureProjectionMatrix(projectionMatrix, true))
		return;

	// Then draw stencil models
	for (ModelStencilComponentPtr stencil : modelStencilList)
	{
		auto stencilConfig= stencil->getModelStencilDefinition();
		const MikanStencilID stencilId= stencilConfig->getComponentId();
		MikanRenderModelResourcePtr renderModelResource= compositorGraph->getOrLoadDepthRenderModel(stencilConfig);

		if (renderModelResource)
		{
			// Set the model matrix of stencil model
			const glm::mat4 modelMatrix= stencil->getWorldTransform();

			for (int meshIndex= 0; meshIndex < renderModelResource->getTriangulatedMeshCount(); ++meshIndex)
			{
				IMkTriangulatedMeshPtr mesh= renderModelResource->getTriangulatedMesh(meshIndex);
				MkMaterialInstancePtr materialInst= mesh->getMaterialInstance();
				MkMaterialConstPtr material= materialInst->getMaterial();

				// Set the model-view-projection matrix on the stencil material instance
				materialInst->setMat4BySemantic(eUniformSemantic::modelMatrix, modelMatrix);
				materialInst->setMat4BySemantic(eUniformSemantic::viewMatrix, viewMatrix);
				materialInst->setMat4BySemantic(eUniformSemantic::projectionMatrix, projectionMatrix);

				// Bind the material and draw the mesh
				// TODO: We could optimize this by sorting on the shader program
				// to minimize the number of shader program switches
				// Or switch to using a GlScene for rendering stencils,
				// which handles the shader program sorting for us.
				auto materialBinding= material->bindMaterial();
				if (materialBinding)
				{
					auto materialInstanceBinding= materialInst->bindMaterialInstance(materialBinding);
					if (materialInstanceBinding)
					{
						mesh->drawElements();
					}
				}
			}
		}
	}
}

// -- ProgramNode Factory -----
NodePtr DepthMaskNodeFactory::createNode(const NodeEditorState& editorState) const
{
	// Create the node and default pins
	// The rest of the input pins can't be connected until we have a material assigned
	auto node= std::static_pointer_cast<DepthMaskNode>(NodeFactory::createNode(editorState));

	// Create stencil input pins
	ArrayPinPtr stencilListInPin= node->addPin<ArrayPin>("stencils", eNodePinDirection::INPUT);
	stencilListInPin->setElementClassName(GraphStencilProperty::k_propertyClassName);
	stencilListInPin->setHasDefaultValue(true); // Unconnected array pin == no stencils
	node->setStencilsPin(stencilListInPin);

	// Create texture output pin
	auto outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);
	node->setDepthTextureOutPin(outputPin);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the default pins to a compatible target pin
	autoConnectInputPin(editorState, stencilListInPin);
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

const char* DepthMaskNode::editorGetHeaderIcon() const { return ICON_FK_ADJUST; }
