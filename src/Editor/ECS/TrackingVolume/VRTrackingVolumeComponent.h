#pragma once

#include "MikanMathTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "TrackingMountComponent.h"
#include "TrackingVolumeComponent.h"
#include "Transform.h"
#include "VRTrackingVolumeComponent.h"

#include <vector>

class VRTrackingVolumeDefinition : public TrackingVolumeDefinition
{
public:
	VRTrackingVolumeDefinition();
	VRTrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId);

	virtual eTrackingVolumeType getTrackingVolumeType() const override;

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
	virtual bool readFromInitParams(MikanObjectSystem* ownerObjectSystem,
									const Serialization::PolymorphicObjectPtr& initParams) override;

	static const std::string k_trackingRuntimePropertyId;
	inline eTrackingRuntime getTrackingRuntime() const { return m_trackingRuntime; }
	void setTrackingRuntime(eTrackingRuntime runtime);

	static const std::string k_charucoMountIdPropertyId;
	inline MikanTrackingMountID getCharucoTrackingMountId() const { return m_charucoMountId; }
	void setCharucoTrackingMountId(MikanTrackingMountID mountId);

	static const std::string k_charucoMountOffsetPropertyId;
	inline MikanVector3f getCharucoMountOffsetMM() const { return m_charucoMountOffsetMM; }
	void setCharucoMountOffsetMM(const MikanVector3f& offset);

	static const std::string k_utilityMarkerIdPropertyId;
	inline MikanMarkerID getUtilityMarkerId() const { return m_utilityMarkerId; }
	void setUtilityMarkerId(MikanMarkerID markerId);

	static const std::string k_trackingMountIdsPropertyId;
	inline const std::vector<MikanTrackingMountID>& getTrackingMountIDs() const { return m_trackingMountIDs; }
	bool addTrackingMountID(MikanTrackingMountID mountId);
	bool removeTrackingMountID(MikanTrackingMountID mountId);

	static const std::string k_vrSpaceToStageSpacePropertyId;
	MikanMatrix4f getVRSpaceToStageSpace() const { return m_vrSpaceToStageSpace; }
	void setVRSpaceToStageSpace(const MikanMatrix4f& poseOffset);

private:
	eTrackingRuntime m_trackingRuntime= eTrackingRuntime::INVALID;
	MikanTrackingMountID m_charucoMountId;
	MikanVector3f m_charucoMountOffsetMM;
	MikanMarkerID m_utilityMarkerId;
	std::vector<MikanTrackingMountID> m_trackingMountIDs;
	MikanMatrix4f m_vrSpaceToStageSpace;
};

class VRTrackingVolumeComponent : public TrackingVolumeComponent
{
public:
	VRTrackingVolumeComponent(MikanObjectWeakPtr owner);

	virtual void init() override;

	inline VRTrackingVolumeDefinitionPtr getVRTrackingVolumeDefinition() const
	{
		return std::static_pointer_cast<VRTrackingVolumeDefinition>(m_definition);
	}

	inline static const std::string k_componentClassName= "VRTrackingVolumeComponent";
	virtual std::string getComponentClassName() const override { return k_componentClassName; }

	MikanTrackingSpace getDisplayTrackingSpace() const { return m_displayTrackingSpace; }

	glm::mat4 getVRSpaceToStageSpace() const;
	void setVRSpaceToStageSpace(const glm::mat4& poseOffset);

	bool ownsTrackingMount(MikanTrackingMountID mountId) const;
	VRDevicePoseViewPtr makeChArUcoTrackingMountPoseView(eVRDevicePoseSpace space) const;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_vrDevicePositionOffsetPropertyId;
	static const std::string k_vrDeviceRotationOffsetPropertyId;
	static const std::string k_displayTrackingSpacePropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;
	virtual bool setPropertyValue(const std::string& propertyName, const MikanVariant& inValue) override;

	// -- IFunctionInterface ----
	static const std::string k_alignTrackingVolumeFunctionId;
	static void getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outPropertyNames);
	virtual bool invokeFunction(const std::string& functionName) override;

protected:
	void alignTrackingVolume();

private:
	GlmTransform m_vrSpaceToStageSpace;
	MikanTrackingSpace m_displayTrackingSpace= MikanTrackingSpace_Stage;
};