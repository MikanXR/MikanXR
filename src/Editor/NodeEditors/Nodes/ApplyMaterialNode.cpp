#include "ApplyMaterialNode.h"
#include "IconsForkAwesome.h"
#include "IMkFrameBuffer.h"
#include "IMkGraphicsContext.h"
#include "IMkShader.h"
#include "IMkTexture.h"
#include "IMkTriangulatedMesh.h"
#include "LocText.h"
#include "Logger.h"
#include "MkGuiDrawUtils.h"
#include "MkGuiStyleManager.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkScopedObjectBinding.h"
#include "MkStateStack.h"
#include "NodeEditorState.h"

#include "Graphs/CompositorNodeGraph.h"
#include "Graphs/NodeEvaluator.h"

#include "Pins/FloatPin.h"
#include "Pins/NodePin.h"
#include "Pins/PropertyPin.h"
#include "Pins/TexturePin.h"

#include "Properties/GraphMaterialProperty.h"

#include "StringUtils.h"

#include "imgui.h"
#include "MkCanvasScopedNode.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

// -- ApplyMaterialNodeConfig -----
configuru::Config ApplyMaterialNodeConfig::writeToJSON()
{
	configuru::Config pt= NodeConfig::writeToJSON();

	CommonConfig::writeStdMap(pt, "float_defaults", m_floatDefaults);
	CommonConfig::writeStdArrayMap<float, 2>(pt, "float2_defaults", m_float2Defaults);
	CommonConfig::writeStdArrayMap<float, 3>(pt, "float3_defaults", m_float3Defaults);
	CommonConfig::writeStdArrayMap<float, 4>(pt, "float4_defaults", m_float4Defaults);

	return pt;
}

void ApplyMaterialNodeConfig::readFromJSON(const configuru::Config& pt)
{
	NodeConfig::readFromJSON(pt);

	CommonConfig::readStdMap(pt, "float_defaults", m_floatDefaults);
	CommonConfig::readStdArrayMap(pt, "float2_defaults", m_float2Defaults);
	CommonConfig::readStdArrayMap(pt, "float3_defaults", m_float3Defaults);
	CommonConfig::readStdArrayMap(pt, "float4_defaults", m_float4Defaults);
}

// -- ApplyMaterialNode -----
ApplyMaterialNode::ApplyMaterialNode()
	: Node()
{
	// Create the frame buffer, but don't init its internals yet.
	// Wait until evaluation to get the right output texture size.
	// Use a 16-bit color format to match the compositor's own working buffer precision
	// (avoids banding when this node's result gets blended into a DrawLayerNode downstream).
	m_outputFrameBuffer= createMkFrameBuffer("ApplyMaterialNode");
	m_outputFrameBuffer->setFrameBufferType(IMkFrameBuffer::eFrameBufferType::COLOR);
	m_outputFrameBuffer->setColorFormat(IMkFrameBuffer::eColorFormat::RGBA16);
}

ApplyMaterialNode::~ApplyMaterialNode()
{
	// Clean up render resources
	m_outputFrameBuffer= nullptr;
	m_materialInstance= nullptr;
	m_material= nullptr;

	// Free pin references
	m_materialPin= nullptr;
	m_outTexturePin= nullptr;

	// Stop listening to events from owner graph
	setOwnerGraph(NodeGraphPtr());
}

bool ApplyMaterialNode::loadFromConfig(NodeConfigConstPtr nodeConfig)
{
	if (Node::loadFromConfig(nodeConfig))
	{
		auto applyMaterialNodeConfig= std::static_pointer_cast<const ApplyMaterialNodeConfig>(nodeConfig);

		m_floatDefaults= applyMaterialNodeConfig->m_floatDefaults;
		m_float2Defaults= applyMaterialNodeConfig->m_float2Defaults;
		m_float3Defaults= applyMaterialNodeConfig->m_float3Defaults;
		m_float4Defaults= applyMaterialNodeConfig->m_float4Defaults;

		return true;
	}

	return false;
}

void ApplyMaterialNode::onGraphLoaded(bool success)
{
	if (success)
	{
		// Make sure we have a material input pin
		PropertyPinPtr materialInPin= getFirstPinOfType<PropertyPin>(eNodePinDirection::INPUT);
		if (materialInPin && materialInPin->getPropertyClassName() == GraphMaterialProperty::k_propertyClassName)
		{
			setMaterialPin(materialInPin);
			m_materialPin->copyValueFromSourcePin();

			auto materialProperty= std::dynamic_pointer_cast<GraphMaterialProperty>(m_materialPin->getValue());
			if (materialProperty)
			{
				setMaterial(materialProperty->getMaterialResource());
				// Loaded pins carry the flags of the material as it was saved, so refresh them against
				// the material as loaded now (a default texture makes its pin optional)
				rebuildInputPins();
			}
		}

		// Make sure we have a texture output pin
		setOutputTexturePin(getFirstPinOfType<TexturePin>(eNodePinDirection::OUTPUT));
	}

	applyDynamicPinDefaultValues();
}

void ApplyMaterialNode::saveToConfig(NodeConfigPtr nodeConfig) const
{
	auto applyMaterialNodeConfig= std::static_pointer_cast<ApplyMaterialNodeConfig>(nodeConfig);

	for (auto& pin : m_pinsIn)
	{
		if (pin->getIsDynamicPin())
		{
			if (FloatPinPtr floatPin= std::dynamic_pointer_cast<FloatPin>(pin))
			{
				applyMaterialNodeConfig->m_floatDefaults[floatPin->getName()]= floatPin->getValue();
			}
			else if (Float2PinPtr float2Pin= std::dynamic_pointer_cast<Float2Pin>(pin))
			{
				applyMaterialNodeConfig->m_float2Defaults[float2Pin->getName()]= float2Pin->getValue();
			}
			else if (Float3PinPtr float3Pin= std::dynamic_pointer_cast<Float3Pin>(pin))
			{
				applyMaterialNodeConfig->m_float3Defaults[float3Pin->getName()]= float3Pin->getValue();
			}
			else if (Float4PinPtr float4Pin= std::dynamic_pointer_cast<Float4Pin>(pin))
			{
				applyMaterialNodeConfig->m_float4Defaults[float4Pin->getName()]= float4Pin->getValue();
			}
		}
	}

	Node::saveToConfig(nodeConfig);
}

void ApplyMaterialNode::setOwnerGraph(NodeGraphPtr newOwnerGraph)
{
	if (newOwnerGraph != m_ownerGraph)
	{
		if (m_ownerGraph)
		{
			m_ownerGraph->OnGraphLoaded-= MakeDelegate(this, &ApplyMaterialNode::onGraphLoaded);
			m_ownerGraph->OnPropertyModifed-= MakeDelegate(this, &ApplyMaterialNode::onGraphPropertyModified);
			m_ownerGraph= nullptr;
		}

		if (newOwnerGraph)
		{
			newOwnerGraph->OnGraphLoaded+= MakeDelegate(this, &ApplyMaterialNode::onGraphLoaded);
			newOwnerGraph->OnPropertyModifed+= MakeDelegate(this, &ApplyMaterialNode::onGraphPropertyModified);
			m_ownerGraph= newOwnerGraph;
		}
	}
}

void ApplyMaterialNode::onGraphPropertyModified(t_graph_property_id id)
{
	if (!m_materialPin || isPendingDeletion())
		return;

	auto materialProperty= std::dynamic_pointer_cast<GraphMaterialProperty>(m_materialPin->getValue());
	if (materialProperty && materialProperty->getId() == id)
	{
		// Recreate the instance against the recompiled program and track its uniforms
		setMaterial(materialProperty->getMaterialResource());
		rebuildInputPins();
	}
}

bool ApplyMaterialNode::editorCanAcceptProperty(NodePinPtr pin, GraphPropertyPtr property) const
{
	if (pin != m_materialPin)
		return true;

	auto materialProperty= std::dynamic_pointer_cast<GraphMaterialProperty>(property);
	return !materialProperty || materialProperty->isCompatibleWithDomain(eMaterialDomain::compositor);
}

void ApplyMaterialNode::setMaterialPin(PropertyPinPtr inPin) { m_materialPin= inPin; }

void ApplyMaterialNode::setOutputTexturePin(TexturePinPtr outPin) { m_outTexturePin= outPin; }

void ApplyMaterialNode::setMaterial(MkMaterialConstPtr inMaterial)
{
	m_material= inMaterial;
	m_materialInstance= m_material ? createMkMaterialInstance(inMaterial) : MkMaterialInstancePtr();
}

bool ApplyMaterialNode::evaluateNode(NodeEvaluator& evaluator)
{
	bool bSuccess= true;

	if (!m_material)
	{
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "Missing material", this));
		if (m_outTexturePin)
			m_outTexturePin->setValue(IMkTexturePtr());

		return false;
	}

	if (!evaluateInputs(evaluator))
	{
		if (m_outTexturePin)
			m_outTexturePin->setValue(IMkTexturePtr());

		return false;
	}

	// Find the first bound dynamic texture input pin and use its resolution
	// to size the output frame buffer (mirrors ColorTextureSourceNode's own frame buffer sizing)
	IMkTexturePtr sizeSourceTexture;
	for (auto& pin : m_pinsIn)
	{
		if (!pin->getIsDynamicPin())
			continue;

		if (TexturePinPtr texturePin= std::dynamic_pointer_cast<TexturePin>(pin))
		{
			if (texturePin->getValue())
			{
				sizeSourceTexture= texturePin->getValue();
				break;
			}
		}
	}

	if (!sizeSourceTexture)
	{
		evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::missingInput, "No input texture bound", this));
		if (m_outTexturePin)
			m_outTexturePin->setValue(IMkTexturePtr());

		return false;
	}

	// Update render target size
	m_outputFrameBuffer->setSize(sizeSourceTexture->getTextureWidth(), sizeSourceTexture->getTextureHeight());

	// Update render resources if the frame buffer is not valid
	if (!m_outputFrameBuffer->isValid())
	{
		if (!m_outputFrameBuffer->createResources())
		{
			evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError,
												   "Unable to create output frame buffer", this));
			if (m_outTexturePin)
				m_outTexturePin->setValue(IMkTexturePtr());

			return false;
		}
	}

	// Map input pins to the material
	for (auto& pin : m_pinsIn)
	{
		// Only consider dynamic pins
		if (!pin->getIsDynamicPin())
			continue;

		if (FloatPinPtr floatPin= std::dynamic_pointer_cast<FloatPin>(pin))
		{
			m_materialInstance->setFloatByUniformName(pin->getName(), floatPin->getValue());
		}
		else if (Float2PinPtr float2Pin= std::dynamic_pointer_cast<Float2Pin>(pin))
		{
			m_materialInstance->setVec2ByUniformName(pin->getName(), glm::make_vec2(float2Pin->getValue().data()));
		}
		else if (Float3PinPtr float3Pin= std::dynamic_pointer_cast<Float3Pin>(pin))
		{
			m_materialInstance->setVec3ByUniformName(pin->getName(), glm::make_vec3(float3Pin->getValue().data()));
		}
		else if (Float4PinPtr float4Pin= std::dynamic_pointer_cast<Float4Pin>(pin))
		{
			m_materialInstance->setVec4ByUniformName(pin->getName(), glm::make_vec4(float4Pin->getValue().data()));
		}
		else if (TexturePinPtr texturePin= std::dynamic_pointer_cast<TexturePin>(pin))
		{
			IMkTexturePtr texturePtr= texturePin->getValue();
			if (texturePtr)
			{
				m_materialInstance->setTextureByUniformName(pin->getName(), texturePtr);
			}
		}
	}

	{
		IMkGraphicsContext* graphicsContext= evaluator.getCurrentGraphicsContext();
		MkScopedObjectBinding frameBufferBinding(graphicsContext->getMkStateStack().getCurrentState(),
												 "Apply Material Node", m_outputFrameBuffer);

		if (frameBufferBinding)
		{
			// Bind the material shader program and uniform parameters.
			// This will fail unless all of the shader uniform parameters are bound.
			MkScopedMaterialBinding materialBinding= m_material->bindMaterial();
			if (materialBinding)
			{
				MkScopedMaterialInstanceBinding materialInstanceBinding=
					m_materialInstance->bindMaterialInstance(materialBinding);

				if (materialInstanceBinding)
				{
					auto compositorGraph= std::static_pointer_cast<CompositorNodeGraph>(getOwnerGraph());

					// Non-V-flipped: this node is a transparent pass-through and must not alter orientation
					compositorGraph->getLayerMesh()->drawElements();
				}
				else
				{
					evaluator.addError(
						NodeEvaluationError(eNodeEvaluationErrorCode::materialError,
											StringUtils::stringify("Unable to bind ", m_material->getName()), this));
					for (const auto& iter : materialInstanceBinding.getUnboundUniforms())
					{
						evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::materialError,
															   StringUtils::stringify("Missing uniform: ", iter),
															   this));
					}
					bSuccess= false;
				}
			}
			else
			{
				evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::materialError,
													   StringUtils::stringify("Unable to bind ", m_material->getName()),
													   this));
				for (const auto& iter : materialBinding.getUnboundUniforms())
				{
					evaluator.addError(NodeEvaluationError(eNodeEvaluationErrorCode::materialError,
														   StringUtils::stringify("Missing uniform: ", iter), this));
				}
				bSuccess= false;
			}
		}
		else
		{
			evaluator.addError(
				NodeEvaluationError(eNodeEvaluationErrorCode::evaluationError, "Broken frame buffer", this));
			bSuccess= false;
		}
	}

	m_outTexturePin->setValue(bSuccess ? m_outputFrameBuffer->getColorTexture() : IMkTexturePtr());

	return bSuccess;
}

void ApplyMaterialNode::editorRenderNode(const NodeEditorState& editorState)
{
	MkCanvasScopedNode scopedNode(m_id, editorGetHeaderColor());

	// Title
	editorRenderTitle(scopedNode);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));

	// Inputs
	editorRenderInputPins(editorState);

	// Texture Preview
	ImGui::Dummy(ImVec2(1.0f, 0.5f));
	IMkTexturePtr colorTexture= m_outputFrameBuffer ? m_outputFrameBuffer->getColorTexture() : IMkTexturePtr();
	MkGui::drawImage(colorTexture, 100.f, 100.f);
	ImGui::SameLine();

	// Outputs
	editorRenderOutputPins(editorState);

	ImGui::Dummy(ImVec2(1.0f, 0.5f));
}

void ApplyMaterialNode::editorOnDoubleClicked(const NodeEditorState& editorState)
{
	auto materialProperty= m_materialPin ? std::dynamic_pointer_cast<GraphMaterialProperty>(m_materialPin->getValue())
										 : GraphMaterialPropertyPtr();
	if (materialProperty)
	{
		materialProperty->editorOpenSourceGraph();
	}
}

void ApplyMaterialNode::editorRenderPropertySheet(const NodeEditorState& editorState)
{
	if (MkGui::drawPropertySheetHeader(editorState.styleManager->getStyle("node_editor_panel_header"),
									   locText("nodes.applyMaterialHeader")))
	{
		const std::string material_name= m_material ? m_material->getName() : "<INVALID>";
		MkGui::drawStaticTextProperty(editorState.styleManager->getStyle("node_editor_property_value"),
									  locText("nodes.material"), material_name);
	}
}

void ApplyMaterialNode::onLinkConnected(NodeLinkPtr link, NodePinPtr pin)
{
	if (pin == m_materialPin)
	{
		m_materialPin->copyValueFromSourcePin();

		auto materialProperty= std::dynamic_pointer_cast<GraphMaterialProperty>(m_materialPin->getValue());
		if (materialProperty)
		{
			setMaterial(materialProperty->getMaterialResource());

			// Rebuild the pins since the material changed
			rebuildInputPins();
		}
	}
}

void ApplyMaterialNode::onLinkDisconnected(NodeLinkPtr link, NodePinPtr pin)
{
	if (pin == m_materialPin)
	{
		setMaterial(MkMaterialConstPtr());

		// Rebuild the pins since the material changed
		if (!isPendingDeletion())
		{
			rebuildInputPins();
		}
	}
}

void ApplyMaterialNode::rebuildInputPins()
{
	assert(!isPendingDeletion());

	// Index the dynamic pins by name. A pin whose uniform survives with the same
	// data type is kept, so its links and value carry across a material reload.
	captureDynamicPinDefaultValues();
	std::map<std::string, NodePinPtr> unclaimedPins;
	for (NodePinPtr pin : m_pinsIn)
	{
		if (pin->getIsDynamicPin())
		{
			unclaimedPins.insert({pin->getName(), pin});
		}
	}

	// Claim or create an input pin for each shader uniform, in uniform order
	std::vector<NodePinPtr> dynamicPins;
	IMkShaderPtr program= m_material ? m_material->getProgram() : IMkShaderPtr();
	if (program)
	{
		for (auto it= program->getUniformBegin(); it != program->getUniformEnd(); ++it)
		{
			const std::string& uniformName= it->first;
			const eUniformDataType uniformDataType= getUniformSemanticDataType(it->second.semantic);
			const std::string& pinClassName= GraphMaterialProperty::getUniformPinClassName(uniformDataType);
			if (pinClassName.empty())
			{
				assert(false);
				continue;
			}

			NodePinPtr pin;
			auto unclaimedIt= unclaimedPins.find(uniformName);
			if (unclaimedIt != unclaimedPins.end() && unclaimedIt->second->getClassName() == pinClassName)
			{
				pin= unclaimedIt->second;
				unclaimedPins.erase(unclaimedIt);
			}
			else
			{
				pin= addPinByClassName(pinClassName, uniformName, eNodePinDirection::INPUT);
				pin->setIsDynamicPin(true);
				GraphMaterialProperty::initDynamicPinFromMaterialDefault(pin, m_material);
			}

			// A texture the material carries a default for need not be connected
			if (uniformDataType == eUniformDataType::datatype_texture)
			{
				IMkTextureConstPtr defaultTexture;
				pin->setHasDefaultValue(m_material->getTextureByUniformName(uniformName, defaultTexture)
										&& defaultTexture != nullptr);
			}

			dynamicPins.push_back(pin);
		}
	}

	// Delete the dynamic pins no uniform claimed
	for (const auto& [pinName, unclaimedPin] : unclaimedPins)
	{
		getOwnerGraph()->deletePinById(unclaimedPin->getId());
	}

	// Keep the dynamic pins after the fixed ones, in uniform order
	m_pinsIn.erase(
		std::remove_if(m_pinsIn.begin(), m_pinsIn.end(), [](const NodePinPtr& pin) { return pin->getIsDynamicPin(); }),
		m_pinsIn.end());
	m_pinsIn.insert(m_pinsIn.end(), dynamicPins.begin(), dynamicPins.end());

	// Update dynamic pin default values
	applyDynamicPinDefaultValues();
}

void ApplyMaterialNode::captureDynamicPinDefaultValues()
{
	for (auto& pin : m_pinsIn)
	{
		if (!pin->getIsDynamicPin())
			continue;

		if (FloatPinPtr floatPin= std::dynamic_pointer_cast<FloatPin>(pin))
		{
			m_floatDefaults[floatPin->getName()]= floatPin->getValue();
		}
		else if (Float2PinPtr float2Pin= std::dynamic_pointer_cast<Float2Pin>(pin))
		{
			m_float2Defaults[float2Pin->getName()]= float2Pin->getValue();
		}
		else if (Float3PinPtr float3Pin= std::dynamic_pointer_cast<Float3Pin>(pin))
		{
			m_float3Defaults[float3Pin->getName()]= float3Pin->getValue();
		}
		else if (Float4PinPtr float4Pin= std::dynamic_pointer_cast<Float4Pin>(pin))
		{
			m_float4Defaults[float4Pin->getName()]= float4Pin->getValue();
		}
	}
}

void ApplyMaterialNode::applyDynamicPinDefaultValues()
{
	for (auto& pin : m_pinsIn)
	{
		if (pin->getIsDynamicPin())
		{
			const std::string& pinName= pin->getName();

			if (FloatPinPtr floatPin= std::dynamic_pointer_cast<FloatPin>(pin))
			{
				auto defaultIt= m_floatDefaults.find(pinName);
				if (defaultIt != m_floatDefaults.end())
				{
					floatPin->setValue(defaultIt->second);
				}
			}
			else if (Float2PinPtr float2Pin= std::dynamic_pointer_cast<Float2Pin>(pin))
			{
				auto defaultIt= m_float2Defaults.find(pinName);
				if (defaultIt != m_float2Defaults.end())
				{
					float2Pin->setValue(defaultIt->second);
				}
			}
			else if (Float3PinPtr float3Pin= std::dynamic_pointer_cast<Float3Pin>(pin))
			{
				auto defaultIt= m_float3Defaults.find(pinName);
				if (defaultIt != m_float3Defaults.end())
				{
					float3Pin->setValue(defaultIt->second);
				}
			}
			else if (Float4PinPtr float4Pin= std::dynamic_pointer_cast<Float4Pin>(pin))
			{
				auto defaultIt= m_float4Defaults.find(pinName);
				if (defaultIt != m_float4Defaults.end())
				{
					float4Pin->setValue(defaultIt->second);
				}
			}
		}
	}
}

// -- ApplyMaterialNode Factory -----
NodePtr ApplyMaterialNodeFactory::createNode(const NodeEditorState& editorState) const
{
	// Create the node and default pins
	// The rest of the input pins can't be connected until we have a material assigned
	auto node= std::static_pointer_cast<ApplyMaterialNode>(NodeFactory::createNode(editorState));

	PropertyPinPtr materialInPin= node->addPin<PropertyPin>("material", eNodePinDirection::INPUT);
	materialInPin->setPropertyClassName(GraphMaterialProperty::k_propertyClassName);
	node->setMaterialPin(materialInPin);

	TexturePinPtr outputPin= node->addPin<TexturePin>("texture", eNodePinDirection::OUTPUT);
	outputPin->editorSetShowPinName(false);
	node->setOutputTexturePin(outputPin);

	// If spawned in an editor context from a dangling pin link
	// auto-connect the default pins to a compatible target pin
	autoConnectInputPin(editorState, materialInPin);
	autoConnectOutputPin(editorState, outputPin);

	return node;
}

const char* ApplyMaterialNode::editorGetHeaderIcon() const { return ICON_FK_MAGIC; }
