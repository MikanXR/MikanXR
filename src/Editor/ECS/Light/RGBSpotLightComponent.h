#pragma once

#include "ColliderQuery.h"
#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "DMXFixtureComponent.h"
#include "LightSystemFwd.h"
#include "MikanLightTypes.h"
#include "MikanRendererFwd.h"

#include <cstdint>
#include <string>
#include <vector>

// ============================================================
// RGBSpotLightDefinition
// ============================================================

class RGBSpotLightDefinition : public DMXFixtureComponentDefinition
{
public:
	RGBSpotLightDefinition();
	RGBSpotLightDefinition(MikanLightID lightId);

	virtual configuru::Config writeToJSON() override;
	virtual void readFromJSON(const configuru::Config& pt) override;
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_coneAngleDegreesPropertyId;
	float getConeAngleDegrees() const { return m_coneAngleDegrees; }
	void setConeAngleDegrees(float deg);

	static const std::string k_coneRangeMetersPropertyId;
	float getConeRangeMeters() const { return m_coneRangeMeters; }
	void setConeRangeMeters(float m);

protected:
	float m_coneAngleDegrees= 30.0f;
	float m_coneRangeMeters= 2.0f;
};

// ============================================================
// RGBSpotLightComponent
// ============================================================

class RGBSpotLightComponent : public DMXFixtureComponent
{
public:
	RGBSpotLightComponent(MikanObjectWeakPtr owner);

	virtual void init() override;
	virtual void dispose() override;
	virtual void customRender(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const override;
	// The additive cone volume, drawn after all opaque geometry: it writes no
	// depth, so anything opaque drawn later would paint over it
	void renderConeVolume(IMkGraphicsContext* graphicsContext, MikanCameraPtr viewportCamera) const;

	void disposeMeshComponents();
	void rebuildMeshComponents();
	void rebuildConeMesh();
	void updateConeColor();

	// Selection event handlers
	void onInteractionRayOverlapEnter(const ColliderRaycastHitResult& hitResult);
	void onInteractionRayOverlapExit(const ColliderRaycastHitResult& hitResult);
	void onInteractionSelected();
	void onInteractionUnselected();
	void onTransformGizmoBound();
	void onTransformGizmoUnbound();

	inline static const std::string k_componentClassName= "RGBSpotLightComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline RGBSpotLightDefinitionPtr getRGBSpotLightDefinition() const
	{
		return std::static_pointer_cast<RGBSpotLightDefinition>(m_definition);
	}

	static const std::string k_redPropertyId;
	uint8_t getRed() const { return m_red; }
	void setRed(uint8_t v);

	static const std::string k_greenPropertyId;
	uint8_t getGreen() const { return m_green; }
	void setGreen(uint8_t v);

	static const std::string k_bluePropertyId;
	uint8_t getBlue() const { return m_blue; }
	void setBlue(uint8_t v);

	void setRGB(uint8_t r, uint8_t g, uint8_t b);

	// -- Channel data --
	virtual void getChannelValues(std::vector<uint8_t>& outValues) const override;
	virtual void setChannelValues(const std::vector<uint8_t>& values) override;
	virtual void sendDMXData() const override;

	// -- IEntityAccessor --
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface --
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface --
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
	{
		DMXFixtureComponent::getFunctionDescriptors(outDescriptors);
	}

	// -- Lua Binding --
	static void bindLuaFunctions(struct lua_State* L);

protected:
	void updateWireframeMeshColor();

protected:
	uint8_t m_red= 0;
	uint8_t m_green= 0;
	uint8_t m_blue= 0;
	SelectionComponentWeakPtr m_selectionComponent;
	std::vector<IMkStaticMeshInstancePtr> m_wireframeMeshes;
	std::vector<TransformComponentPtr> m_meshComponents;
	std::vector<StaticMeshComponentPtr> m_triMeshComponents;
	std::vector<MeshColliderComponentPtr> m_colliderComponents;
	IMkTriangulatedMeshPtr m_coneMesh;
	static constexpr float k_coneAlpha= 0.15f;
	bool m_bIsHovered= false;
	bool m_bIsSelected= false;
	bool m_bIsTransformGizmoBound= false;
};
