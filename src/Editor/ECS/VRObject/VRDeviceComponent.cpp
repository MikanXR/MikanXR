#include "App.h"
#include "Colors.h"
#include "IMkStaticMeshInstance.h"
#include "IMkTriangulatedMesh.h"
#include "IMkWireframeMesh.h"
#include "IVRDevice.h"
#include "MainWindow.h"
#include "MathGLM.h"
#include "MathTypeConversion.h"
#include "MeshColliderComponent.h"
#include "MkMaterialInstance.h"
#include "MikanLineRenderer.h"
#include "MikanObject.h"
#include "MikanTextRenderer.h"
#include "MikanVRDeviceTypes.h"
#include "MulticastDelegate.h"
#include "ProjectConfig.h"
#include "StageComponent.h"
#include "SelectionComponent.h"
#include "StageObjectSystem.h"
#include "StaticMeshComponent.h"
#include "StringUtils.h"
#include "TransformComponent.h"
#include "VRDeviceComponent.h"
#include "EditorObjectSystem.h"
#include "VRObjectSystem.h"

// -- VRDeviceConfig -----
const std::string VRDeviceDefinition::k_trackingRuntimeTypePropertyId= "vr_device_api";
const std::string VRDeviceDefinition::k_vrDeviceIndexTypePropertyId= "vr_device_index";
const std::string VRDeviceDefinition::k_vrDeviceTypePropertyId= "vr_device_type";
const std::string VRDeviceDefinition::k_vrDevicePathTypePropertyId= "vr_device_path";

VRDeviceDefinition::VRDeviceDefinition(MikanVRDeviceID vrDeviceId)
	: TransformComponentDefinition(vrDeviceId)
	, m_trackingRuntime(eTrackingRuntime::INVALID)
	, m_vrDeviceIndex(0)
	, m_vrDevicePath("")
{
}

void VRDeviceDefinition::setTrackingRuntimeType(eTrackingRuntime trackingRuntime)
{
	m_trackingRuntime= trackingRuntime;
}

void VRDeviceDefinition::setVRDeviceIndex(size_t vrDeviceIndex) { m_vrDeviceIndex= vrDeviceIndex; }

void VRDeviceDefinition::setVRDeviceType(eVRDeviceType vrDeviceType) { m_vrDeviceType= vrDeviceType; }

void VRDeviceDefinition::setVRDevicePath(const std::string& vrDevicePath) { m_vrDevicePath= vrDevicePath; }

// -- VRDeviceComponent -----
VRDeviceComponent::VRDeviceComponent(MikanObjectWeakPtr owner)
	: TransformComponent(owner)
{
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle VRDeviceComponent::getClientAPIValuesStructType() const
{
	return &MikanVRDeviceComponentValues::staticGetArchetype();
}

void VRDeviceComponent::init()
{
	TransformComponent::init();

	// Listen to selection events so update editor selection display
	SelectionComponentPtr selectionComponentPtr= getOwnerObject()->getComponentOfType<SelectionComponent>();
	if (selectionComponentPtr)
	{
		// Bind selection events
		selectionComponentPtr->OnInteractionRayOverlapEnter+=
			MakeDelegate(this, &VRDeviceComponent::onInteractionRayOverlapEnter);
		selectionComponentPtr->OnInteractionRayOverlapExit+=
			MakeDelegate(this, &VRDeviceComponent::onInteractionRayOverlapExit);
		selectionComponentPtr->OnInteractionSelected+= MakeDelegate(this, &VRDeviceComponent::onInteractionSelected);
		selectionComponentPtr->OnInteractionUnselected+=
			MakeDelegate(this, &VRDeviceComponent::onInteractionUnselected);

		// Remember the selection component
		m_selectionComponentWeakPtr= selectionComponentPtr;
	}

	// Push our world transformToTargetSpace to all child scene components
	propogateWorldTransformChange(eTransformChangeType::recomputeWorldTransformAndPropogate);
}

void VRDeviceComponent::setVRDeviceInterface(IVRDevice* vrDeviceInterface)
{
	if (m_vrDeviceInterface != vrDeviceInterface)
	{
		m_vrDeviceInterface= vrDeviceInterface;

		rebuildSockets();
		rebuildMeshComponents();
	}
}

bool VRDeviceComponent::getDevicePose(const int vrFrameDelay, VRDevicePose& outPose) const
{
	if (m_vrDeviceInterface != nullptr)
	{
		return m_vrDeviceInterface->getDevicePose(vrFrameDelay, outPose);
	}

	return false;
}

void VRDeviceComponent::disposeSockets()
{
	// Clean up any previously created sockets
	for (auto& kvpair : m_socketMap)
	{
		TransformComponentPtr socketPtr= kvpair.second;
		socketPtr->dispose();
	}

	// Forget about the socket components
	m_socketMap.clear();
	m_socketNames.clear();
}

void VRDeviceComponent::rebuildSockets()
{
	MikanObjectPtr vrDeviceObject= getOwnerObject();
	VRDeviceComponentPtr vrDeviceComponentPtr= getSelfPtr<VRDeviceComponent>();

	// Clean up any previously created socket components
	disposeSockets();

	// Create sockets
	if (m_vrDeviceInterface != nullptr)
	{
		for (int socketIndex= 0; socketIndex < m_vrDeviceInterface->getSocketCount(); socketIndex++)
		{
			IVRDeviceSocket* vrDeviceSocket= m_vrDeviceInterface->geSocketByIndex(socketIndex);
			const std::string socketName= vrDeviceSocket->getName();

			// Create a static mesh component to hold the mesh instance
			TransformComponentPtr socketComponentPtr= vrDeviceObject->addComponent<TransformComponent>();
			socketComponentPtr->setName(socketName);
			socketComponentPtr->attachToComponent(vrDeviceComponentPtr);
			m_socketMap.insert({socketName, socketComponentPtr});

			// Also keep track of the socket names
			m_socketNames.push_back(socketName);
		}

		// Initialize all of the newly created sockets
		for (auto& kvpair : m_socketMap)
		{
			kvpair.second->init();
		}

		// Post-Initialize all of the newly created sockets
		for (auto& kvpair : m_socketMap)
		{
			kvpair.second->postInit();
		}
	}
}

bool VRDeviceComponent::getSocketRelativePoseByName(const std::string& socketName, glm::mat4& outPose) const
{
	glm::mat4 vrTrackingSpacePose= glm::mat4(1.f);

	if (m_vrDeviceInterface != nullptr)
	{
		IVRDeviceSocket* socket= m_vrDeviceInterface->getSocketByName(socketName.c_str());

		VRDevicePose socketRelativePose;
		if (socket != nullptr && socket->getSocketState(socketRelativePose))
		{
			outPose= VRDevicePose_to_GlmTransform(socketRelativePose).getMat4();

			return true;
		}
	}

	return false;
}

VRDevicePoseViewPtr VRDeviceComponent::makePoseView(eVRDevicePoseSpace space, const std::string& socketName) const
{
	return std::make_shared<VRDevicePoseView>(this, space, socketName);
}

void VRDeviceComponent::disposeMeshComponents()
{
	// Clean up any previously created mesh components
	for (auto& kvpair : m_meshComponentMap)
	{
		VRDeviceMeshInfo& meshInfo= kvpair.second;

		meshInfo.colliderComponent->dispose();
		meshInfo.triStaticMeshComponent->dispose();
		meshInfo.wireStaticMeshComponent->dispose();
	}

	// Forget about any collider components
	m_meshComponentMap.clear();
}

void VRDeviceComponent::rebuildMeshComponents()
{
	MikanObjectPtr vrDeviceObject= getOwnerObject();
	VRDeviceComponentPtr vrDeviceComponentPtr= getSelfPtr<VRDeviceComponent>();

	// Clean up any previously created mesh components
	disposeMeshComponents();

	// If a model loaded, create meshes and colliders for it
	if (m_vrDeviceInterface != nullptr)
	{
		for (int meshIndex= 0; meshIndex < m_vrDeviceInterface->getMeshCount(); meshIndex++)
		{
			IVRDeviceMesh* vrDeviceMesh= m_vrDeviceInterface->getMeshByIndex(meshIndex);
			const std::string meshName= vrDeviceMesh->getName();

			// Fetch the mesh and material resources
			IMkTriangulatedMeshConstPtr triMeshPtr= vrDeviceMesh->getTriangulatedMesh();
			IMkWireframeMeshConstPtr wireframeMeshPtr= vrDeviceMesh->getWireframeMesh();

			// Create a new static mesh instance from the mesh resources
			IMkStaticMeshInstancePtr triMeshInstancePtr= createMkStaticMeshInstance(triMeshPtr->getName(), triMeshPtr);
			triMeshInstancePtr->setVisible(true);

			// Create a new (hidden) static mesh instance from the mesh resources
			IMkStaticMeshInstancePtr wireframeMeshInstancePtr=
				createMkStaticMeshInstance("wireframe", wireframeMeshPtr);
			wireframeMeshInstancePtr->setVisible(false);

			// Create a static mesh component to hold the mesh instance
			StaticMeshComponentPtr triMeshComponentPtr= vrDeviceObject->addComponent<StaticMeshComponent>();
			triMeshComponentPtr->setName(triMeshPtr->getName());
			triMeshComponentPtr->setStaticMesh(triMeshInstancePtr);
			triMeshComponentPtr->attachToComponent(vrDeviceComponentPtr);

			// Create a static mesh component to hold the mesh instance
			StaticMeshComponentPtr wireMeshComponentPtr= vrDeviceObject->addComponent<StaticMeshComponent>();
			wireMeshComponentPtr->setName(wireframeMeshPtr->getName());
			wireMeshComponentPtr->setStaticMesh(wireframeMeshInstancePtr);
			wireMeshComponentPtr->attachToComponent(vrDeviceComponentPtr);

			// Add a mesh collider component that generates collision from the mesh data
			MeshColliderComponentPtr colliderPtr= vrDeviceObject->addComponent<MeshColliderComponent>();
			colliderPtr->setName(triMeshPtr->getName());
			colliderPtr->setStaticMeshComponent(triMeshComponentPtr);
			colliderPtr->attachToComponent(vrDeviceComponentPtr);

			VRDeviceMeshInfo meshInfo;
			meshInfo.triStaticMeshComponent= triMeshComponentPtr;
			meshInfo.wireStaticMeshComponent= wireMeshComponentPtr;
			meshInfo.colliderComponent= colliderPtr;

			m_meshComponentMap.insert({meshName, meshInfo});
		}

		// Update colors of all attached wireframe meshes
		updateWireframeMeshDisplay();

		// Initialize all of the newly created components
		for (auto& kvpair : m_meshComponentMap)
		{
			VRDeviceMeshInfo& meshInfo= kvpair.second;

			meshInfo.triStaticMeshComponent->init();
			meshInfo.wireStaticMeshComponent->init();
			meshInfo.colliderComponent->init();
		}

		// PostInitialize all of the newly created components
		for (auto& kvpair : m_meshComponentMap)
		{
			VRDeviceMeshInfo& meshInfo= kvpair.second;

			meshInfo.triStaticMeshComponent->postInit();
			meshInfo.wireStaticMeshComponent->postInit();
			meshInfo.colliderComponent->postInit();
		}
	}

	// Refresh the child collider list on the selection component
	SelectionComponentPtr selectionComponentPtr= m_selectionComponentWeakPtr.lock();
	if (selectionComponentPtr)
	{
		selectionComponentPtr->rebindColliders();
	}
}

void VRDeviceComponent::refreshDevicePose()
{
	const int vrFrameDelay= 0; // Use the latest pose for rendering
	VRDevicePose vrDevicePose;
	if (m_vrDeviceInterface != nullptr && m_vrDeviceInterface->getDevicePose(vrFrameDelay, vrDevicePose))
	{
		// Set the parent device transformToTargetSpace
		setRelativeTransform(VRDevicePose_to_GlmTransform(vrDevicePose));

		// Update the child render mesh component relative transforms
		for (size_t meshIndex= 0; meshIndex < m_vrDeviceInterface->getMeshCount(); meshIndex++)
		{
			const IVRDeviceMesh* deviceMesh= m_vrDeviceInterface->getMeshByIndex(meshIndex);
			const std::string meshName= deviceMesh->getName();

			auto it= m_meshComponentMap.find(meshName);
			if (it != m_meshComponentMap.end())
			{
				const VRDeviceMeshInfo& meshInfo= it->second;

				VRDevicePose vrMeshPose;
				bool bIsVisible= false;
				if (deviceMesh->getMeshState(vrMeshPose, bIsVisible))
				{
					const GlmTransform vrMeshTransform= VRDevicePose_to_GlmTransform(vrMeshPose);

					meshInfo.triStaticMeshComponent->setRelativeTransform(vrMeshTransform);
					meshInfo.wireStaticMeshComponent->setRelativeTransform(vrMeshTransform);
					meshInfo.colliderComponent->setRelativeTransform(vrMeshTransform);

					meshInfo.triStaticMeshComponent->getStaticMesh()->setVisible(bIsVisible);
					if (!bIsVisible)
					{
						meshInfo.wireStaticMeshComponent->getStaticMesh()->setVisible(false);
					}
				}
			}
		}

		// Update the child attachment component relative transforms
		for (size_t attachmentIndex= 0; attachmentIndex < m_vrDeviceInterface->getSocketCount(); attachmentIndex++)
		{
			const IVRDeviceSocket* attachment= m_vrDeviceInterface->geSocketByIndex(attachmentIndex);
			const std::string attachmentName= attachment->getName();

			auto it= m_socketMap.find(attachmentName);
			if (it != m_socketMap.end())
			{
				TransformComponentPtr attachmentComponent= it->second;

				VRDevicePose attachmentPose;
				if (attachment->getSocketState(attachmentPose))
				{
					const GlmTransform attachmentTransform= VRDevicePose_to_GlmTransform(attachmentPose);

					attachmentComponent->setRelativeTransform(attachmentTransform);
				}
			}
		}
	}
}

void VRDeviceComponent::renderVRDeviceInfo(IMkGraphicsContext* graphicsContext, IMkCameraConstPtr camera,
										   const glm::mat4& transformToTargetSpace) const
{
	TextStyle style= getDefaultTextStyle();

	const IVRDevice* vrDevice= getVRDeviceInterface();
	wchar_t wszVRDeviceName[256];
	StringUtils::convertMbsToWcs(vrDevice->getDevicePath(), wszVRDeviceName, sizeof(wszVRDeviceName));

	glm::vec3 xColor= Colors::DarkRed;
	glm::vec3 yColor= Colors::DarkGreen;
	glm::vec3 zColor= Colors::DarkBlue;
	SelectionComponentPtr selectionComponent= m_selectionComponentWeakPtr.lock();
	if (selectionComponent)
	{
		if (selectionComponent->getIsSelected())
		{
			xColor= Colors::Red;
			yColor= Colors::Green;
			zColor= Colors::Blue;
		}
		else if (selectionComponent->getIsHovered())
		{
			xColor= Colors::LightGreen;
			yColor= Colors::LightGreen;
			zColor= Colors::LightBlue;
		}
	}

	glm::mat4 vrDeviceXform= glm_composite_xform(getRelativeTransform().getMat4(), transformToTargetSpace);
	glm::vec3 vrDevicePos(vrDeviceXform[3]);

	drawTransformedAxes(graphicsContext, vrDeviceXform, 0.1f, 0.1f, 0.1f, xColor, yColor, zColor);
	if (getObjectSystemOfType<EditorObjectSystem>()->getEditorSettings().bRenderComponentNames)
	{
		drawTextAtWorldPosition(graphicsContext, style, vrDevicePos, L"%s", wszVRDeviceName);
	}
}

void VRDeviceComponent::updateWireframeMeshDisplay()
{
	glm::vec3 newColor= Colors::White;
	bool bNewVisibility= false;

	if (m_bIsSelected)
	{
		newColor= Colors::Yellow;
		bNewVisibility= false;
	}
	else if (m_bIsHovered)
	{
		newColor= Colors::LightGray;
		bNewVisibility= false;
	}
	else
	{
		bNewVisibility= false;
	}

	for (auto& kvpair : m_meshComponentMap)
	{
		VRDeviceMeshInfo& meshInfo= kvpair.second;
		IMkStaticMeshInstancePtr meshPtr= meshInfo.wireStaticMeshComponent->getStaticMesh();

		meshPtr->getMaterialInstance()->setVec4BySemantic(eUniformSemantic::diffuseColorRGBA, glm::vec4(newColor, 1.f));
		meshPtr->setVisible(bNewVisibility);
	}
}

// Selection Events
void VRDeviceComponent::onInteractionRayOverlapEnter(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= true;
	updateWireframeMeshDisplay();
}

void VRDeviceComponent::onInteractionRayOverlapExit(const ColliderRaycastHitResult& hitResult)
{
	m_bIsHovered= false;
	updateWireframeMeshDisplay();
}

void VRDeviceComponent::onInteractionSelected()
{
	m_bIsSelected= true;
	updateWireframeMeshDisplay();
}

void VRDeviceComponent::onInteractionUnselected()
{
	m_bIsSelected= false;
	updateWireframeMeshDisplay();
}

// -- IPropertyInterface ----
const std::string VRDeviceComponent::k_socketNameListPropertyId= "socket_names";
void VRDeviceComponent::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	TransformComponent::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VRDeviceDefinition::k_trackingRuntimeTypePropertyId, MikanVariantType::INT)
			->setReadOnly());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VRDeviceDefinition::k_vrDeviceIndexTypePropertyId, MikanVariantType::INT)
			->setReadOnly());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VRDeviceDefinition::k_vrDeviceTypePropertyId, MikanVariantType::INT)
			->setReadOnly());
	outDescriptors.push_back(
		std::make_shared<PropertyDescriptor>(VRDeviceDefinition::k_vrDevicePathTypePropertyId, MikanVariantType::STRING)
			->setReadOnly());
	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(VRDeviceComponent::k_socketNameListPropertyId,
																  MikanVariantType::STRING_ARRAY)
								 ->setReadOnly());
}

bool VRDeviceComponent::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == VRDeviceDefinition::k_trackingRuntimeTypePropertyId)
	{
		outValue= static_cast<int>(getVRDeviceDefinition()->getTrackingRuntimeType());
		return true;
	}
	else if (propertyName == VRDeviceDefinition::k_vrDeviceIndexTypePropertyId)
	{
		outValue= static_cast<int>(getVRDeviceDefinition()->getVRDeviceIndex());
		return true;
	}
	else if (propertyName == VRDeviceDefinition::k_vrDeviceTypePropertyId)
	{
		outValue= static_cast<int>(getVRDeviceDefinition()->getVRDeviceType());
		return true;
	}
	else if (propertyName == VRDeviceDefinition::k_vrDevicePathTypePropertyId)
	{
		outValue= getVRDeviceDefinition()->getVRDevicePath();
		return true;
	}
	else if (propertyName == VRDeviceComponent::k_socketNameListPropertyId)
	{
		outValue.setValue(m_socketNames);
		return true;
	}

	return TransformComponent::getPropertyValue(propertyName, outValue);
}