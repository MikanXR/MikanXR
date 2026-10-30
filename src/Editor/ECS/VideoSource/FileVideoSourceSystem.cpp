#include "FileVideoSourceSystem.h"
#include "CameraComponent.h"
#include "CameraObjectSystem.h"
#include "FileVideoSourceComponent.h"
#include "Logger.h"
#include "MarkerObjectSystem.h"
#include "ModelStencilComponent.h"
#include "ModelStencilSystem.h"
#include "StageComponent.h"
#include "TrackingVolumeComponent.h"

#include <cmath>

// -- FileVideoSourceSystemDefinition -----
FileVideoSourceSystemDefinition::FileVideoSourceSystemDefinition(const std::string& configName,
																 IEntityIDAllocatorPtr idAllocator)
	: Super::MikanTypedObjectSystemDefinition(configName, idAllocator)
{
}

configuru::Config FileVideoSourceSystemDefinition::writeToJSON()
{
	configuru::Config pt= Super::writeToJSON();

	return pt;
}

void FileVideoSourceSystemDefinition::readFromJSON(const configuru::Config& pt) { Super::readFromJSON(pt); }

// -- FileVideoSourceSystem -----
FileVideoSourceSystem::FileVideoSourceSystem(ProjectManagerPtr ownerObjectSystem)
	: Super::MikanTypedObjectSystem(ownerObjectSystem)
{
}

void FileVideoSourceSystem::update(float deltaSeconds)
{
	Super::update(deltaSeconds);

	updateMarkerShotAlignment();
	updateAlignedStencils();
}

glm::mat4 FileVideoSourceSystem::computeTakeToWorldTransform(const StageComponent& stage,
															 const FileVideoSourceComponent& source)
{
	return stage.getWorldTransform() * source.getPoseOffset();
}

// The marker shot aligns its take to the stage of the camera playing it, whose
// origin marker is the one the shot shows, and the height snap then sizes its
// table annulus from that marker. A source no camera on a marker stage plays
// waits until one does.
void FileVideoSourceSystem::updateMarkerShotAlignment()
{
	CameraObjectSystemPtr cameraSystem= getObjectSystemOfType<CameraObjectSystem>();
	MarkerObjectSystemPtr markerSystem= getObjectSystemOfType<MarkerObjectSystem>();
	if (!cameraSystem || !markerSystem)
		return;

	for (const auto& [id, weakComp] : getComponentMap())
	{
		FileVideoSourceComponentPtr source= weakComp.lock();
		if (!source || (!source->wantsMarkerShotAlignment() && !source->wantsHeightSnap()))
			continue;

		CameraComponentPtr camera= cameraSystem->findCameraForVideoSource(id);
		StageComponentConstPtr stage= camera ? camera->getOwnerStageComponent() : StageComponentConstPtr();
		TrackingVolumeDefinitionConstPtr trackingVolume=
			stage ? stage->getTrackingVolumeDefinitionConst() : TrackingVolumeDefinitionConstPtr();
		MarkerDefinitionConstPtr originMarker=
			trackingVolume ? trackingVolume->getOriginMarker() : MarkerDefinitionConstPtr();
		if (!originMarker)
			continue;

		if (source->wantsMarkerShotAlignment())
			source->alignFromMarkerShot(originMarker, markerSystem->getTypedDefinition()->getArucoDictionaryType());
		if (source->wantsHeightSnap())
			source->startHeightSnap(originMarker->getLengthMM() * 0.001f);
	}
}

void FileVideoSourceSystem::updateAlignedStencils()
{
	const auto& componentMap= getComponentMap();
	for (const auto& [id, weakComp] : componentMap)
	{
		FileVideoSourceComponentPtr source= weakComp.lock();
		if (!source)
			continue;

		ObservedAlignment observed;
		observed.bAligned= source->hasPoseOffset();
		observed.sessionId= source->getPoseSessionId();
		observed.poseOffset= source->getPoseOffset();

		auto it= m_observedAlignment.find(id);
		const bool bChanged= it == m_observedAlignment.end() || it->second.bAligned != observed.bAligned
							 || it->second.sessionId != observed.sessionId
							 || it->second.poseOffset != observed.poseOffset;
		m_observedAlignment[id]= observed;

		if (bChanged && observed.bAligned)
			placeAlignedStencils(*source);
	}

	// A deleted source's record goes with it
	for (auto it= m_observedAlignment.begin(); it != m_observedAlignment.end();)
	{
		it= componentMap.count(it->first) == 0 ? m_observedAlignment.erase(it) : std::next(it);
	}
}

void FileVideoSourceSystem::placeAlignedStencils(const FileVideoSourceComponent& source)
{
	ModelStencilSystemPtr stencilSystem= getObjectSystemOfType<ModelStencilSystem>();
	CameraObjectSystemPtr cameraSystem= getObjectSystemOfType<CameraObjectSystem>();
	if (!stencilSystem || !cameraSystem || !source.hasPoseOffset())
		return;

	CameraComponentPtr camera= cameraSystem->findCameraForVideoSource(source.getVideoSourceId());
	StageComponentConstPtr stage= camera ? camera->getOwnerStageComponent() : StageComponentConstPtr();
	if (!stage)
		return;

	const std::string sessionId= source.getPoseSessionId();
	const glm::mat4 takeToWorld= computeTakeToWorldTransform(*stage, source);
	for (const auto& [stencilId, weakStencil] : stencilSystem->getComponentMap())
	{
		ModelStencilComponentPtr stencil= weakStencil.lock();
		if (!stencil)
			continue;

		ModelStencilDefinitionPtr definition= stencil->getModelStencilDefinition();
		if (definition->getAlignedVideoSourceId() != source.getVideoSourceId()
			|| definition->getAlignedSessionId() != sessionId)
			continue;

		// Opening a project on its saved alignment lands every stencil where it
		// already is, and records no change
		const glm::mat4& current= stencil->getWorldTransform();
		bool bInPlace= true;
		for (int column= 0; column < 4 && bInPlace; ++column)
		{
			for (int row= 0; row < 4 && bInPlace; ++row)
			{
				bInPlace= std::fabs(current[column][row] - takeToWorld[column][row]) < 1e-5f;
			}
		}
		if (bInPlace)
			continue;

		stencil->setWorldTransform(takeToWorld);
		MIKAN_LOG_INFO("FileVideoSourceSystem::placeAlignedStencils")
			<< "Re-placed stencil " << stencilId << " on its take's new alignment";
	}
}

VideoSourceIdList FileVideoSourceSystem::getVideoSourceIdList() const
{
	VideoSourceIdList videoSourceIdList;
	for (const auto& it : Super::getComponentMap())
	{
		FileVideoSourceComponentPtr componentPtr= it.second.lock();
		if (componentPtr)
		{
			videoSourceIdList.push_back(componentPtr->getVideoSourceId());
		}
	}
	return videoSourceIdList;
}
