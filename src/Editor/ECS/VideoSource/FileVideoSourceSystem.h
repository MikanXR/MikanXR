#pragma once

#include "ComponentFwd.h"
#include "FileVideoSourceComponent.h"
#include "MikanTypedObjectSystem.h"
#include "MikanVideoSourceTypes.h"
#include "ObjectSystemConfigFwd.h"
#include "VideoSourceQueries.h"

#include "glm/ext/matrix_float4x4.hpp"

#include <map>
#include <string>

class StageComponent;

class FileVideoSourceSystemDefinition
	: public MikanTypedObjectSystemDefinition<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID>
{
public:
	using Super=
		MikanTypedObjectSystemDefinition<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID>;

	FileVideoSourceSystemDefinition(const std::string& configName, IEntityIDAllocatorPtr idAllocator);

	virtual configuru::Config writeToJSON();
	virtual void readFromJSON(const configuru::Config& pt);
};

// File video sources decode in-process, so unlike the other video source
// systems there is no plugin module to load and nothing to wait on.
//
// The system also keeps recorded takes aligned. A trackcamera take with no
// alignment for its session aligns itself from its own marker shot when it
// opens. A model stencil fused from a take follows that take's alignment: when
// the take is aligned again, by its marker shot or by Align Camera, the stencil
// is re-placed.
class FileVideoSourceSystem
	: public MikanTypedObjectSystem<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID,
									FileVideoSourceSystem, FileVideoSourceSystemDefinition>
{
public:
	using Super= MikanTypedObjectSystem<FileVideoSourceComponent, FileVideoSourceDefinition, MikanVideoSourceID,
										FileVideoSourceSystem, FileVideoSourceSystemDefinition>;

	FileVideoSourceSystem(ProjectManagerPtr ownerObjectSystem);

	inline static const std::string k_objectSystemClassName= "FileVideoSourceSystem";
	virtual std::string getObjectSystemClassName() const { return k_objectSystemClassName; }

	virtual void update(float deltaSeconds) override;

	VideoSourceIdList getVideoSourceIdList() const;

	// Where a mesh in the take's own world lands in the editor's world: the same
	// offset that carries a recorded camera pose into the stage carries the mesh,
	// and the stage's own transform takes it on into the world
	static glm::mat4 computeTakeToWorldTransform(const StageComponent& stage, const FileVideoSourceComponent& source);

	// Moves every model stencil fused from this source's take, in its current
	// session, to where the take's alignment puts its mesh. A stencil already
	// there is left alone.
	void placeAlignedStencils(const FileVideoSourceComponent& source);

private:
	void updateMarkerShotAlignment();
	void updateAlignedStencils();

	// The alignment each source last reported, so a change re-places its stencils
	struct ObservedAlignment
	{
		bool bAligned= false;
		std::string sessionId;
		glm::mat4 poseOffset= glm::mat4(1.f);
	};
	std::map<MikanVideoSourceID, ObservedAlignment> m_observedAlignment;
};

using FileVideoSourceSystemPtr= std::shared_ptr<FileVideoSourceSystem>;
