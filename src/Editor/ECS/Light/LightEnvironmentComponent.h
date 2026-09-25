#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "LightSystemFwd.h"
#include "MikanLightTypes.h"
#include "MikanRendererFwd.h"
#include "SphericalHarmonics.h"
#include "TransformComponent.h"

#include <string>
#include <vector>

// ============================================================
// LightEnvironmentDefinition
// ============================================================

/// Persisted state of a scene lighting probe: the recovered spherical harmonic
/// environment plus the manual exposure calibration.
class LightEnvironmentDefinition : public TransformComponentDefinition
{
public:
	LightEnvironmentDefinition();
	LightEnvironmentDefinition(MikanLightID lightId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	/// 27 floats, laid out (coefficient * 3 + channel). Always this length -
	/// a malformed or absent list is replaced with zeros on load rather than
	/// left short, so downstream indexing is unconditional.
	static const std::string k_shCoefficientsPropertyId;
	const std::vector<float>& getSHCoefficients() const { return m_shCoefficients; }
	void setSHCoefficients(const std::vector<float>& coefficients);

	static const std::string k_exposureScalePropertyId;
	float getExposureScale() const { return m_exposureScale; }
	void setExposureScale(float scale);

	static const std::string k_directionalityPropertyId;
	float getDirectionality() const { return m_directionality; }
	void setDirectionality(float directionality);

	static const std::string k_keyLightDirectionPropertyId;
	const MikanVector3f& getKeyLightDirection() const { return m_keyLightDirection; }
	void setKeyLightDirection(const MikanVector3f& direction);

	/// Convenience conversions between the flat float list and the math type.
	SHLightingEnvironment getLightingEnvironment() const;
	void setLightingEnvironment(const SHLightingEnvironment& environment);

protected:
	std::vector<float> m_shCoefficients;
	float m_exposureScale= 1.f;
	float m_directionality= 0.f;
	MikanVector3f m_keyLightDirection= {0.f, 0.f, 1.f};
};

// ============================================================
// LightEnvironmentComponent
// ============================================================

class LightEnvironmentComponent : public TransformComponent
{
public:
	LightEnvironmentComponent(MikanObjectWeakPtr owner);

	inline static const std::string k_componentClassName= "LightEnvironmentComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	virtual void init() override;
	virtual void dispose() override;

	// -- TransformComponent ----
	virtual bool canAttachToParent(TransformComponentConstPtr newParentComponent) const override;

	/// Draws a sphere shaded with the recovered environment, so the probe can
	/// be seen in the scene rather than only in the capture tool. Visualization
	/// only: it has no collider and lights nothing, the editor twin of the
	/// Unreal skydome.
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;

	inline LightEnvironmentDefinitionPtr getLightEnvironmentDefinition() const
	{
		return std::static_pointer_cast<LightEnvironmentDefinition>(m_definition);
	}

	/// The recovered environment scaled by the exposure calibration. This is
	/// what a renderer should consume.
	SHLightingEnvironment getScaledLightingEnvironment() const;

	/// Get the camera component that owns this light environment
	CameraComponentConstPtr getOwnerCameraComponent() const;

	// Get the stage component that owns this light environment
	StageComponentConstPtr getOwnerStageComponent() const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface --
	/// Opens the scene lighting capture tool against this probe. Exposed as a
	/// component function so it is reachable from the editor UI, from Lua, and
	/// remotely over the client API.
	static const std::string k_captureSceneLightingFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors);
	virtual bool invokeFunction(const std::string& functionName) override;

	void captureSceneLighting();

protected:
	void rebuildEnvironmentSphereMesh();

	/// Position-only unit sphere; the object-space position doubles as the
	/// direction the SH is evaluated along, so it must be drawn unrotated.
	IMkTriangulatedMeshPtr m_environmentSphereMesh;
};
