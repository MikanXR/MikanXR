#pragma once

#include "CommonConfig.h"
#include "ComponentFwd.h"
#include "IVRDeviceManager.h"
#include "MikanTypeFwd.h"
#include "MikanMathTypes.h"
#include "MikanTypedObjectSystem.h"
#include "MkRendererFwd.h"
#include "MulticastDelegate.h"
#include "ObjectSystemFwd.h"
#include "ObjectSystemConfigFwd.h"
#include "ProjectConfigConstants.h"
#include "VRDeviceComponent.h"

#include <future>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <glm/glm.hpp>
#include <glm/ext/matrix_float4x4.hpp>

class GlmTransform;

class VRObjectSystemDefinition
	: public MikanTypedObjectSystemDefinition<VRDeviceComponent, VRDeviceDefinition, MikanVRDeviceID>
{
public:
	using Super= MikanTypedObjectSystemDefinition<VRDeviceComponent, VRDeviceDefinition, MikanVRDeviceID>;

	VRObjectSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	// VRObjectSystem is runtime only and isn't saved to/loaded from the project file
	virtual bool wantsConfigSerialization() const override { return false; }
};

class VRObjectSystem : public MikanTypedObjectSystem<VRDeviceComponent, VRDeviceDefinition, MikanVRDeviceID,
													 VRObjectSystem, VRObjectSystemDefinition>,
					   public IVRDeviceManagerListener
{
public:
	using Super= MikanTypedObjectSystem<VRDeviceComponent, VRDeviceDefinition, MikanVRDeviceID, VRObjectSystem,
										VRObjectSystemDefinition>;

	VRObjectSystem(ProjectManagerPtr ownerObjectSystem)
		: Super::MikanTypedObjectSystem(ownerObjectSystem)
	{
	}

	inline static const std::string k_objectSystemClassName= "VRObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual bool init(MikanObjectSystemDefinitionPtr definitionPtr) override;
	virtual void update(float deltaSeconds) override;
	virtual void dispose() override;
	virtual bool isLoading() const override
	{
		return !m_deferredRuntimeLaunches.empty() || !m_pendingRuntimeFutures.empty();
	}

	bool createTrackingRuntime(eTrackingRuntime desiredRuntime);
	void launchDeferredRuntimeThreads();

	enum class eTrackingRuntimeState
	{
		uninitialized,
		initializing,
		ready,
		failed
	};
	eTrackingRuntimeState getTrackingRuntimeState(eTrackingRuntime runtime) const;

	VRDeviceComponentPtr getVRDeviceByPath(const std::string& VRDevicePath) const;
	void getVRDevicePathList(std::vector<std::string>& outDevicePathList) const;

	MulticastDelegate<void(eTrackingRuntime runtime)> OnActiveDeviceListChanged;
	MulticastDelegate<void(eTrackingRuntime runtime, MikanVRDeviceID deviceId)> OnDevicePropertyChanged;
	MulticastDelegate<void(eTrackingRuntime runtime, int64_t newFrameId)> OnDevicePosesChanged;

	// -- IEntityAccessor ----
	virtual Serialization::StructTypeHandle getClientAPIValuesStructType() const override;

	// -- IPropertyInterface ----
	static const std::string k_vrDevicePathListPropertyId;
	static void getPropertyDescriptors(std::vector<PropertyDescriptorConstPtr>& outDescriptors);
	virtual bool getPropertyValue(const std::string& propertyName, MikanVariant& outValue) const override;

protected:
	eTrackingRuntime findTrackingRuntimeForDeviceManager(const IVRDeviceManager* deviceManager) const;
	bool findMikanDeviceIdForDeviceIndex(const IVRDeviceManager* deviceManager, const int deviceIndex,
										 eTrackingRuntime& outRuntime, MikanVRDeviceID& outMikanDeviceId) const;

	VRDeviceComponentPtr addNewVRDevice(eTrackingRuntime trackingRuntime, class IVRDevice* vrDeviceInterface);
	bool removeVRDevice(MikanVRDeviceID VRDeviceId);

	// Project Config Events
	void onProjectConfigMarkedDirty(CommonConfigPtr configPtr, const class ConfigPropertyChangeSet& changedPropertySet);

	// VRSystem Config Events
	void onVRSystemConfigMarkedDirty(CommonConfigPtr configPtr,
									 const class ConfigPropertyChangeSet& changedPropertySet);

	// IVRDeviceManagerListener
	virtual void onActiveDeviceListChanged(IVRDeviceManager* deviceManager) override;
	virtual void onDevicePropertyChanged(IVRDeviceManager* deviceManager, int deviceId) override;
	virtual void onDevicePosesChanged(IVRDeviceManager* deviceManager, int64_t newFrameId) override;

private:
	struct TrackingRuntimeInitResult
	{
		eTrackingRuntime runtime= eTrackingRuntime::INVALID;
		class IVRDeviceModule* module= nullptr;
		IVRDeviceManagerPtr manager;
	};
	static TrackingRuntimeInitResult initTrackingRuntimeOnThread(eTrackingRuntime runtime,
																 const std::string& moduleName,
																 class IMkGraphicsContext* graphicsContext);

	using VRTrackingRuntimePtr= std::shared_ptr<class VRTrackingRuntime>;
	std::map<eTrackingRuntime, VRTrackingRuntimePtr> m_trackingRuntimes;
	std::map<eTrackingRuntime, eTrackingRuntimeState> m_trackingRuntimeStates;
	std::map<eTrackingRuntime, std::future<TrackingRuntimeInitResult>> m_pendingRuntimeFutures;
	std::set<eTrackingRuntime> m_deferredRuntimeLaunches; // Runtimes requested during init, launched on first update
	ProjectConfigWeakPtr m_projectConfigWeakPtr;
};

// -- Utility Methods
void addAllVRDevicesToMkScene(VRObjectSystemPtr vrObjectSystem, IMkScenePtr mkScenePtr,
							  const glm::mat4& vrSpaceToStageSpace= glm::mat4(1.f));
void renderAllVRDeviceInfo(VRObjectSystemPtr vrObjectSystem, IMkGraphicsContext* graphicsContext,
						   IMkCameraConstPtr camera, const glm::mat4& vrSpaceToStageSpace= glm::mat4(1.f));