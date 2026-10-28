#pragma once

#include "ARKitVideoDeviceManagerLoader.h"
#include "ARKitVideoSourceComponent.h"
#include "ComponentFwd.h"
#include "IARKitVideoDeviceManager.h"
#include "MikanTypedObjectSystem.h"
#include "MikanVideoSourceTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "VideoSourceQueries.h"

#include <map>
#include <memory>
#include <string>

class ARKitMarkerAutoAligner;

class ARKitVideoSourceSystemDefinition
	: public MikanTypedObjectSystemDefinition<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID>
{
public:
	using Super=
		MikanTypedObjectSystemDefinition<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID>;

	ARKitVideoSourceSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

// ECS object system for ARKit video sources (ticket E1). The async MikanARKitVideo
// plugin/device-manager loading itself is delegated to ARKitVideoDeviceManagerLoader
// (formerly this class, before ticket E1 - see that class's header comment) rather
// than reimplemented inline, so its existing test coverage kept working unchanged.
class ARKitVideoSourceSystem
	: public MikanTypedObjectSystem<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID,
									ARKitVideoSourceSystem, ARKitVideoSourceSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<ARKitVideoSourceComponent, ARKitVideoSourceDefinition, MikanVideoSourceID,
										ARKitVideoSourceSystem, ARKitVideoSourceSystemDefinition>;

	ARKitVideoSourceSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "ARKitVideoObjectSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual void update(float deltaTime) override;
	virtual ~ARKitVideoSourceSystem();

	virtual void dispose() override;
	virtual bool isLoading() const override { return m_deviceManagerLoader.isLoading(); }

	IARKitVideoDeviceManagerPtr getARKitVideoDeviceManager() const
	{
		return m_deviceManagerLoader.getARKitVideoDeviceManager();
	}
	ARKitVideoDeviceManagerLoader::eARKitVideoManagerState getARKitVideoManagerState() const
	{
		return m_deviceManagerLoader.getARKitVideoManagerState();
	}

	VideoSourceIdList getVideoSourceIdList() const;

	// The stage alignment state of one of this system's sources: whether it holds
	// a pose offset, the marker watch's last detection, and whether a marker watch
	// runs on it. False for an id this system does not own.
	bool getAlignmentState(MikanVideoSourceID videoSourceId, bool& outAligned, bool& outMarkerVisible,
						   bool& outWatching) const;

private:
	struct AlignmentState
	{
		bool bAligned= false;
		bool bMarkerVisible= false;
		bool bWatching= false;

		bool operator==(const AlignmentState& other) const
		{
			return bAligned == other.bAligned && bMarkerVisible == other.bMarkerVisible && bWatching == other.bWatching;
		}
	};

	// Once per process, when the manager failed and the project holds an ARKit source,
	// shows a modal if the NVIDIA driver is too old for the plugin's CUDA version
	void updateCudaDriverWarning();

	// Runs a marker watch on each unaligned source while it streams or a client
	// watches it, and publishes each source's alignment state when it changes
	void updateAlignment();
	AlignmentState computeAlignmentState(MikanVideoSourceID videoSourceId,
										 const ARKitVideoSourceComponent& source) const;

	ARKitVideoDeviceManagerLoader m_deviceManagerLoader;
	bool m_bCudaDriverChecked= false;
	std::map<MikanVideoSourceID, std::unique_ptr<ARKitMarkerAutoAligner>> m_aligners;
	// The state last published per source, so a change publishes once
	std::map<MikanVideoSourceID, AlignmentState> m_publishedAlignment;
};

using ARKitVideoSourceSystemPtr= std::shared_ptr<ARKitVideoSourceSystem>;
