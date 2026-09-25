#include "LightEnvironmentComponent.h"
#include "CameraObjectSystem.h"
#include "IEditorWindow.h"
#include "IMkGraphicsContext.h"
#include "IMkShaderCache.h"
#include "IMkState.h"
#include "IMkTriangulatedMesh.h"
#include "MathTypeConversion.h"
#include "MikanCamera.h"
#include "MikanObject.h"
#include "MkMaterial.h"
#include "MkMaterialInstance.h"
#include "MkStateStack.h"
#include "ModalSelectCamera/ModalDialog_SelectCamera.h"
#include "PropertyInterface.h"
#include "SceneLightingCapture/AppStage_SceneLightingCapture.h"
#include "SerializableList.h"
#include "StageComponent.h"

#include <cmath>
#include <vector>

// The flat coefficient list is always exactly this long: 9 SH coefficients
// times 3 color channels.
static constexpr int k_shCoefficientFloatCount= k_shCoefficientCount * 3;

// -- LightEnvironmentDefinition -----
const std::string LightEnvironmentDefinition::k_shCoefficientsPropertyId= "sh_coefficients";
const std::string LightEnvironmentDefinition::k_exposureScalePropertyId= "exposure_scale";
const std::string LightEnvironmentDefinition::k_directionalityPropertyId= "directionality";
const std::string LightEnvironmentDefinition::k_keyLightDirectionPropertyId= "key_light_direction";

LightEnvironmentDefinition::LightEnvironmentDefinition()
	: TransformComponentDefinition()
	, m_shCoefficients((size_t)k_shCoefficientFloatCount, 0.f)
{
}

LightEnvironmentDefinition::LightEnvironmentDefinition(MikanLightID lightId)
	: TransformComponentDefinition(lightId)
	, m_shCoefficients((size_t)k_shCoefficientFloatCount, 0.f)
{
}

// Every setter must call notifyPropertyChanged. It is not just a UI hint: it
// arms the config auto-save cooldown AND fires CommonConfig::OnPropertyChanged,
// which is what PropertyRequestHandler forwards to connected clients. Writing
// the member without notifying leaves the value live in memory only - it never
// reaches the project file and no client ever learns it changed.
void LightEnvironmentDefinition::setSHCoefficients(const std::vector<float>& coefficients)
{
	// Normalize the length here so every reader can index unconditionally.
	m_shCoefficients.assign((size_t)k_shCoefficientFloatCount, 0.f);
	const size_t count= std::min(coefficients.size(), (size_t)k_shCoefficientFloatCount);
	for (size_t i= 0; i < count; ++i)
		m_shCoefficients[i]= coefficients[i];

	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_shCoefficientsPropertyId));
}

void LightEnvironmentDefinition::setExposureScale(float scale)
{
	m_exposureScale= scale;

	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_exposureScalePropertyId));
}

void LightEnvironmentDefinition::setDirectionality(float directionality)
{
	m_directionality= directionality;

	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_directionalityPropertyId));
}

void LightEnvironmentDefinition::setKeyLightDirection(const MikanVector3f& direction)
{
	m_keyLightDirection= direction;

	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_keyLightDirectionPropertyId));
}

SHLightingEnvironment LightEnvironmentDefinition::getLightingEnvironment() const
{
	SHLightingEnvironment environment;

	if ((int)m_shCoefficients.size() >= k_shCoefficientFloatCount)
	{
		for (int i= 0; i < k_shCoefficientCount; ++i)
		{
			environment.coefficients[i]=
				glm::vec3(m_shCoefficients[(size_t)i * 3 + 0], m_shCoefficients[(size_t)i * 3 + 1],
						  m_shCoefficients[(size_t)i * 3 + 2]);
		}
	}

	return environment;
}

void LightEnvironmentDefinition::setLightingEnvironment(const SHLightingEnvironment& environment)
{
	m_shCoefficients.assign((size_t)k_shCoefficientFloatCount, 0.f);
	for (int i= 0; i < k_shCoefficientCount; ++i)
	{
		m_shCoefficients[(size_t)i * 3 + 0]= environment.coefficients[i].r;
		m_shCoefficients[(size_t)i * 3 + 1]= environment.coefficients[i].g;
		m_shCoefficients[(size_t)i * 3 + 2]= environment.coefficients[i].b;
	}

	m_directionality= environment.getDirectionality();

	const glm::vec3 keyDirection= environment.getDominantDirection();
	m_keyLightDirection= {keyDirection.x, keyDirection.y, keyDirection.z};

	// Members are assigned directly above rather than through the individual
	// setters so this lands as ONE change set. Going through the setters would
	// re-arm the auto-save and emit a client event three times for what is a
	// single logical update.
	notifyPropertyChanged(ConfigPropertyChangeSet()
							  .addPropertyName(k_shCoefficientsPropertyId)
							  .addPropertyName(k_directionalityPropertyId)
							  .addPropertyName(k_keyLightDirectionPropertyId));
}

configuru::Config LightEnvironmentDefinition::writeToJSON()
{
	configuru::Config pt= TransformComponentDefinition::writeToJSON();

	std::vector<configuru::Config> coefficients;
	coefficients.reserve(m_shCoefficients.size());
	for (float value : m_shCoefficients)
		coefficients.push_back(configuru::Config(value));
	pt[k_shCoefficientsPropertyId]= coefficients;

	pt[k_exposureScalePropertyId]= m_exposureScale;
	pt[k_directionalityPropertyId]= m_directionality;
	pt["key_light_direction_x"]= m_keyLightDirection.x;
	pt["key_light_direction_y"]= m_keyLightDirection.y;
	pt["key_light_direction_z"]= m_keyLightDirection.z;

	return pt;
}

void LightEnvironmentDefinition::readFromJSON(const configuru::Config& pt)
{
	TransformComponentDefinition::readFromJSON(pt);

	m_shCoefficients.assign((size_t)k_shCoefficientFloatCount, 0.f);
	if (pt.has_key(k_shCoefficientsPropertyId))
	{
		const configuru::Config& coefficients= pt[k_shCoefficientsPropertyId];
		if (coefficients.is_array())
		{
			int index= 0;
			for (const configuru::Config& value : coefficients.as_array())
			{
				if (index >= k_shCoefficientFloatCount)
					break;
				m_shCoefficients[(size_t)index++]= (float)value.as_float();
			}
		}
	}

	m_exposureScale= pt.get_or<float>(k_exposureScalePropertyId, 1.f);
	m_directionality= pt.get_or<float>(k_directionalityPropertyId, 0.f);
	m_keyLightDirection.x= pt.get_or<float>("key_light_direction_x", 0.f);
	m_keyLightDirection.y= pt.get_or<float>("key_light_direction_y", 0.f);
	m_keyLightDirection.z= pt.get_or<float>("key_light_direction_z", 1.f);
}

bool LightEnvironmentDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													const Serialization::PolymorphicObjectPtr& initParams)
{
	return TransformComponentDefinition::readFromInitParams(ownerObjectSystem, initParams);
}

// -- LightEnvironmentComponent -----
// Radius of the environment sphere drawn in the editor viewport, in meters.
static constexpr float k_environmentSphereRadius= 10.0f;

LightEnvironmentComponent::LightEnvironmentComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

void LightEnvironmentComponent::init()
{
	TransformComponent::init();

	rebuildEnvironmentSphereMesh();
}

void LightEnvironmentComponent::dispose()
{
	m_environmentSphereMesh= nullptr;

	TransformComponent::dispose();
}

void LightEnvironmentComponent::rebuildEnvironmentSphereMesh()
{
	m_environmentSphereMesh= nullptr;

	IEditorWindow* ownerWindow= getOwnerEditorWindow();
	if (!ownerWindow)
		return;

	IMkGraphicsContextPtr graphicsContext= ownerWindow->getGraphicsContext();
	if (!graphicsContext)
		return;

	// Position-only unit sphere. The shader treats the object-space position as
	// the direction to evaluate the environment along, so no normals or UVs are
	// needed - the position carries both roles.
	constexpr int k_stackCount= 24;
	constexpr int k_sectorCount= 48;
	constexpr float k_pi= 3.14159265f;

	struct PosVert
	{
		float x, y, z;
	};

	std::vector<PosVert> vertices;
	vertices.reserve((k_stackCount + 1) * (k_sectorCount + 1));
	for (int stack= 0; stack <= k_stackCount; ++stack)
	{
		const float polar= k_pi * (float)stack / (float)k_stackCount; // 0 at +Y pole
		const float y= cosf(polar);
		const float ringRadius= sinf(polar);

		for (int sector= 0; sector <= k_sectorCount; ++sector)
		{
			const float azimuth= 2.f * k_pi * (float)sector / (float)k_sectorCount;
			vertices.push_back({ringRadius * cosf(azimuth), y, ringRadius * sinf(azimuth)});
		}
	}

	std::vector<uint32_t> indices;
	indices.reserve(k_stackCount * k_sectorCount * 6);
	for (int stack= 0; stack < k_stackCount; ++stack)
	{
		for (int sector= 0; sector < k_sectorCount; ++sector)
		{
			const uint32_t topLeft= (uint32_t)(stack * (k_sectorCount + 1) + sector);
			const uint32_t bottomLeft= topLeft + (uint32_t)(k_sectorCount + 1);

			// The quads touching a pole collapse to a single triangle, so skip
			// the degenerate half rather than emitting zero-area triangles.
			if (stack != 0)
			{
				indices.push_back(topLeft);
				indices.push_back(bottomLeft);
				indices.push_back(topLeft + 1);
			}
			if (stack != k_stackCount - 1)
			{
				indices.push_back(topLeft + 1);
				indices.push_back(bottomLeft);
				indices.push_back(bottomLeft + 1);
			}
		}
	}

	m_environmentSphereMesh= createMkTriangulatedMesh(
		graphicsContext.get(), "lightEnvironmentSphere", reinterpret_cast<const uint8_t*>(vertices.data()),
		sizeof(PosVert), (uint32_t)vertices.size(), reinterpret_cast<const uint8_t*>(indices.data()), sizeof(uint32_t),
		(uint32_t)(indices.size() / 3),
		false); // data is uploaded to the GPU by createResources(), so no CPU copy is kept

	if (m_environmentSphereMesh)
	{
		MkMaterialConstPtr material=
			graphicsContext->getShaderCache()->getMaterialByName(INTERNAL_MATERIAL_P_SH_ENVIRONMENT);
		m_environmentSphereMesh->setMaterial(material);
		m_environmentSphereMesh->createResources();
	}
}

void LightEnvironmentComponent::customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const
{
	if (!m_environmentSphereMesh)
		return;

	// Pushed every frame rather than tracked against property changes: nine
	// uniform writes are cheaper than the bookkeeping, and it cannot go stale.
	const SHLightingEnvironment environment= getScaledLightingEnvironment();
	MkMaterialInstancePtr materialInstance= m_environmentSphereMesh->getMaterialInstance();
	for (int coefficientIndex= 0; coefficientIndex < k_shCoefficientCount; ++coefficientIndex)
	{
		const eUniformSemantic semantic= (eUniformSemantic)((int)eUniformSemantic::shCoefficient0 + coefficientIndex);
		materialInstance->setVec3BySemantic(semantic, environment.coefficients[coefficientIndex]);
	}

	// The shader evaluates the environment along the object-space position, so
	// object space has to be the space the coefficients are in. That is the owner
	// stage's space, which means the sphere carries the stage's world rotation and
	// nothing else: the probe's own orientation would silently rotate the
	// environment with it, and means nothing here anyway.
	StageComponentConstPtr ownerStage= getOwnerStageComponent();
	const glm::mat4 stageToWorld= ownerStage ? ownerStage->getWorldTransform() : glm::mat4(1.f);

	const glm::vec3 position= glm::vec3(getWorldTransform()[3]);
	glm::mat4 sphereXform(1.f);
	sphereXform[0]= glm::vec4(glm::normalize(glm::vec3(stageToWorld[0])) * k_environmentSphereRadius, 0.f);
	sphereXform[1]= glm::vec4(glm::normalize(glm::vec3(stageToWorld[1])) * k_environmentSphereRadius, 0.f);
	sphereXform[2]= glm::vec4(glm::normalize(glm::vec3(stageToWorld[2])) * k_environmentSphereRadius, 0.f);
	sphereXform[3]= glm::vec4(position, 1.f);

	MkStateStack& stateStack= graphicsContext->getMkStateStack();
	IMkState* sphereState= stateStack.pushState("lightEnvironmentSphere");
	// Visible from outside as a probe ball, and from inside if the radius is
	// raised to enclose the scene.
	sphereState->disableFlag(eMkStateFlagType::cullFace);

	drawTransformedTriangulatedMesh(viewportCamera, sphereXform, m_environmentSphereMesh);

	stateStack.popState();
}

SHLightingEnvironment LightEnvironmentComponent::getScaledLightingEnvironment() const
{
	LightEnvironmentDefinitionPtr definition= getLightEnvironmentDefinition();
	if (!definition)
		return SHLightingEnvironment();

	SHLightingEnvironment environment= definition->getLightingEnvironment();
	const float scale= definition->getExposureScale();
	for (int i= 0; i < k_shCoefficientCount; ++i)
		environment.coefficients[i]*= scale;

	return environment;
}

// -- TransformComponent ----
bool LightEnvironmentComponent::canAttachToParent(TransformComponentConstPtr newParentComponent) const
{
	// The probe belongs to the camera whose frame recovered it, not to the stage
	// its readings are expressed in
	return std::dynamic_pointer_cast<const CameraComponent>(newParentComponent) != nullptr;
}

/// Get the camera component that owns this light environment
CameraComponentConstPtr LightEnvironmentComponent::getOwnerCameraComponent() const
{
	MikanTransformID transformId= getLightEnvironmentDefinition()->getParentTransformId();

	return getObjectSystemOfType<CameraObjectSystem>()->getCameraById(transformId);
}

StageComponentConstPtr LightEnvironmentComponent::getOwnerStageComponent() const
{
	CameraComponentConstPtr ownerCamera= getOwnerCameraComponent();

	return (ownerCamera) ? ownerCamera->getOwnerStageComponent() : nullptr;
}

Serialization::StructTypeHandle LightEnvironmentComponent::getClientAPIValuesStructType() const
{
	return &MikanLightEnvironmentComponentValues::staticGetArchetype();
}

void LightEnvironmentComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	// Everything the capture tool recovers is read only: these are solved
	// outputs, and hand-editing them would silently desync the coefficients
	// from the directionality and key direction derived off them.
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 LightEnvironmentDefinition::k_shCoefficientsPropertyId, MikanVariantType::FLOAT_ARRAY)
								 ->setReadOnly());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 LightEnvironmentDefinition::k_directionalityPropertyId, MikanVariantType::FLOAT)
								 ->setReadOnly());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 LightEnvironmentDefinition::k_keyLightDirectionPropertyId, MikanVariantType::VECTOR3F)
								 ->setReadOnly());

	// The exposure scale is the exception: it is a manual calibration input,
	// not a solved output - the decomposition recovers shading only up to a
	// global scale - so it stays writable.
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(LightEnvironmentDefinition::k_exposureScalePropertyId,
																  MikanVariantType::FLOAT));
}

bool LightEnvironmentComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	LightEnvironmentDefinitionPtr definition= getLightEnvironmentDefinition();

	if (propertyName == LightEnvironmentDefinition::k_shCoefficientsPropertyId)
	{
		if (definition)
		{
			Serialization::List<float> coefficients;
			for (float value : definition->getSHCoefficients())
				coefficients.push_back(value);
			outValue= coefficients;
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_exposureScalePropertyId)
	{
		if (definition)
		{
			outValue= definition->getExposureScale();
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_directionalityPropertyId)
	{
		if (definition)
		{
			outValue= definition->getDirectionality();
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_keyLightDirectionPropertyId)
	{
		if (definition)
		{
			outValue= definition->getKeyLightDirection();
			return true;
		}
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}

bool LightEnvironmentComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	LightEnvironmentDefinitionPtr definition= getLightEnvironmentDefinition();

	if (propertyName == LightEnvironmentDefinition::k_shCoefficientsPropertyId)
	{
		if (definition)
		{
			const Serialization::List<float>& source= inValue.getFloatArrayValue();
			std::vector<float> coefficients;
			coefficients.reserve(source.size());
			for (size_t i= 0; i < source.size(); ++i)
				coefficients.push_back(source[i]);
			definition->setSHCoefficients(coefficients);
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_exposureScalePropertyId)
	{
		if (definition)
		{
			definition->setExposureScale(inValue.getFloatValue());
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_directionalityPropertyId)
	{
		if (definition)
		{
			definition->setDirectionality(inValue.getFloatValue());
			return true;
		}
	}
	else if (propertyName == LightEnvironmentDefinition::k_keyLightDirectionPropertyId)
	{
		if (definition)
		{
			definition->setKeyLightDirection(inValue.getVector3fValue());
			return true;
		}
	}

	return TransformComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string LightEnvironmentComponent::k_captureSceneLightingFunctionId= "capture_scene_lighting";

void LightEnvironmentComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_captureSceneLightingFunctionId, "Capture Scene Lighting"));
}

bool LightEnvironmentComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == LightEnvironmentComponent::k_captureSceneLightingFunctionId)
	{
		captureSceneLighting();
		return true;
	}

	return TransformComponent::invokeFunction(functionName);
}

void LightEnvironmentComponent::captureSceneLighting()
{
	// The estimate is only meaningful in world space if the frame it was
	// captured from has a known camera pose, so the camera is chosen up front
	// rather than guessed at.
	auto parentCamera= std::dynamic_pointer_cast<CameraComponent>(getParentTransformComponent());
	if (parentCamera != nullptr)
	{
		auto* captureStage= getOwnerEditorWindow()->pushAppStageOfType<AppStage_SceneLightingCapture>();
		if (captureStage != nullptr)
		{
			captureStage->setSourceCamera(parentCamera);
			captureStage->setTargetProbe(getSelfPtr<LightEnvironmentComponent>());
		}
		return;
	}
	else
	{
		MIKAN_LOG_WARNING("LightEnvironmentComponent::captureSceneLighting")
			<< "Called on a probe that is not parented to a camera. "
			<< "The scene lighting capture tool requires a camera to be selected first.";
	}
}
