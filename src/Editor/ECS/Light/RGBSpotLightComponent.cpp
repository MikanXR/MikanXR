#include "RGBSpotLightComponent.h"
#include "EditorObjectSystem.h"
#include "Colors.h"
#include "DMXFixtureComponent.h"
#include "DMXObjectSystem.h"
#include "IEditorWindow.h"
#include "IDMXManager.h"
#include "IMkGraphicsContext.h"
#include "IMkState.h"
#include "IMkStaticMeshInstance.h"
#include "IMkTriangulatedMesh.h"
#include "IMkWireframeMesh.h"
#include "MkStateModifiers.h"
#include "MkStateStack.h"
#include "MeshColliderComponent.h"
#include "MikanCamera.h"
#include "MikanLineRenderer.h"
#include "MikanModelResourceManager.h"
#include "MikanObject.h"
#include "MikanRenderModelResource.h"
#include "MikanShaderCache.h"
#include "MikanTextureCache.h"
#include "MikanTextRenderer.h"
#include "MikanLightTypes.h"
#include "MikanVariantTypes.h"
#include "MkMaterialInstance.h"
#include "PathUtils.h"
#include "SelectionComponent.h"
#include "StaticMeshComponent.h"
#include "StringUtils.h"
#include "TextStyle.h"

#include <cmath>

#include "lua.hpp"
#include "LuaBridge/LuaBridge.h"

// -- RGBSpotLightDefinition -----
const std::string RGBSpotLightDefinition::k_coneAngleDegreesPropertyId= "cone_angle_degrees";
const std::string RGBSpotLightDefinition::k_coneRangeMetersPropertyId= "cone_range_meters";

RGBSpotLightDefinition::RGBSpotLightDefinition()
	: DMXFixtureComponentDefinition()
{
	setDMXChannelCount(3);
}

RGBSpotLightDefinition::RGBSpotLightDefinition(MikanLightID lightId)
	: DMXFixtureComponentDefinition(lightId)
{
	setDMXChannelCount(3);
}

void RGBSpotLightDefinition::setConeAngleDegrees(float deg)
{
	if (m_coneAngleDegrees != deg)
	{
		m_coneAngleDegrees= deg;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_coneAngleDegreesPropertyId));
	}
}

void RGBSpotLightDefinition::setConeRangeMeters(float m)
{
	if (m_coneRangeMeters != m)
	{
		m_coneRangeMeters= m;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_coneRangeMetersPropertyId));
	}
}

configuru::Config RGBSpotLightDefinition::writeToJSON()
{
	configuru::Config pt= DMXFixtureComponentDefinition::writeToJSON();

	pt[k_coneAngleDegreesPropertyId]= m_coneAngleDegrees;
	pt[k_coneRangeMetersPropertyId]= m_coneRangeMeters;

	return pt;
}

void RGBSpotLightDefinition::readFromJSON(const configuru::Config& pt)
{
	DMXFixtureComponentDefinition::readFromJSON(pt);

	m_coneAngleDegrees= pt.get_or<float>(k_coneAngleDegreesPropertyId, 30.0f);
	m_coneRangeMeters= pt.get_or<float>(k_coneRangeMetersPropertyId, 2.0f);
}

bool RGBSpotLightDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
												const Serialization::PolymorphicObjectPtr& initParams)
{
	return DMXFixtureComponentDefinition::readFromInitParams(ownerObjectSystem, initParams);
}

// -- RGBSpotLightComponent -----
const std::string RGBSpotLightComponent::k_redPropertyId= "red";
const std::string RGBSpotLightComponent::k_greenPropertyId= "green";
const std::string RGBSpotLightComponent::k_bluePropertyId= "blue";

RGBSpotLightComponent::RGBSpotLightComponent(MikanObjectWeakPtr owner)
	: DMXFixtureComponent(owner)
{
}

static const std::filesystem::path k_spotLightModelRelPath=
	std::filesystem::path("models") / "spot_light" / "spot_light.obj";

void RGBSpotLightComponent::init()
{
	DMXFixtureComponent::init();

	SelectionComponentPtr selectionComponentPtr= getOwnerObject()->getComponentOfType<SelectionComponent>();
	if (selectionComponentPtr)
	{
		selectionComponentPtr->OnInteractionRayOverlapEnter+=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionRayOverlapEnter);
		selectionComponentPtr->OnInteractionRayOverlapExit+=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionRayOverlapExit);
		selectionComponentPtr->OnInteractionSelected+=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionSelected);
		selectionComponentPtr->OnInteractionUnselected+=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionUnselected);
		selectionComponentPtr->OnTransformGizmoBound+=
			MakeDelegate(this, &RGBSpotLightComponent::onTransformGizmoBound);
		selectionComponentPtr->OnTransformGizmoUnbound+=
			MakeDelegate(this, &RGBSpotLightComponent::onTransformGizmoUnbound);

		m_selectionComponent= selectionComponentPtr;
	}

	propogateWorldTransformChange(eTransformChangeType::propogateWorldTransform);
}

void RGBSpotLightComponent::dispose()
{
	SelectionComponentPtr selectionComponentPtr= m_selectionComponent.lock();
	if (selectionComponentPtr)
	{
		selectionComponentPtr->OnInteractionRayOverlapEnter-=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionRayOverlapEnter);
		selectionComponentPtr->OnInteractionRayOverlapExit-=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionRayOverlapExit);
		selectionComponentPtr->OnInteractionSelected-=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionSelected);
		selectionComponentPtr->OnInteractionUnselected-=
			MakeDelegate(this, &RGBSpotLightComponent::onInteractionUnselected);
		selectionComponentPtr->OnTransformGizmoBound-=
			MakeDelegate(this, &RGBSpotLightComponent::onTransformGizmoBound);
		selectionComponentPtr->OnTransformGizmoUnbound-=
			MakeDelegate(this, &RGBSpotLightComponent::onTransformGizmoUnbound);
	}

	DMXFixtureComponent::dispose();
}

void RGBSpotLightComponent::disposeMeshComponents()
{
	while (m_meshComponents.size() > 0)
	{
		TransformComponentPtr componentPtr= m_meshComponents[m_meshComponents.size() - 1];
		componentPtr->dispose();
		m_meshComponents.pop_back();
	}

	m_colliderComponents.clear();
	m_triMeshComponents.clear();
	m_wireframeMeshes.clear();
	m_coneMesh= nullptr;
}

void RGBSpotLightComponent::rebuildMeshComponents()
{
	MikanObjectPtr ownerObject= getOwnerObject();
	RGBSpotLightComponentPtr selfPtr= getSelfPtr<RGBSpotLightComponent>();

	disposeMeshComponents();

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	IMkGraphicsContextPtr graphicsContext= ownerWindow->getGraphicsContext();
	MikanModelResourceManager* modelResourceManager= ownerWindow->getModelResourceManager();
	MikanTextureCache* textureCache= ownerWindow->getTextureCache();
	IMkTexturePtr texture= textureCache->tryGetTextureByName(INTERNAL_TEXTURE_BLACK_RGB);
	MkMaterialConstPtr material=
		graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_PNT_TEXTURED_LIT_COLORED);

	const std::filesystem::path modelPath= PathUtils::makeAbsoluteResourceFilePath(k_spotLightModelRelPath);
	MikanRenderModelResourcePtr modelResourcePtr= modelResourceManager->fetchRenderModel(modelPath, material);

	if (modelResourcePtr)
	{
		// Add static tri meshes + colliders
		for (int meshIndex= 0; meshIndex < modelResourcePtr->getTriangulatedMeshCount(); ++meshIndex)
		{
			IMkTriangulatedMeshPtr triMeshPtr= modelResourcePtr->getTriangulatedMesh(meshIndex);

			IMkStaticMeshInstancePtr triMeshInstancePtr= createMkStaticMeshInstance(triMeshPtr->getName(), triMeshPtr);
			triMeshInstancePtr->setVisible(true);
			triMeshInstancePtr->getMaterialInstance()->setTextureBySemantic(eUniformSemantic::diffuseTexture, texture);

			StaticMeshComponentPtr meshComponentPtr= ownerObject->addComponent<StaticMeshComponent>();
			meshComponentPtr->setName(triMeshPtr->getName());
			meshComponentPtr->setStaticMesh(triMeshInstancePtr);
			meshComponentPtr->attachToComponent(selfPtr);
			m_meshComponents.push_back(meshComponentPtr);
			m_triMeshComponents.push_back(meshComponentPtr);

			MeshColliderComponentPtr colliderPtr= ownerObject->addComponent<MeshColliderComponent>();
			colliderPtr->setName(triMeshPtr->getName());
			colliderPtr->setStaticMeshComponent(meshComponentPtr);
			colliderPtr->attachToComponent(selfPtr);
			m_colliderComponents.push_back(colliderPtr);
			m_meshComponents.push_back(colliderPtr);
		}

		// Add wireframe meshes
		for (int meshIndex= 0; meshIndex < modelResourcePtr->getWireframeMeshCount(); ++meshIndex)
		{
			IMkWireframeMeshPtr wireframeMeshPtr= modelResourcePtr->getWireframeMesh(meshIndex);

			IMkStaticMeshInstancePtr wireframeMeshInstancePtr=
				createMkStaticMeshInstance("wireframe", wireframeMeshPtr);
			m_wireframeMeshes.push_back(wireframeMeshInstancePtr);

			StaticMeshComponentPtr meshComponentPtr= ownerObject->addComponent<StaticMeshComponent>();
			meshComponentPtr->setName(wireframeMeshPtr->getName());
			meshComponentPtr->setStaticMesh(wireframeMeshInstancePtr);
			meshComponentPtr->attachToComponent(selfPtr);
			m_meshComponents.push_back(meshComponentPtr);
		}

		updateWireframeMeshColor();

		for (TransformComponentPtr childComponentPtr : m_meshComponents)
			childComponentPtr->init();

		for (TransformComponentPtr childComponentPtr : m_meshComponents)
			childComponentPtr->postInit();
	}

	// Rebuild the collider list on the selection component
	SelectionComponentPtr selectionComponentPtr= m_selectionComponent.lock();
	if (selectionComponentPtr)
		selectionComponentPtr->rebindColliders();

	rebuildConeMesh();
}

void RGBSpotLightComponent::updateWireframeMeshColor()
{
	glm::vec3 color;

	if (m_bIsTransformGizmoBound)
	{
		color= Colors::GreenYellow;
	}
	else if (m_bIsSelected)
	{
		color= Colors::Yellow;
	}
	else if (m_bIsHovered)
	{
		color= Colors::LightGray;
	}
	else
	{
		const float r= m_red / 255.0f;
		const float g= m_green / 255.0f;
		const float b= m_blue / 255.0f;
		color= (r + g + b > 0.01f) ? glm::vec3(r, g, b) : Colors::DarkGray;
	}

	for (IMkStaticMeshInstancePtr meshPtr : m_wireframeMeshes)
	{
		meshPtr->getMaterialInstance()->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, glm::vec4(color, 1.f));
	}
}

void RGBSpotLightComponent::rebuildConeMesh()
{
	m_coneMesh= nullptr;

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	if (!ownerWindow)
		return;

	IMkGraphicsContextPtr graphicsContext= ownerWindow->getGraphicsContext();

	// Build position-only cone (tip at origin, base circle at z=-1, radius=1, N segments)
	constexpr int N= 16;
	struct PosVert
	{
		float x, y, z;
	};
	std::vector<PosVert> verts;
	verts.reserve(N + 2);
	verts.push_back({0.f, 0.f, 0.f}); // tip: index 0
	for (int i= 0; i < N; ++i)
	{
		const float a= (float)i / (float)N * (2.0f * 3.14159265f);
		verts.push_back({cosf(a), sinf(a), -1.f}); // base circle: indices 1..N
	}
	verts.push_back({0.f, 0.f, -1.f}); // base center: index N+1

	std::vector<uint32_t> indices;
	indices.reserve(2 * N * 3);
	for (int i= 0; i < N; ++i)
	{
		// Lateral triangle: tip -> edge[i] -> edge[i+1]
		indices.push_back(0);
		indices.push_back(i + 1);
		indices.push_back((i + 1) % N + 1);
		// Cap triangle: center -> edge[i+1] -> edge[i]  (reversed winding for back face)
		indices.push_back(N + 1);
		indices.push_back((i + 1) % N + 1);
		indices.push_back(i + 1);
	}

	m_coneMesh= createMkTriangulatedMesh(
		graphicsContext.get(), "spotLightCone", reinterpret_cast<const uint8_t*>(verts.data()), sizeof(PosVert),
		(uint32_t)verts.size(), reinterpret_cast<const uint8_t*>(indices.data()), sizeof(uint32_t), N * 2,
		false); // data uploaded to GPU in createResources(), no need for mesh to own CPU copy

	if (m_coneMesh)
	{
		MkMaterialConstPtr material=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_CONE_VOLUME);
		m_coneMesh->setMaterial(material);
		m_coneMesh->createResources();
		updateConeColor();
	}
}

void RGBSpotLightComponent::updateConeColor()
{
	if (!m_coneMesh)
		return;
	const glm::vec4 col(m_red / 255.f, m_green / 255.f, m_blue / 255.f, k_coneAlpha);
	m_coneMesh->getMaterialInstance()->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, col);
}

void RGBSpotLightComponent::onInteractionRayOverlapEnter(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= true;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::onInteractionRayOverlapExit(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= false;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::onInteractionSelected()
{
	m_bIsSelected= true;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::onInteractionUnselected()
{
	m_bIsSelected= false;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::onTransformGizmoBound()
{
	m_bIsTransformGizmoBound= true;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::onTransformGizmoUnbound()
{
	m_bIsTransformGizmoBound= false;
	updateWireframeMeshColor();
}

void RGBSpotLightComponent::setRed(uint8_t v) { setRGB(v, m_green, m_blue); }

void RGBSpotLightComponent::setGreen(uint8_t v) { setRGB(m_red, v, m_blue); }

void RGBSpotLightComponent::setBlue(uint8_t v) { setRGB(m_red, m_green, v); }

void RGBSpotLightComponent::setRGB(uint8_t r, uint8_t g, uint8_t b)
{
	if (r != m_red || g != m_green || b != m_blue)
	{
		m_red= r;
		m_green= g;
		m_blue= b;

		updateWireframeMeshColor();
		updateConeColor();
		sendDMXData();
	}
}

void RGBSpotLightComponent::getChannelValues(std::vector<uint8_t>& outValues) const
{
	outValues= {m_red, m_green, m_blue};
}

void RGBSpotLightComponent::setChannelValues(const std::vector<uint8_t>& values)
{
	const uint8_t r= values.size() > 0 ? values[0] : 0;
	const uint8_t g= values.size() > 1 ? values[1] : 0;
	const uint8_t b= values.size() > 2 ? values[2] : 0;
	setRGB(r, g, b);
}

void RGBSpotLightComponent::sendDMXData() const
{
	DMXObjectSystemPtr dmxObjectSystem= getDMXObjectSystem();
	RGBSpotLightDefinitionPtr def= getRGBSpotLightDefinition();
	if (!def || def->getIsDisabled() || !dmxObjectSystem)
		return;

	const uint8_t rgb[3]= {getRed(), getGreen(), getBlue()};
	dmxObjectSystem->writeUniverseData(def->getDMXUniverse(), def->getDMXStartChannel(), rgb, 3);
}

// -- IEntityAccessor --
rfk::Struct const* RGBSpotLightComponent::getClientAPIValuesStructType() const
{
	return &MikanRGBSpotLightComponentValues::staticGetArchetype();
}

// -- IPropertyInterface --
void RGBSpotLightComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	DMXFixtureComponent::getPropertyDescriptors(outDescriptors);

	// Don't expose the DMX values in the ComponentValues ClientAPI struct
	// since that data is sent via the DMX universe buffer update events
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBSpotLightComponent::k_redPropertyId, MikanVariantType::UBYTE)
			->setClientAPIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBSpotLightComponent::k_greenPropertyId, MikanVariantType::UBYTE)
			->setClientAPIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(RGBSpotLightComponent::k_bluePropertyId, MikanVariantType::UBYTE)
			->setClientAPIHidden());

	// Spotlight properties
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(RGBSpotLightDefinition::k_coneAngleDegreesPropertyId,
																  MikanVariantType::FLOAT));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(RGBSpotLightDefinition::k_coneRangeMetersPropertyId,
																  MikanVariantType::FLOAT));
}

bool RGBSpotLightComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	RGBSpotLightDefinitionPtr def= getRGBSpotLightDefinition();

	if (propertyName == RGBSpotLightComponent::k_redPropertyId)
	{
		outValue= getRed();
		return true;
	}
	else if (propertyName == RGBSpotLightComponent::k_greenPropertyId)
	{
		outValue= getGreen();
		return true;
	}
	else if (propertyName == RGBSpotLightComponent::k_bluePropertyId)
	{
		outValue= getBlue();
		return true;
	}
	else if (propertyName == RGBSpotLightDefinition::k_coneAngleDegreesPropertyId)
	{
		if (def)
		{
			outValue= def->getConeAngleDegrees();
			return true;
		}
	}
	else if (propertyName == RGBSpotLightDefinition::k_coneRangeMetersPropertyId)
	{
		if (def)
		{
			outValue= def->getConeRangeMeters();
			return true;
		}
	}

	return DMXFixtureComponent::getPropertyValue(propertyName, outValue);
}

bool RGBSpotLightComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	RGBSpotLightDefinitionPtr def= getRGBSpotLightDefinition();

	if (propertyName == RGBSpotLightComponent::k_redPropertyId)
	{
		setRed(inValue.getUByteValue());
		return true;
	}
	else if (propertyName == RGBSpotLightComponent::k_greenPropertyId)
	{
		setGreen(inValue.getUByteValue());
		return true;
	}
	else if (propertyName == RGBSpotLightComponent::k_bluePropertyId)
	{
		setBlue(inValue.getUByteValue());
		return true;
	}
	else if (propertyName == RGBSpotLightDefinition::k_coneAngleDegreesPropertyId)
	{
		if (def)
		{
			def->setConeAngleDegrees(inValue.getFloatValue());
			return true;
		}
	}
	else if (propertyName == RGBSpotLightDefinition::k_coneRangeMetersPropertyId)
	{
		if (def)
		{
			def->setConeRangeMeters(inValue.getFloatValue());
			return true;
		}
	}

	return DMXFixtureComponent::setPropertyValue(propertyName, inValue);
}

// -- customRender --
void RGBSpotLightComponent::customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	RGBSpotLightDefinitionPtr def= getRGBSpotLightDefinition();
	if (!def || def->getIsDisabled())
		return;

	if (!m_bIsTransformGizmoBound)
	{
		const glm::mat4 xform= getWorldTransform();
		const glm::vec3 position= glm::vec3(xform[3]);

		drawTransformedAxes(graphicsContext, xform, 0.05f, 0.05f, 0.05f);

		if (getObjectSystemOfType<EditorObjectSystem>()->getEditorSettings().bRenderComponentNames)
		{
			TextStyle style= getDefaultTextStyle();
			wchar_t wszName[256];
			StringUtils::convertMbsToWcs(def->getComponentName().c_str(), wszName, sizeof(wszName));
			drawTextAtWorldPosition(graphicsContext, style, position, L"%s [%d,%d,%d]", wszName,
									static_cast<int>(getRed()), static_cast<int>(getGreen()),
									static_cast<int>(getBlue()));
		}
	}
}

void RGBSpotLightComponent::renderConeVolume(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	RGBSpotLightDefinitionPtr def= getRGBSpotLightDefinition();
	if (!def || def->getIsDisabled())
		return;

	if (m_coneMesh)
	{
		const float halfAngleRad= def->getConeAngleDegrees() * 0.5f * (3.14159265f / 180.0f);
		const float range= def->getConeRangeMeters();
		const float radius= tanf(halfAngleRad) * range;

		// Scale the unit cone (radius=1, height=1 along -Z) to match light properties
		glm::mat4 coneScale(1.f);
		coneScale[0][0]= radius;
		coneScale[1][1]= radius;
		coneScale[2][2]= range;
		const glm::mat4 coneXform= getWorldTransform() * coneScale;

		MkStateStack& stateStack= graphicsContext->getMkStateStack();
		IMkState* coneState= stateStack.pushState("spotLightConeVolume");
		coneState->enableFlag(eMkStateFlagType::blend);
		coneState->disableFlag(eMkStateFlagType::cullFace);
		mkStateSetBlendFunc(coneState, eMkBlendFunction::ONE, eMkBlendFunction::ONE);
		mkStateSetDepthMask(coneState, false);

		drawTransformedTriangulatedMesh(viewportCamera, coneXform, m_coneMesh);

		stateStack.popState();
	}
}

// -- Lua Binding --
void RGBSpotLightComponent::bindLuaFunctions(lua_State* L)
{
	luabridge::getGlobalNamespace(L)
		.deriveClass<RGBSpotLightComponent, DMXFixtureComponent>(RGBSpotLightComponent::k_componentClassName.c_str())
		.addProperty(
			"red", [](RGBSpotLightComponent* c) -> int { return static_cast<int>(c->getRed()); },
			[](RGBSpotLightComponent* c, int v) { c->setRed(static_cast<uint8_t>(v)); })
		.addProperty(
			"green", [](RGBSpotLightComponent* c) -> int { return static_cast<int>(c->getGreen()); },
			[](RGBSpotLightComponent* c, int v) { c->setGreen(static_cast<uint8_t>(v)); })
		.addProperty(
			"blue", [](RGBSpotLightComponent* c) -> int { return static_cast<int>(c->getBlue()); },
			[](RGBSpotLightComponent* c, int v) { c->setBlue(static_cast<uint8_t>(v)); })
		.addFunction("setRGB", [](RGBSpotLightComponent* c, int r, int g, int b)
					 { c->setRGB(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)); })
		.addProperty(
			"coneAngleDegrees",
			[](RGBSpotLightComponent* c) -> float { return c->getRGBSpotLightDefinition()->getConeAngleDegrees(); },
			[](RGBSpotLightComponent* c, float v) { c->getRGBSpotLightDefinition()->setConeAngleDegrees(v); })
		.addProperty(
			"coneRangeMeters",
			[](RGBSpotLightComponent* c) -> float { return c->getRGBSpotLightDefinition()->getConeRangeMeters(); },
			[](RGBSpotLightComponent* c, float v) { c->getRGBSpotLightDefinition()->setConeRangeMeters(v); })
		.endClass();
}
