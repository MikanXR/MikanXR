#include "VRTrackingVolumeComponent.h"
#include "EnumPropertyMetaData.h"
#include "CameraObjectSystem.h"
#include "MarkerObjectSystem.h"
#include "MathGLM.h"
#include "MathTypeConversion.h"
#include "MikanObject.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanTrackingVolumeTypes.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "ModalSelectCamera/ModalDialog_SelectCamera.h"
#include "ProjectConfig.h"
#include "SelectionComponent.h"
#include "StringUtils.h"
#include "TrackingMountObjectSystem.h"
#include "VRTrackingRecenter/AppStage_VRTrackingRecenter.h"

// -- VRTrackingVolumeDefinition -----
const std::string VRTrackingVolumeDefinition::k_trackingRuntimePropertyId= "tracking_runtime";
const std::string VRTrackingVolumeDefinition::k_charucoMountIdPropertyId= "charuco_mount_id";
const std::string VRTrackingVolumeDefinition::k_charucoMountOffsetPropertyId= "charuco_mount_offset_mm";
const std::string VRTrackingVolumeDefinition::k_utilityMarkerIdPropertyId= "utility_marker_id";
const std::string VRTrackingVolumeDefinition::k_trackingMountIdsPropertyId= "tracking_mount_ids";
const std::string VRTrackingVolumeDefinition::k_vrSpaceToStageSpacePropertyId= "vr_space_to_stage_space";

VRTrackingVolumeDefinition::VRTrackingVolumeDefinition()
	: TrackingVolumeDefinition()
	, m_vrSpaceToStageSpace({1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f})
{
}

VRTrackingVolumeDefinition::VRTrackingVolumeDefinition(MikanTrackingVolumeID trackingVolumeId)
	: TrackingVolumeDefinition(trackingVolumeId)
	, m_trackingRuntime(eTrackingRuntime::INVALID)
	, m_charucoMountId(INVALID_MIKAN_ID)
	, m_utilityMarkerId(INVALID_MIKAN_ID)
	, m_charucoMountOffsetMM(
		  {DEFAULT_PUCK_HORIZONTAL_OFFSET_MM, DEFAULT_PUCK_VERTICAL_OFFSET_MM, DEFAULT_PUCK_DEPTH_OFFSET_MM})
	, m_vrSpaceToStageSpace({1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f})
{
}

eTrackingVolumeType VRTrackingVolumeDefinition::getTrackingVolumeType() const { return eTrackingVolumeType::vr; }

configuru::Config VRTrackingVolumeDefinition::writeToJSON()
{
	configuru::Config pt= TrackingVolumeDefinition::writeToJSON();

	pt[k_trackingRuntimePropertyId]= k_trackingRuntimeStrings[(int)m_trackingRuntime];
	pt[k_charucoMountIdPropertyId]= m_charucoMountId;
	pt[k_utilityMarkerIdPropertyId]= m_utilityMarkerId;

	writeVector3f(pt, k_charucoMountOffsetPropertyId.c_str(), m_charucoMountOffsetMM);
	writeStdValueVector(pt, k_trackingMountIdsPropertyId.c_str(), m_trackingMountIDs);
	writeMatrix4f(pt, k_vrSpaceToStageSpacePropertyId.c_str(), m_vrSpaceToStageSpace);

	return pt;
}

void VRTrackingVolumeDefinition::readFromJSON(const configuru::Config& pt)
{
	TrackingVolumeDefinition::readFromJSON(pt);

	const std::string trackingRuntimeString=
		pt.get_or<std::string>(k_trackingRuntimePropertyId.c_str(), k_trackingRuntimeStrings[0]);
	m_trackingRuntime= StringUtils::FindEnumValue<eTrackingRuntime>(trackingRuntimeString, k_trackingRuntimeStrings);
	m_charucoMountId= pt.get_or<MikanTrackingMountID>(k_charucoMountIdPropertyId.c_str(), INVALID_MIKAN_ID);
	m_utilityMarkerId= pt.get_or<MikanMarkerID>(k_utilityMarkerIdPropertyId.c_str(), INVALID_MIKAN_ID);

	readVector3f(pt, k_charucoMountOffsetPropertyId.c_str(), m_charucoMountOffsetMM);
	readStdValueVector(pt, k_trackingMountIdsPropertyId.c_str(), m_trackingMountIDs);
	readMatrix4f(pt, k_vrSpaceToStageSpacePropertyId.c_str(), m_vrSpaceToStageSpace);
}

bool VRTrackingVolumeDefinition::readFromInitParams(MikanObjectSystem* ownerObjectSystem,
													const Serialization::PolymorphicObjectPtr& initParams)
{
	if (!TrackingVolumeDefinition::readFromInitParams(ownerObjectSystem, initParams))
		return false;

	const auto* componentValues= initParams.getTypedPointer<MikanVRTrackingVolumeComponentValues>();
	if (componentValues)
	{
		m_trackingRuntime= (eTrackingRuntime)componentValues->tracking_runtime;
		m_charucoMountId= componentValues->charuco_mount_id;
		m_charucoMountOffsetMM= componentValues->charuco_mount_offset_mm;
		m_utilityMarkerId= componentValues->utility_marker_id;

		// Convert Serialization::List to std::vector
		const auto& mountIdsList= componentValues->tracking_mount_ids;
		m_trackingMountIDs.clear();
		m_trackingMountIDs.reserve(mountIdsList.size());
		for (const auto& mountId : mountIdsList)
		{
			m_trackingMountIDs.push_back(mountId);
		}

		m_vrSpaceToStageSpace= componentValues->vr_space_to_stage_space;
	}

	return true;
}

void VRTrackingVolumeDefinition::setTrackingRuntime(eTrackingRuntime runtime)
{
	if (runtime != m_trackingRuntime)
	{
		m_trackingRuntime= runtime;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_trackingRuntimePropertyId));
	}
}

void VRTrackingVolumeDefinition::setCharucoTrackingMountId(MikanTrackingMountID mountId)
{
	if (mountId != m_charucoMountId)
	{
		m_charucoMountId= mountId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoMountIdPropertyId));
	}
}

void VRTrackingVolumeDefinition::setCharucoMountOffsetMM(const MikanVector3f& offset)
{
	m_charucoMountOffsetMM= offset;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_charucoMountOffsetPropertyId));
}

void VRTrackingVolumeDefinition::setUtilityMarkerId(MikanMarkerID markerId)
{
	if (markerId != m_utilityMarkerId)
	{
		m_utilityMarkerId= markerId;
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_utilityMarkerIdPropertyId));
	}
}

bool VRTrackingVolumeDefinition::addTrackingMountID(MikanTrackingMountID mountId)
{
	auto it= std::find(m_trackingMountIDs.begin(), m_trackingMountIDs.end(), mountId);
	if (it == m_trackingMountIDs.end())
	{
		m_trackingMountIDs.push_back(mountId);
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_trackingMountIdsPropertyId));
		return true;
	}

	return false;
}

bool VRTrackingVolumeDefinition::removeTrackingMountID(MikanTrackingMountID mountId)
{
	auto it= std::find(m_trackingMountIDs.begin(), m_trackingMountIDs.end(), mountId);
	if (it != m_trackingMountIDs.end())
	{
		m_trackingMountIDs.erase(it);
		notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_trackingMountIdsPropertyId));
		return true;
	}

	return false;
}

void VRTrackingVolumeDefinition::setVRSpaceToStageSpace(const MikanMatrix4f& poseOffset)
{
	m_vrSpaceToStageSpace= poseOffset;
	notifyPropertyChanged(ConfigPropertyChangeSet().addPropertyName(k_vrSpaceToStageSpacePropertyId));
}

// -- VRTrackingVolumeComponent -----
VRTrackingVolumeComponent::VRTrackingVolumeComponent(MikanObjectWeakPtr owner)
	: TrackingVolumeComponent(owner)
{
}

void VRTrackingVolumeComponent::init()
{
	TrackingVolumeComponent::init();

	const MikanMatrix4f vrSpaceToStageSpace= getVRTrackingVolumeDefinition()->getVRSpaceToStageSpace();
	m_vrSpaceToStageSpace= GlmTransform(MikanMatrix4f_to_glm_mat4(vrSpaceToStageSpace));
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle VRTrackingVolumeComponent::getClientAPIValuesStructType() const
{
	return &MikanVRTrackingVolumeComponentValues::staticGetArchetype();
}

glm::mat4 VRTrackingVolumeComponent::getVRSpaceToStageSpace() const { return m_vrSpaceToStageSpace.getMat4(); }

void VRTrackingVolumeComponent::setVRSpaceToStageSpace(const glm::mat4& poseOffset)
{
	m_vrSpaceToStageSpace= GlmTransform(poseOffset);
	getVRTrackingVolumeDefinition()->setVRSpaceToStageSpace(glm_mat4_to_MikanMatrix4f(poseOffset));
}

bool VRTrackingVolumeComponent::ownsTrackingMount(MikanTrackingMountID mountId) const
{
	const auto& ownedIds= getVRTrackingVolumeDefinition()->getTrackingMountIDs();

	return std::find(ownedIds.begin(), ownedIds.end(), mountId) != ownedIds.end();
}

VRDevicePoseViewPtr VRTrackingVolumeComponent::makeChArUcoTrackingMountPoseView(eVRDevicePoseSpace space) const
{
	MikanTrackingMountID mountId= getVRTrackingVolumeDefinition()->getCharucoTrackingMountId();
	if (mountId != INVALID_MIKAN_ID)
	{
		const auto trackingMountSystem= getObjectSystemOfType<TrackingMountObjectSystem>();
		assert(trackingMountSystem);

		const auto mountComponent= trackingMountSystem->getTypedComponentById(mountId);
		if (mountComponent)
		{
			return mountComponent->makePoseView(space);
		}
	}

	return VRDevicePoseView::makeInvalidPoseView();
}

// -- IPropertyInterface ----
const std::string VRTrackingVolumeComponent::k_vrDevicePositionOffsetPropertyId= "vr_device_position_offset";
const std::string VRTrackingVolumeComponent::k_vrDeviceRotationOffsetPropertyId= "vr_device_rotation_offset";
const std::string VRTrackingVolumeComponent::k_displayTrackingSpacePropertyId= "display_tracking_space";

// Localization keys, not display text (see EnumPropertyMetaData)
static const std::string k_trackingSpaceStrings[]= {"propertyValues.stage_space", "propertyValues.vr_space"};

void VRTrackingVolumeComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TrackingVolumeComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VRTrackingVolumeDefinition::k_trackingRuntimePropertyId, MikanVariantType::INT)
								 ->setDefaultValue(-1)
								 ->setReadOnly());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VRTrackingVolumeDefinition::k_charucoMountIdPropertyId, MikanVariantType::INT)
								 ->setDefaultValue(-1));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VRTrackingVolumeDefinition::k_charucoMountOffsetPropertyId, MikanVariantType::VECTOR3F)
								 ->setDefaultValue(MikanVector3f(0.f, 0.f, 0.f)));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VRTrackingVolumeDefinition::k_utilityMarkerIdPropertyId, MikanVariantType::INT)
								 ->setDefaultValue(-1));
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(
								 VRTrackingVolumeDefinition::k_trackingMountIdsPropertyId, MikanVariantType::INT_ARRAY)
								 ->setReadOnly()
								 ->setUIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VRTrackingVolumeDefinition::k_vrSpaceToStageSpacePropertyId,
											 MikanVariantType::MATRIX4F)
			->setDefaultValue(
				MikanMatrix4f(1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f)));
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_vrDevicePositionOffsetPropertyId, MikanVariantType::VECTOR3F)
			->setDefaultValue(MikanVector3f(0.f, 0.f, 0.f))
			->setReadOnly()
			->setClientAPIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_vrDeviceRotationOffsetPropertyId, MikanVariantType::VECTOR3F)
			->setDefaultValue(MikanVector3f(0.f, 0.f, 0.f))
			->setReadOnly()
			->setClientAPIHidden());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(k_displayTrackingSpacePropertyId, MikanVariantType::INT)
			->setDefaultValue(0)
			->addMetaData(
				std::make_shared<EnumPropertyMetaData>(k_trackingSpaceStrings, 2, eEnumDisplayStyle::RadioButtons)));
}

bool VRTrackingVolumeComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == VRTrackingVolumeDefinition::k_trackingRuntimePropertyId)
	{
		outValue= (int)getVRTrackingVolumeDefinition()->getTrackingRuntime();
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_charucoMountIdPropertyId)
	{
		outValue= getVRTrackingVolumeDefinition()->getCharucoTrackingMountId();
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_charucoMountOffsetPropertyId)
	{
		outValue= getVRTrackingVolumeDefinition()->getCharucoMountOffsetMM();
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_utilityMarkerIdPropertyId)
	{
		outValue= getVRTrackingVolumeDefinition()->getUtilityMarkerId();
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_trackingMountIdsPropertyId)
	{
		outValue= getVRTrackingVolumeDefinition()->getTrackingMountIDs();
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_vrSpaceToStageSpacePropertyId)
	{
		outValue= getVRTrackingVolumeDefinition()->getVRSpaceToStageSpace();
		return true;
	}
	else if (propertyName == k_vrDevicePositionOffsetPropertyId)
	{
		const glm::vec3 positionOffset= m_vrSpaceToStageSpace.getPosition();
		outValue= glm_vec3_to_MikanVector3f(positionOffset);
		return true;
	}
	else if (propertyName == k_vrDeviceRotationOffsetPropertyId)
	{
		float angles[3]{};
		glm_quat_to_euler_angles(m_vrSpaceToStageSpace.getRotation(), angles[0], angles[1], angles[2]);
		angles[0]*= k_radians_to_degrees;
		angles[1]*= k_radians_to_degrees;
		angles[2]*= k_radians_to_degrees;

		outValue= glm_vec3_to_MikanVector3f(glm::vec3(angles[0], angles[1], angles[2]));
		return true;
	}
	else if (propertyName == k_displayTrackingSpacePropertyId)
	{
		int trackingSpaceInt= (int)m_displayTrackingSpace;
		outValue= trackingSpaceInt;
		return true;
	}

	return TrackingVolumeComponent::getPropertyValue(propertyName, outValue);
}

bool VRTrackingVolumeComponent::setPropertyValue(const std::string& propertyName, const MikanVariant& inValue)
{
	if (propertyName == VRTrackingVolumeDefinition::k_trackingRuntimePropertyId)
	{
		getVRTrackingVolumeDefinition()->setTrackingRuntime((eTrackingRuntime)inValue.getIntValue());
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_charucoMountIdPropertyId)
	{
		getVRTrackingVolumeDefinition()->setCharucoTrackingMountId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_charucoMountOffsetPropertyId)
	{
		MikanVector3f offset= inValue.getVector3fValue();

		getVRTrackingVolumeDefinition()->setCharucoMountOffsetMM({offset.x, offset.y, offset.z});
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_utilityMarkerIdPropertyId)
	{
		getVRTrackingVolumeDefinition()->setUtilityMarkerId(inValue.getIntValue());
		return true;
	}
	else if (propertyName == VRTrackingVolumeDefinition::k_vrSpaceToStageSpacePropertyId)
	{
		MikanMatrix4f poseOffset= inValue.getMatrix4fValue();

		getVRTrackingVolumeDefinition()->setVRSpaceToStageSpace(poseOffset);
		return true;
	}
	else if (propertyName == k_displayTrackingSpacePropertyId)
	{
		m_displayTrackingSpace= (MikanTrackingSpace)inValue.getIntValue();
		return true;
	}

	return TrackingVolumeComponent::setPropertyValue(propertyName, inValue);
}

// -- IFunctionInterface ----
const std::string VRTrackingVolumeComponent::k_alignTrackingVolumeFunctionId= "align_tracking_volume";

void VRTrackingVolumeComponent::getFunctionDescriptors(std::vector<FunctionDescriptorConstPtr>& outDescriptors)
{
	TrackingVolumeComponent::getFunctionDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<FunctionDescriptor>(k_alignTrackingVolumeFunctionId, "Align Tracking Volume"));
}

bool VRTrackingVolumeComponent::invokeFunction(const std::string& functionName)
{
	if (functionName == k_alignTrackingVolumeFunctionId)
	{
		alignTrackingVolume();
		return true;
	}

	return TrackingVolumeComponent::invokeFunction(functionName);
}

void VRTrackingVolumeComponent::alignTrackingVolume()
{
	AppStage* currentAppStage= getOwnerEditorWindow()->getCurrentAppStage();

	ModalDialog_SelectCamera::selectCamera(
		currentAppStage,
		[this, currentAppStage](MikanCameraID cameraId)
		{
			const MikanTrackingVolumeID volumeId= getTrackingVolumeDefinition()->getTrackingVolumeId();
			CameraComponentPtr cameraComponent= getObjectSystemOfType<CameraObjectSystem>()->getCameraById(cameraId);

			AppStage_VRTrackingRecenter::tryEnterAlignmentCalibration(currentAppStage, cameraComponent,
																	  getSelfPtr<VRTrackingVolumeComponent>());
		});
}