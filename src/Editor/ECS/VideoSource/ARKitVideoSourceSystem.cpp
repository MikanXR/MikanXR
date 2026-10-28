#include "ARKitVideoSourceSystem.h"
#include "ARKitMarkerAutoAligner.h"
#include "ARKitVideoSourceComponent.h"
#include "CudaDriverUtils.h"
#include "IEditorWindow.h"
#include "LocText.h"
#include "Logger.h"
#include "MikanServer.h"
#include "MikanVideoSourceEvents.h"
#include "ModalMessageBox/ModalDialog_MessageBox.h"
#include "VideoSourceRequestHandler.h"

#include <algorithm>
#include <chrono>

namespace
{
bool isStreaming(const ARKitVideoSourceComponent& source)
{
	return source.getVideoStreamingStatus() == eVideoStreamingStatus::started;
}
} // namespace

// -- ARKitVideoSourceSystemDefinition -----
ARKitVideoSourceSystemDefinition::ARKitVideoSourceSystemDefinition(const std::string& configName,
																   IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config ARKitVideoSourceSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void ARKitVideoSourceSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- ARKitVideoSourceSystem -----
ARKitVideoSourceSystem::ARKitVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
	// Retry openVideoSource() on any components that were waiting once the manager
	// becomes ready - the ECS-component-map-walk equivalent of
	// NetworkVideoSourceSystem's inline retry loop, expressed through
	// ARKitVideoDeviceManagerLoader's manager-ready-callback mechanism since the
	// async load itself lives there now (see that class's header comment).
	m_deviceManagerLoader.addManagerReadyCallback(
		[this]
		{
			for (const auto& [id, weakComp] : Super::getComponentMap())
			{
				if (auto comp= weakComp.lock(); comp && comp->isPendingOpen())
					comp->openVideoSource();
			}
		});
}

void ARKitVideoSourceSystem::update(float deltaTime)
{
	m_deviceManagerLoader.update(deltaTime);

	Super::update(deltaTime);

	updateCudaDriverWarning();
	updateAlignment();
}

void ARKitVideoSourceSystem::updateCudaDriverWarning()
{
#if defined(MIKAN_ARKIT_CUDA_VERSION)
	if (m_bCudaDriverChecked
		|| m_deviceManagerLoader.getARKitVideoManagerState()
			   != ARKitVideoDeviceManagerLoader::eARKitVideoManagerState::failed)
		return;

	// MikanCmd has no window to show a modal in
	IEditorWindow* ownerWindow= getOwnerWindow();
	if (ownerWindow == nullptr)
		return;

	// Only a project that uses ARKit video hears about it
	const auto& componentMap= getComponentMap();
	const bool bHasSource= std::any_of(componentMap.begin(), componentMap.end(),
									   [](const auto& entry) { return !entry.second.expired(); });
	if (!bHasSource)
		return;

	AppStage* appStage= ownerWindow->getCurrentAppStage();
	if (appStage == nullptr)
		return;

	// The plugin refuses to start on a driver older than the CUDA major version it was built
	// against. A new enough driver means the failure had another cause, which the log covers.
	m_bCudaDriverChecked= true;

	const int requiredMajorVersion= MIKAN_ARKIT_CUDA_VERSION / 1000;
	const int driverVersion= CudaDriverUtils::getInstalledCudaDriverVersion();
	if (driverVersion >= requiredMajorVersion * 1000)
		return;

	const std::string message= (driverVersion == 0)
								   ? locFormat("arkitVideoSource.noNvidiaDriverFmt", requiredMajorVersion)
								   : locFormat("arkitVideoSource.cudaDriverTooOldFmt", requiredMajorVersion,
											   driverVersion / 1000, (driverVersion % 1000) / 10);
	ModalDialog_MessageBox::showMessageBox(appStage, message);
#endif
}

void ARKitVideoSourceSystem::updateAlignment()
{
	const double now= std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	MikanServer* mikanServer= MikanServer::getInstance();

	// A client watch keeps a watch running on a source nothing else streams: the
	// aligner's own view subscription is what starts the stream
	auto isWatchedByClient= [mikanServer](MikanVideoSourceID videoSourceId)
	{ return mikanServer != nullptr && mikanServer->isVideoSourceAlignmentWatched(videoSourceId); };

	// The marker watch reads frames through the window's GL context, which
	// MikanCmd does not have
	IEditorWindow* ownerWindow= getOwnerWindow();
	if (ownerWindow != nullptr)
	{
		for (const auto& [id, weakComp] : getComponentMap())
		{
			ARKitVideoSourceComponentPtr source= weakComp.lock();
			if (!source || source->hasPoseOffset() || m_aligners.count(id) > 0)
				continue;

			const bool bStreaming= isStreaming(*source);
			if (!bStreaming && !isWatchedByClient(id))
				continue;

			m_aligners.emplace(id, std::make_unique<ARKitMarkerAutoAligner>(ownerWindow, source));
			if (bStreaming)
			{
				MIKAN_LOG_INFO("ARKitVideoSourceSystem")
					<< "Watching for the origin marker on " << source->getDevicePath();
			}
			else
			{
				MIKAN_LOG_INFO("ARKitVideoSourceSystem")
					<< "Watching for the origin marker on " << source->getDevicePath() << ", asked by a client";
			}
		}
	}

	for (auto it= m_aligners.begin(); it != m_aligners.end();)
	{
		ARKitVideoSourceComponentPtr source= getTypedComponentById(it->first);
		const bool bStreaming= source && isStreaming(*source);
		const bool bWanted= source && (bStreaming || isWatchedByClient(it->first));
		if (bWanted)
		{
			it->second->update(now);
		}

		if (!bWanted || it->second->shouldRetire())
		{
			it= m_aligners.erase(it);
		}
		else
		{
			++it;
		}
	}

	VideoSourceRequestHandler* requestHandler=
		mikanServer != nullptr ? mikanServer->getVideoSourceRequestHandler() : nullptr;
	const auto& componentMap= getComponentMap();
	for (const auto& [id, weakComp] : componentMap)
	{
		ARKitVideoSourceComponentPtr source= weakComp.lock();
		if (!source)
			continue;

		const AlignmentState state= computeAlignmentState(id, *source);
		auto publishedIt= m_publishedAlignment.find(id);
		if (publishedIt != m_publishedAlignment.end() && publishedIt->second == state)
			continue;

		m_publishedAlignment[id]= state;
		if (requestHandler != nullptr)
		{
			MikanVideoSourceAlignmentEvent alignmentEvent;
			alignmentEvent.video_source_id= id;
			alignmentEvent.is_aligned= state.bAligned;
			alignmentEvent.marker_visible= state.bMarkerVisible;
			alignmentEvent.is_watching= state.bWatching;
			requestHandler->publishVideoSourceAlignmentEvent(alignmentEvent);
		}
	}

	// A deleted source's state goes with it, so a source recreated under the same
	// id publishes again
	for (auto it= m_publishedAlignment.begin(); it != m_publishedAlignment.end();)
	{
		if (componentMap.count(it->first) == 0)
		{
			it= m_publishedAlignment.erase(it);
		}
		else
		{
			++it;
		}
	}
}

ARKitVideoSourceSystem::AlignmentState ARKitVideoSourceSystem::computeAlignmentState(
	MikanVideoSourceID videoSourceId, const ARKitVideoSourceComponent& source) const
{
	AlignmentState state;
	state.bAligned= source.hasPoseOffset();

	auto alignerIt= m_aligners.find(videoSourceId);
	if (alignerIt != m_aligners.end())
	{
		state.bMarkerVisible= alignerIt->second->isMarkerVisible();
		state.bWatching= true;
	}

	return state;
}

bool ARKitVideoSourceSystem::getAlignmentState(MikanVideoSourceID videoSourceId, bool& outAligned,
											   bool& outMarkerVisible, bool& outWatching) const
{
	ARKitVideoSourceComponentPtr source= getTypedComponentById(videoSourceId);
	if (!source)
		return false;

	const AlignmentState state= computeAlignmentState(videoSourceId, *source);
	outAligned= state.bAligned;
	outMarkerVisible= state.bMarkerVisible;
	outWatching= state.bWatching;
	return true;
}

// The device manager and its plugin live for the process, not the project:
// the components close their devices in Super::dispose(), and the loader is
// torn down with the system at app shutdown, after every device is gone. The
// marker watches release their stream subscriptions before the sources go.
void ARKitVideoSourceSystem::dispose()
{
	m_aligners.clear();
	m_publishedAlignment.clear();

	Super::dispose();
}

ARKitVideoSourceSystem::~ARKitVideoSourceSystem() { m_deviceManagerLoader.dispose(); }

VideoSourceIdList ARKitVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		ARKitVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}
