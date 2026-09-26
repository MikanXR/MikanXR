#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "IVRDevice.h"
#include "VRDevicePoseView.h"
#include "TransformComponent.h"
#include "MikanTypeFwd.h"
#include "MikanVRDeviceTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "ObjectFwd.h"
#include "ProjectConfigConstants.h"
#include "SceneFwd.h"
#include "Transform.h"

#include <map>
#include <memory>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

// -- VRDeviceDefinition -----
class VRDeviceDefinition : public TransformComponentDefinition
{
public:
	VRDeviceDefinition()= default;
	VRDeviceDefinition(MikanVRDeviceID vrDeviceId);

	static const std::string k_trackingRuntimeTypePropertyId;
	eTrackingRuntime getTrackingRuntimeType() const { return m_trackingRuntime; }
	void setTrackingRuntimeType(eTrackingRuntime trackingRuntime);

	static const std::string k_vrDeviceIndexTypePropertyId;
	inline size_t getVRDeviceIndex() const { return m_vrDeviceIndex; }
	void setVRDeviceIndex(size_t vrDeviceIndex);

	static const std::string k_vrDeviceTypePropertyId;
	inline eVRDeviceType getVRDeviceType() const { return m_vrDeviceType; }
	void setVRDeviceType(eVRDeviceType vrDeviceType);

	static const std::string k_vrDevicePathTypePropertyId;
	inline const std::string getVRDevicePath() const { return m_vrDevicePath; }
	void setVRDevicePath(const std::string& vrDevicePath);

	// Don't send property update for transform property updates as they are spammy
	virtual bool isAutoNotifyTransformPropertyChangeDisabled() override { return true; }
	// VRDeviceDefinition is runtime only and isn't saved to the project file
	virtual bool wantsSaveForPropertyChange(const ConfigPropertyChangeSet& changedPropertySet) const { return false; }

private:
	// These properties are set at VRDevice creation and are read-only after that
	eTrackingRuntime m_trackingRuntime= eTrackingRuntime::INVALID;
	size_t m_vrDeviceIndex= 0;
	eVRDeviceType m_vrDeviceType= eVRDeviceType::INVALID;
	std::string m_vrDevicePath;
};

// -- VRDeviceComponent -----
class VRDeviceComponent : public TransformComponent
{
public:
	VRDeviceComponent(MikanObjectWeakPtr owner);
	virtual void init() override;

	inline static const std::string k_componentClassName= "VRDeviceComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	inline VRDeviceDefinitionPtr getVRDeviceDefinition() const
	{
		return std::static_pointer_cast<VRDeviceDefinition>(m_definition);
	}

	void setVRDeviceInterface(class IVRDevice* vrDeviceInterface);
	class IVRDevice* getVRDeviceInterface() const { return m_vrDeviceInterface; }

	bool getDevicePose(const int vrFrameDelay, struct VRDevicePose& outPose) const;

	const std::vector<std::string>& getSocketNames() const { return m_socketNames; }
	bool getSocketRelativePoseByName(const std::string& socketName, glm::mat4& outPose) const;
	VRDevicePoseViewPtr makePoseView(eVRDevicePoseSpace space, const std::string& socketName= "") const;

	void refreshDevicePose();
	void renderVRDeviceInfo(IMkGraphicsContext* graphicsContext, IMkCameraConstPtr camera,
							const glm::mat4& transformToTargetSpace) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_socketNameListPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;

protected:
	void updateWireframeMeshDisplay();

	// Selection Events
	void onInteractionRayOverlapEnter(const struct ColliderRaycastHitResult& hitResult);
	void onInteractionRayOverlapExit(const struct ColliderRaycastHitResult& hitResult);
	void onInteractionSelected();
	void onInteractionUnselected();

	// Socket Component Management
	void disposeSockets();
	void rebuildSockets();

	// Mesh Component Management
	void disposeMeshComponents();
	void rebuildMeshComponents();

protected:
	struct VRDeviceMeshInfo
	{
		StaticMeshComponentPtr wireStaticMeshComponent;
		StaticMeshComponentPtr triStaticMeshComponent;
		MeshColliderComponentPtr colliderComponent;
	};

	class IVRDevice* m_vrDeviceInterface= nullptr;
	SelectionComponentWeakPtr m_selectionComponentWeakPtr;
	std::map<std::string, TransformComponentPtr> m_socketMap;
	std::vector<std::string> m_socketNames;
	std::map<std::string, VRDeviceMeshInfo> m_meshComponentMap;
	bool m_bIsHovered= false;
	bool m_bIsSelected= false;
};