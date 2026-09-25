#include "App.h"
#include "BoxColliderComponent.h"
#include "IMkGraphicsContext.h"
#include "IMkScene.h"
#include "IMkWindowContext.h"
#include "IEditorWindow.h"
#include "IVRDeviceModule.h"
#include "IVRDevice.h"
#include "TransformComponent.h"
#include "Logger.h"
#include "MathTypeConversion.h"
#include "MathGLM.h"
#include "MikanObject.h"
#include "MikanAPITypes.h"
#include "MikanMathTypes.h"
#include "MikanModuleManager.h"
#include "MulticastDelegate.h"
#include "ProjectConfig.h"
#include "ProjectManager.h"
#include "SelectionComponent.h"
#include "StringUtils.h"
#include "VRObjectSystem.h"
#include "VRDeviceComponent.h"

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <objbase.h>
#endif //_WIN32

// -- VRObjectSystemConfig -----
VRObjectSystemDefinition::VRObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

// -- VRTrackingRuntime -----
class VRTrackingRuntime
{
public:
	VRTrackingRuntime()= default;
	VRTrackingRuntime(VRObjectSystem* ownerSystem, eTrackingRuntime inRuntime, IVRDeviceModule* inModule,
					  IVRDeviceManagerPtr inManager)
		: m_ownerSystem(ownerSystem)
		, m_runtime(inRuntime)
		, m_vrDeviceModule(inModule)
		, m_vrDeviceManager(inManager)
	{
		if (m_vrDeviceManager)
		{
			m_vrDeviceManager->addListener(m_ownerSystem);
		}
	}

	virtual ~VRTrackingRuntime()
	{
		if (m_vrDeviceManager)
		{
			m_vrDeviceManager->removeListener(m_ownerSystem);
			m_vrDeviceManager->shutdown();
			m_vrDeviceManager= nullptr;
		}

		if (m_vrDeviceModule)
		{
			getMikanModuleManager()->disposeModule(m_vrDeviceModule);
		}
	}

	eTrackingRuntime getRuntimeType() const { return m_runtime; }
	IVRDeviceModule* getVRDeviceModule() const { return m_vrDeviceModule; }
	IVRDeviceManagerPtr getVRDeviceManager() const { return m_vrDeviceManager; }

	void update(float deltaSeconds)
	{
		if (m_vrDeviceManager)
		{
			m_vrDeviceManager->update(deltaSeconds);
		}
	}

private:
	VRObjectSystem* m_ownerSystem= nullptr;
	eTrackingRuntime m_runtime= eTrackingRuntime::INVALID;
	IVRDeviceModule* m_vrDeviceModule= nullptr;
	IVRDeviceManagerPtr m_vrDeviceManager= nullptr;
};

// -- VRObjectSystem -----
bool VRObjectSystem::init(MikanObjectSystemDefinitionPtr definitionPtr)
{
	if (!Super::init(definitionPtr))
		return false;

	// Listen for project config changes
	ProjectConfigPtr projectConfig= getProjectConfig();
	projectConfig->OnPropertyChanged+= MakeDelegate(this, &VRObjectSystem::onProjectConfigMarkedDirty);
	m_projectConfigWeakPtr= projectConfig;

	VRObjectSystemDefinitionPtr vrSystemConfig= projectConfig->vrObjectConfig;
	vrSystemConfig->OnPropertyChanged+= MakeDelegate(this, &VRObjectSystem::onVRSystemConfigMarkedDirty);

	return true;
}

void VRObjectSystem::update(float deltaSeconds)
{
	// On first update (after all startup), launch any deferred runtime threads
	if (!m_deferredRuntimeLaunches.empty())
		launchDeferredRuntimeThreads();

	// Poll pending async runtime inits
	for (auto it= m_pendingRuntimeFutures.begin(); it != m_pendingRuntimeFutures.end();)
	{
		if (it->second.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			auto result= it->second.get();
			const std::string runtimeName= k_trackingRuntimeStrings[(int)result.runtime];
			if (result.manager)
			{
				m_trackingRuntimeStates[result.runtime]= eTrackingRuntimeState::ready;
				m_trackingRuntimes[result.runtime]=
					std::make_shared<VRTrackingRuntime>(this, result.runtime, result.module, result.manager);

				// Create graphics resources for render models that were
				// previously loaded on the async VR-Runtime init thread
				result.manager->createGraphicsResources();

				onActiveDeviceListChanged(result.manager.get());
				MIKAN_LOG_INFO("VRObjectSystem::update") << "Tracking runtime ready for " << runtimeName;
			}
			else
			{
				m_trackingRuntimeStates[result.runtime]= eTrackingRuntimeState::failed;
				MIKAN_LOG_ERROR("VRObjectSystem::update") << "Async tracking runtime init failed for " << runtimeName;
			}
			it= m_pendingRuntimeFutures.erase(it);
		}
		else
		{
			++it;
		}
	}

	for (auto it= m_trackingRuntimes.begin(); it != m_trackingRuntimes.end(); it++)
	{
		it->second->update(deltaSeconds);
	}
}

void VRObjectSystem::dispose()
{
	ProjectConfigPtr projectConfigPtr= m_projectConfigWeakPtr.lock();
	if (projectConfigPtr)
	{
		projectConfigPtr->OnPropertyChanged-= MakeDelegate(this, &VRObjectSystem::onProjectConfigMarkedDirty);
	}

	// Wait for any in-flight async inits before cleaning up
	for (auto& [runtime, future] : m_pendingRuntimeFutures)
	{
		if (future.valid())
		{
			auto result= future.get();
			if (result.manager)
			{
				// Init completed during disposal — shut it down immediately
				result.manager->shutdown();
				getMikanModuleManager()->disposeModule(result.module);
			}
		}
	}
	m_pendingRuntimeFutures.clear();
	m_trackingRuntimeStates.clear();

	// Clean up all tracking modules
	m_trackingRuntimes.clear();

	// Dispose all MikanObjects (will clear component map)
	Super::dispose();
}

void VRObjectSystem::onProjectConfigMarkedDirty(CommonConfigPtr configPtr,
												const class ConfigPropertyChangeSet& changedPropertySet)
{
}

void VRObjectSystem::onVRSystemConfigMarkedDirty(CommonConfigPtr configPtr,
												 const class ConfigPropertyChangeSet& changedPropertySet)
{
}

bool VRObjectSystem::createTrackingRuntime(eTrackingRuntime desiredRuntime)
{
	if (desiredRuntime == eTrackingRuntime::INVALID)
	{
		MIKAN_LOG_WARNING("VRObjectSystem::createTrackingRuntime") << "Invalid tracking runtime type requested";
		return false;
	}

	switch (getTrackingRuntimeState(desiredRuntime))
	{
	case eTrackingRuntimeState::ready:
		return true;
	case eTrackingRuntimeState::initializing:
		return false;
	case eTrackingRuntimeState::failed:
		return false;
	default:
		break;
	}

	// Already queued for deferred launch
	if (m_deferredRuntimeLaunches.count(desiredRuntime) > 0)
		return false;

	// Defer the thread launch to the first update() call, so that DLL loading
	// doesn't hold the loader lock while mikanServer::startup / ImGui init run.
	const std::string moduleName= StringUtils::stringify("Mikan", k_trackingRuntimeStrings[(int)desiredRuntime]);
	MIKAN_LOG_INFO("VRObjectSystem::createTrackingRuntime")
		<< "Deferring async init for tracking runtime " << moduleName;
	m_deferredRuntimeLaunches.insert(desiredRuntime);

	return false;
}

void VRObjectSystem::launchDeferredRuntimeThreads()
{
	for (eTrackingRuntime desiredRuntime : m_deferredRuntimeLaunches)
	{
		const std::string runtimeName= k_trackingRuntimeStrings[(int)desiredRuntime];
		const std::string moduleName= StringUtils::stringify("Mikan", runtimeName);

		// Capture the graphics context on the main thread before launching the async task
		IEditorWindow* ownerWindow= getOwnerProjectManager()->getOwnerWindow();
		IMkGraphicsContext* graphicsContext= ownerWindow->getGraphicsContext().get();

		MIKAN_LOG_INFO("VRObjectSystem::launchDeferredRuntimeThreads")
			<< "Launching async init for tracking runtime " << moduleName;
		m_trackingRuntimeStates[desiredRuntime]= eTrackingRuntimeState::initializing;
		auto promise= std::make_shared<std::promise<TrackingRuntimeInitResult>>();
		m_pendingRuntimeFutures[desiredRuntime]= promise->get_future();
		std::thread([promise, desiredRuntime, moduleName, graphicsContext]() mutable
					{ promise->set_value(initTrackingRuntimeOnThread(desiredRuntime, moduleName, graphicsContext)); })
			.detach();
	}
	m_deferredRuntimeLaunches.clear();
}

VRObjectSystem::eTrackingRuntimeState VRObjectSystem::getTrackingRuntimeState(eTrackingRuntime runtime) const
{
	auto it= m_trackingRuntimeStates.find(runtime);
	return it != m_trackingRuntimeStates.end() ? it->second : eTrackingRuntimeState::uninitialized;
}

VRObjectSystem::TrackingRuntimeInitResult VRObjectSystem::initTrackingRuntimeOnThread(
	eTrackingRuntime runtime, const std::string& moduleName, IMkGraphicsContext* graphicsContext)
{
#ifdef _WIN32
	// Initialize COM in MTA on this thread so that OpenVR/SteamVR initialization
	// doesn't try to marshal COM calls to the main thread's COM apartment
	const HRESULT comInitResult= CoInitializeEx(NULL, COINIT_MULTITHREADED);
	const bool bComInitialized= (comInitResult == S_OK);
#endif // _WIN32

	TrackingRuntimeInitResult result;
	result.runtime= runtime;

	// Attempt to load the vr device module
	result.module= getMikanModuleManager()->getModule<IVRDeviceModule>(moduleName);
	if (result.module)
	{
		MIKAN_LOG_INFO("VRObjectSystem::initTrackingRuntimeOnThread") << "Loaded module " << moduleName;

		// Attempt to create a vr device manager
		result.manager= result.module->createTrackingRuntime();
		if (result.manager)
		{
			MIKAN_LOG_INFO("VRObjectSystem::initTrackingRuntimeOnThread")
				<< "Allocated TrackingRuntime for " << moduleName;

			// Attempt to startup the vr device manager
			if (result.manager->startup(graphicsContext))
			{
				MIKAN_LOG_INFO("VRObjectSystem::initTrackingRuntimeOnThread")
					<< "Started VRDeviceManger for " << moduleName;

				if (bComInitialized)
					CoUninitialize();
				return result;
			}
			else
			{
				MIKAN_LOG_WARNING("VRObjectSystem::initTrackingRuntimeOnThread")
					<< "Failed to startup VRDeviceManger for " << moduleName;
			}
		}
		else
		{
			MIKAN_LOG_WARNING("VRObjectSystem::initTrackingRuntimeOnThread")
				<< "Failed to allocate TrackingRuntime for " << moduleName;
		}
	}
	else
	{
		MIKAN_LOG_ERROR("VRObjectSystem::initTrackingRuntimeOnThread") << "Failed to load module " << moduleName;
	}

	// Clean up on failure
	if (result.manager)
	{
		result.manager->shutdown();
		result.manager= nullptr;
	}
	if (result.module)
	{
		getMikanModuleManager()->disposeModule(result.module);
		result.module= nullptr;
	}

#ifdef _WIN32
	if (bComInitialized)
		CoUninitialize();
#endif // _WIN32

	return result;
}

eTrackingRuntime VRObjectSystem::findTrackingRuntimeForDeviceManager(const IVRDeviceManager* deviceManager) const
{
	for (const auto& kvpair : m_trackingRuntimes)
	{
		if (kvpair.second->getVRDeviceManager().get() == deviceManager)
		{
			return kvpair.first;
		}
	}

	return eTrackingRuntime::INVALID;
}

bool VRObjectSystem::findMikanDeviceIdForDeviceIndex(const IVRDeviceManager* deviceManager, const int deviceIndex,
													 eTrackingRuntime& outRuntime,
													 MikanVRDeviceID& outMikanDeviceId) const
{
	outRuntime= findTrackingRuntimeForDeviceManager(deviceManager);
	outMikanDeviceId= INVALID_MIKAN_ID;

	if (outRuntime != eTrackingRuntime::INVALID)
	{
		for (const auto& kvpair : Super::getComponentMap())
		{
			MikanVRDeviceID vrDeviceId= kvpair.first;
			VRDeviceComponentPtr vrDeviceComponentPtr= kvpair.second.lock();

			if (vrDeviceComponentPtr)
			{
				VRDeviceDefinitionPtr vrDeviceDefinition= vrDeviceComponentPtr->getVRDeviceDefinition();
				if (vrDeviceDefinition->getTrackingRuntimeType() == outRuntime)
				{
					IVRDevice* vrDeviceInterface= vrDeviceComponentPtr->getVRDeviceInterface();
					if (vrDeviceInterface && vrDeviceInterface->getDeviceIndex() == deviceIndex)
					{
						outMikanDeviceId= vrDeviceId;
						return true;
					}
				}
			}
		}
	}

	return false;
}

VRDeviceComponentPtr VRObjectSystem::getVRDeviceByPath(const std::string& VRDevicePath) const
{
	return Super::getTypedComponentByPredicate(
		[VRDevicePath](VRDeviceComponentConstPtr componentPtr)
		{ return componentPtr->getVRDeviceDefinition()->getVRDevicePath() == VRDevicePath; });
}

void VRObjectSystem::getVRDevicePathList(std::vector<std::string>& outDevicePathList) const
{
	outDevicePathList.clear();

	for (const auto& kvpair : Super::getComponentMap())
	{
		VRDeviceComponentPtr vrDeviceComponentPtr= kvpair.second.lock();
		if (vrDeviceComponentPtr)
		{
			VRDeviceDefinitionPtr vrDeviceDefinition= vrDeviceComponentPtr->getVRDeviceDefinition();
			const std::string devicePath= vrDeviceDefinition->getVRDevicePath();

			outDevicePathList.push_back(devicePath);
		}
	}
}

void VRObjectSystem::onActiveDeviceListChanged(IVRDeviceManager* deviceManager)
{
	eTrackingRuntime runtimeType= findTrackingRuntimeForDeviceManager(deviceManager);

	if (runtimeType != eTrackingRuntime::INVALID)
	{
		std::set<MikanVRDeviceID> pendingDeleteDeviceIds;
		for (const auto& kvpair : Super::getComponentMap())
		{
			const MikanVRDeviceID vrDeviceId= kvpair.first;
			VRDeviceComponentPtr vrDeviceComponentPtr= kvpair.second.lock();

			if (!vrDeviceComponentPtr
				|| vrDeviceComponentPtr->getVRDeviceDefinition()->getTrackingRuntimeType() == runtimeType)
			{
				pendingDeleteDeviceIds.insert(vrDeviceId);
			}
		}

		for (size_t deviceIndex= 0; deviceIndex < deviceManager->getDeviceCount(); deviceIndex++)
		{
			IVRDevice* vrDeviceInterface= deviceManager->getDeviceByIndex(deviceIndex);
			const std::string devicePath= vrDeviceInterface ? vrDeviceInterface->getDevicePath() : "";

			if (!devicePath.empty())
			{
				VRDeviceComponentPtr existingVRDeice= getVRDeviceByPath(devicePath);

				if (existingVRDeice)
				{
					const MikanVRDeviceID vrDeviceId= existingVRDeice->getVRDeviceDefinition()->getComponentId();

					pendingDeleteDeviceIds.erase(vrDeviceId);
				}
				else
				{
					addNewVRDevice(runtimeType, vrDeviceInterface);
				}
			}
		}

		for (const MikanVRDeviceID vrDeviceId : pendingDeleteDeviceIds)
		{
			removeVRDevice(vrDeviceId);
		}

		if (OnActiveDeviceListChanged)
		{
			OnActiveDeviceListChanged(runtimeType);
		}
	}
}

void VRObjectSystem::onDevicePropertyChanged(IVRDeviceManager* deviceManager, int deviceId)
{
	if (OnDevicePropertyChanged)
	{
		eTrackingRuntime runtimeType;
		MikanVRDeviceID vrDeviceId;

		if (findMikanDeviceIdForDeviceIndex(deviceManager, deviceId, runtimeType, vrDeviceId))
		{
			OnDevicePropertyChanged(runtimeType, vrDeviceId);
		}
	}
}

void VRObjectSystem::onDevicePosesChanged(IVRDeviceManager* deviceManager, int64_t newFrameId)
{
	eTrackingRuntime runtimeType= findTrackingRuntimeForDeviceManager(deviceManager);

	if (runtimeType != eTrackingRuntime::INVALID)
	{
		for (const auto& kvpair : Super::getComponentMap())
		{
			VRDeviceComponentPtr vrDeviceComponentPtr= kvpair.second.lock();

			if (vrDeviceComponentPtr
				&& vrDeviceComponentPtr->getVRDeviceDefinition()->getTrackingRuntimeType() == runtimeType)
			{
				vrDeviceComponentPtr->refreshDevicePose();
			}
		}

		if (OnDevicePosesChanged)
		{
			OnDevicePosesChanged(runtimeType, newFrameId);
		}
	}
}

// -- IEntityAccessor ----
Serialization::StructTypeHandle VRObjectSystem::getClientAPIValuesStructType() const
{
	return &MikanVRObjectSystemValues::staticGetArchetype();
}

// -- IPropertyInterface ----
const std::string VRObjectSystem::k_vrDevicePathListPropertyId= "vr_device_path_list";
void VRObjectSystem::getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors)
{
	Super::getPropertyDescriptors(outDescriptors);

	outDescriptors.push_back(std::make_shared<PropertyDescriptor>(VRObjectSystem::k_vrDevicePathListPropertyId,
																  MikanVariantType::STRING_ARRAY)
								 ->setReadOnly());
}

bool VRObjectSystem::getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const
{
	if (propertyName == k_vrDevicePathListPropertyId)
	{
		std::vector<std::string> devicePathList;
		getVRDevicePathList(devicePathList);
		outValue= devicePathList;
		return true;
	}

	return false;
}

VRDeviceComponentPtr VRObjectSystem::addNewVRDevice(eTrackingRuntime trackingRuntime, IVRDevice* vrDeviceInterface)
{
	VRDeviceComponentPtr vrDeviceComponent= Super::addNewObjectByTypedDefinition(
		[trackingRuntime, vrDeviceInterface](VRDeviceDefinitionPtr def)
		{
			const eVRDeviceType deviceType= vrDeviceInterface->getDeviceType();
			const std::string vrDevicePath= vrDeviceInterface->getDevicePath();
			const int vrFrameDelay= 0; // Use the latest pose for rendering

			VRDevicePose pose= {};
			vrDeviceInterface->getDevicePose(vrFrameDelay, pose);
			GlmTransform glmTransform= VRDevicePose_to_GlmTransform(pose);

			def->setTrackingRuntimeType(trackingRuntime);
			def->setVRDeviceIndex(vrDeviceInterface->getDeviceIndex());
			def->setVRDevicePath(vrDevicePath);
			def->setRelativeTransform(glmTransform);

			return true;
		});

	if (vrDeviceComponent)
	{
		// This will dynamically attach socket and mesh components based on the device interface
		vrDeviceComponent->setVRDeviceInterface(vrDeviceInterface);
	}

	return vrDeviceComponent;
}

bool VRObjectSystem::removeVRDevice(MikanVRDeviceID vrDeviceId)
{
	return Super::removeObjectByPrimaryComponentId(vrDeviceId);
}

// -- Utility Methods
void addAllVRDevicesToMkScene(VRObjectSystemPtr vrObjectSystem, IMkScenePtr mkScenePtr,
							  const glm::mat4& vrSpaceToStageSpace)
{
	vrObjectSystem->visitComponents(
		[&mkScenePtr, &vrSpaceToStageSpace](VRDeviceComponentPtr vrDeviceComponent)
		{
			vrDeviceComponent->visitAllTransformComponents(
				[&mkScenePtr, &vrSpaceToStageSpace](TransformComponent* transformComponent)
				{
					IMkSceneRenderablePtr renderable= transformComponent->getGlSceneRenderable();
					if (renderable)
					{
						// Apply VRSpace -> StageSpace offset to the renderable's model matrix.
						// VRObjectSystem resets this each frame via refreshDevicePose(), so
						// temporarily overwriting it here is safe.
						renderable->setModelMatrix(
							glm_composite_xform(renderable->getModelMatrix(), vrSpaceToStageSpace));
						mkScenePtr->addInstance(renderable);
					}
				});
		});
}

void renderAllVRDeviceInfo(VRObjectSystemPtr vrObjectSystem, IMkGraphicsContext* graphicsContext,
						   IMkCameraConstPtr camera, const glm::mat4& vrSpaceToStageSpace)
{
	vrObjectSystem->visitComponents(
		[graphicsContext, camera, &vrSpaceToStageSpace](VRDeviceComponentPtr vrDeviceComponent)
		{ vrDeviceComponent->renderVRDeviceInfo(graphicsContext, camera, vrSpaceToStageSpace); });
}