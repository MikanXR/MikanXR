#include "ProjectPipelineModel.h"

namespace
{
PipelineText makeKeyText(const char* key, double arg0= 0.0, double arg1= 0.0)
{
	PipelineText text;
	text.key= key;
	text.args[0]= arg0;
	text.args[1]= arg1;
	return text;
}

PipelineText makeNameText(const std::string& name)
{
	PipelineText text;
	text.name= name;
	return text;
}

PipelineStatus makeStatus(ePipelineState state, const PipelineText& value, const PipelineText& reason= PipelineText())
{
	PipelineStatus status;
	status.state= state;
	status.value= value;
	status.reason= reason;
	return status;
}

// A present node with nothing better to show reads its component's name
PipelineStatus makeConfigured(const PipelineNode& node)
{
	return makeStatus(ePipelineState::configured,
					  node.name.empty() ? makeKeyText("pipeline.valueConfigured") : makeNameText(node.name));
}

PipelineStatus makeCount(const PipelineNode& node)
{
	return makeStatus(node.facts.count > 0 ? ePipelineState::configured : ePipelineState::inactive,
					  makeKeyText("pipeline.valueCountFmt", node.facts.count));
}

PipelineStatus makeDisabled() { return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueDisabled")); }

// A mount is only as good as the device it names. Whether that device is
// being tracked right now is not known, so a resolved device reads configured.
PipelineStatus computeMountStatus(const PipelineNode& node)
{
	if (!node.facts.bActive)
	{
		return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueNoDevice"),
						  makeKeyText("pipeline.reasonDeviceNotConnected"));
	}
	return makeConfigured(node);
}

// A stencil or shape the compositor owns
PipelineStatus computeSceneMemberStatus(const PipelineNode& node)
{
	const PipelineFacts& facts= node.facts;
	if (facts.bFault)
	{
		return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueFailed"),
						  makeKeyText("pipeline.reasonModelFailed"));
	}
	if (!facts.bEnabled)
		return makeDisabled();
	if (facts.bGraphUseKnown && !facts.bUsedByGraph)
	{
		return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueUnused"),
						  makeKeyText("pipeline.reasonNotUsedByGraph"));
	}
	return makeStatus(ePipelineState::configured, PipelineText());
}

PipelineStatus computePresentStatus(const PipelineNode& node)
{
	const PipelineFacts& facts= node.facts;
	switch (node.box)
	{
	case ePipelineBox::volume:
		return makeStatus(ePipelineState::configured,
						  makeKeyText(facts.variant == (int)ePipelineVolumeVariant::vr ? "pipeline.valueVolumeVR"
																					   : "pipeline.valueVolumeMarker"));
	case ePipelineBox::steamVR:
		switch ((ePipelineRuntimeVariant)facts.variant)
		{
		case ePipelineRuntimeVariant::ready:
			return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueReady"));
		case ePipelineRuntimeVariant::initializing:
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueStarting"),
							  makeKeyText("pipeline.reasonRuntimeStarting"));
		case ePipelineRuntimeVariant::failed:
			return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueFailed"),
							  makeKeyText("pipeline.reasonRuntimeFailed"));
		case ePipelineRuntimeVariant::uninitialized:
			break;
		}
		return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueNotRunning"),
						  makeKeyText("pipeline.reasonRuntimeNotRunning"));
	case ePipelineBox::matMount:
	case ePipelineBox::trackingMount:
	case ePipelineBox::cameraMount:
		return computeMountStatus(node);
	case ePipelineBox::dmx:
		if (!facts.bHasSignal)
			return makeStatus(ePipelineState::configured, PipelineText());
		// The sender runs from project load, so one that is down failed to start
		if (!facts.bActive)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueNotRunning"),
							  makeKeyText("pipeline.reasonDmxNotRunning"));
		}
		// Nothing is sent until something writes a universe, so idle is not a fault
		if (facts.count == 0)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueIdle"));
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueUniversesFmt", facts.count));
	case ePipelineBox::lightGroup:
		if (facts.variant == (int)ePipelineLightGroupVariant::dark)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueDark"));
		return makeStatus(ePipelineState::running, makeNameText(facts.detailName));
	case ePipelineBox::scene:
		return makeStatus(ePipelineState::running, makeNameText(node.name));
	case ePipelineBox::environmentLight:
		// A probe that was never captured lights nothing
		if (facts.bHasSignal && !facts.bActive)
		{
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueNotCaptured"),
							  makeKeyText("pipeline.reasonNotCaptured"));
		}
		return makeConfigured(node);
	case ePipelineBox::depthMesh:
	case ePipelineBox::stencil:
	case ePipelineBox::shape:
		return computeSceneMemberStatus(node);
	case ePipelineBox::videoSource:
		switch ((ePipelineVideoVariant)facts.variant)
		{
		case ePipelineVideoVariant::started:
			return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueFpsFmt", facts.rateHz));
		case ePipelineVideoVariant::pendingStart:
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueStarting"),
							  makeKeyText("pipeline.reasonVideoStarting"));
		case ePipelineVideoVariant::stopped:
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueStopped"),
							  makeKeyText("pipeline.reasonVideoStopped"));
		case ePipelineVideoVariant::failed:
			break;
		}
		return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueFailed"),
						  makeKeyText("pipeline.reasonVideoFailed"));
	case ePipelineBox::lensCalibration:
		switch ((ePipelineLensVariant)facts.variant)
		{
		case ePipelineLensVariant::calibrated:
			return makeStatus(ePipelineState::configured, makeKeyText("pipeline.valueCalibrated"));
		case ePipelineLensVariant::resolutionMismatch:
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueMismatch"),
							  makeKeyText("pipeline.reasonLensResolutionMismatch"));
		case ePipelineLensVariant::uncalibrated:
			break;
		}
		return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueUncalibrated"),
						  makeKeyText("pipeline.reasonLensUncalibrated"));
	case ePipelineBox::compositor:
		if (!facts.bEnabled)
		{
			return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueNoGraph"),
							  makeKeyText("pipeline.reasonNoGraph"));
		}
		if (!facts.bActive)
		{
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueStopped"),
							  makeKeyText("pipeline.reasonCompositorStopped"));
		}
		if (facts.errorCount > 0)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueErrorsFmt", facts.errorCount),
							  makeKeyText("pipeline.reasonGraphErrorsFmt", facts.errorCount));
		}
		if (facts.variant == (int)ePipelineCompositorVariant::editorHeld)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueFpsFmt", facts.rateHz),
							  makeKeyText("pipeline.reasonEditorHeld"));
		}
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueFpsFmt", facts.rateHz));
	case ePipelineBox::clientTextureSource:
	case ePipelineBox::firstClientTextureSource:
		// The usual reason a composite is video only
		if (!facts.bActive)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueWaiting"),
							  makeKeyText("pipeline.reasonWaitingForClient"));
		}
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueFpsFmt", facts.rateHz));
	case ePipelineBox::spoutTextureSource:
		if (!facts.bHasSignal)
			return makeStatus(ePipelineState::configured, PipelineText());
		if (!facts.bActive)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueWaiting"),
							  makeKeyText("pipeline.reasonWaitingForSender"));
		}
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueReceiving"));
	case ePipelineBox::cefTextureSource:
		if (!facts.bActive)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueNoBrowser"),
							  makeKeyText("pipeline.reasonNoBrowser"));
		}
		if (facts.variant == (int)ePipelineBrowserVariant::loading)
		{
			return makeStatus(ePipelineState::degraded, makeKeyText("pipeline.valueLoading"),
							  makeKeyText("pipeline.reasonBrowserLoading"));
		}
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueReady"));
	case ePipelineBox::unresolvedTextureSource:
		return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueMissing"),
						  makeKeyText("pipeline.reasonUnresolvedTextureSource"));
	case ePipelineBox::output:
		return makeStatus(facts.count > 0 ? ePipelineState::running : ePipelineState::inactive,
						  makeKeyText("pipeline.valuePublishingFmt", facts.count));
	case ePipelineBox::spoutOutput:
		if (!facts.bEnabled)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueOff"));
		// A sender opens on the first frame it is handed, so closed means
		// nothing has published through it yet
		if (!facts.bActive)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueIdle"));
		return makeStatus(ePipelineState::running, makeNameText(facts.detailName));
	case ePipelineBox::rtpOutput:
		if (!facts.bEnabled)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueOff"));
		if (facts.bFault)
		{
			return makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueNoDestination"),
							  makeKeyText("pipeline.reasonNoDestination"));
		}
		if (!facts.bHasSignal)
			return makeStatus(ePipelineState::configured, makeNameText(facts.detailName));
		if (!facts.bActive)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueIdle"));
		return makeStatus(ePipelineState::running, makeNameText(facts.detailName));
	case ePipelineBox::returnFeed:
		if (!facts.bHasSignal)
			return makeStatus(ePipelineState::configured, PipelineText());
		if (!facts.bActive)
			return makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueIdle"));
		// How many clients take the feed is not held anywhere to read
		return makeStatus(ePipelineState::running, makeKeyText("pipeline.valueStreaming"));
	case ePipelineBox::trackingMounts:
	case ePipelineBox::lightGroups:
	case ePipelineBox::ungroupedLights:
	case ePipelineBox::depthMeshes:
	case ePipelineBox::textureSources:
	case ePipelineBox::stencils:
	case ePipelineBox::shapes:
		return makeCount(node);
	case ePipelineBox::stage:
	case ePipelineBox::originMarker:
	case ePipelineBox::lightingPreset:
	case ePipelineBox::camera:
	default:
		break;
	}
	return makeConfigured(node);
}

// The reason a required node reads not configured when it is absent
const char* getMissingReasonKey(ePipelineBox box)
{
	switch (box)
	{
	case ePipelineBox::volume:
		return "pipeline.reasonNoVolume";
	case ePipelineBox::stage:
		return "pipeline.reasonNoStage";
	case ePipelineBox::scene:
		return "pipeline.reasonNoCurrentScene";
	case ePipelineBox::originMarker:
		return "pipeline.reasonNoOriginMarker";
	case ePipelineBox::camera:
		return "pipeline.reasonNoCamera";
	case ePipelineBox::videoSource:
	case ePipelineBox::lensCalibration:
		return "pipeline.reasonNoVideoSource";
	case ePipelineBox::compositor:
		return "pipeline.reasonNoCompositor";
	default:
		break;
	}
	return "pipeline.reasonNotAssigned";
}

bool isValidIndex(const PipelineSnapshot& snapshot, int nodeIndex)
{
	return nodeIndex >= 0 && nodeIndex < (int)snapshot.nodes.size();
}

void openRowsForNode(const PipelineSnapshot& snapshot, int nodeIndex, PipelineSelection& selection)
{
	const PipelineNode& node= snapshot.nodes[nodeIndex];
	const PipelineNode* parent= isValidIndex(snapshot, node.parentIndex) ? &snapshot.nodes[node.parentIndex] : nullptr;

	switch (node.kind)
	{
	case ePipelineNodeKind::stage:
		selection.openStageId= node.id;
		selection.openKindId.clear();
		break;
	case ePipelineNodeKind::box:
		selection.openStageId= parent ? parent->id : std::string();
		selection.openKindId.clear();
		break;
	case ePipelineNodeKind::kindBox:
		selection.openStageId= parent ? parent->id : std::string();
		selection.openKindId= node.id;
		break;
	case ePipelineNodeKind::member:
		selection.openKindId= parent ? parent->id : std::string();
		selection.openStageId.clear();
		if (parent && isValidIndex(snapshot, parent->parentIndex))
			selection.openStageId= snapshot.nodes[parent->parentIndex].id;
		break;
	default:
		break;
	}
}

// Whether the row that draws this node is open under the selection
bool isNodeRowOpen(const PipelineSnapshot& snapshot, int nodeIndex, const PipelineSelection& selection)
{
	const PipelineNode& node= snapshot.nodes[nodeIndex];
	if (node.kind == ePipelineNodeKind::stage)
		return true;
	if (!isValidIndex(snapshot, node.parentIndex))
		return false;

	const std::string& parentId= snapshot.nodes[node.parentIndex].id;
	return node.kind == ePipelineNodeKind::member ? parentId == selection.openKindId
												  : parentId == selection.openStageId;
}
} // namespace

namespace ProjectPipelineModel
{
const char* getBoxNameKey(ePipelineBox box)
{
	switch (box)
	{
	case ePipelineBox::volume:
		return "pipeline.boxVolume";
	case ePipelineBox::steamVR:
		return "pipeline.boxSteamVR";
	case ePipelineBox::originMarker:
		return "pipeline.boxOriginMarker";
	case ePipelineBox::matMount:
		return "pipeline.boxMatMount";
	case ePipelineBox::trackingMounts:
		return "pipeline.boxTrackingMounts";
	case ePipelineBox::stage:
		return "pipeline.boxStage";
	case ePipelineBox::dmx:
		return "pipeline.boxDmx";
	case ePipelineBox::lightGroups:
		return "pipeline.boxLightGroups";
	case ePipelineBox::ungroupedLights:
		return "pipeline.boxUngroupedLights";
	case ePipelineBox::scene:
		return "pipeline.boxScene";
	case ePipelineBox::environmentLight:
		return "pipeline.boxEnvironmentLight";
	case ePipelineBox::depthMeshes:
		return "pipeline.boxDepthMeshes";
	case ePipelineBox::lightingPreset:
		return "pipeline.boxLightingPreset";
	case ePipelineBox::camera:
		return "pipeline.boxCamera";
	case ePipelineBox::cameraMount:
		return "pipeline.boxCameraMount";
	case ePipelineBox::videoSource:
		return "pipeline.boxVideoSource";
	case ePipelineBox::lensCalibration:
		return "pipeline.boxLensCalibration";
	case ePipelineBox::compositor:
		return "pipeline.boxCompositor";
	case ePipelineBox::textureSources:
		return "pipeline.boxTextureSources";
	case ePipelineBox::firstClientTextureSource:
		return "pipeline.boxFirstClientTextureSource";
	case ePipelineBox::unresolvedTextureSource:
		return "pipeline.boxUnresolvedTextureSource";
	case ePipelineBox::stencils:
		return "pipeline.boxStencils";
	case ePipelineBox::shapes:
		return "pipeline.boxShapes";
	case ePipelineBox::output:
		return "pipeline.boxOutput";
	case ePipelineBox::returnFeed:
		return "pipeline.boxReturnFeed";
	default:
		break;
	}
	// The remaining boxes are members, labeled by their component's name
	return "";
}

const char* getStateKey(ePipelineState state)
{
	switch (state)
	{
	case ePipelineState::configured:
		return "pipeline.stateConfigured";
	case ePipelineState::running:
		return "pipeline.stateRunning";
	case ePipelineState::degraded:
		return "pipeline.stateDegraded";
	case ePipelineState::notConfigured:
		return "pipeline.stateNotConfigured";
	case ePipelineState::inactive:
	default:
		break;
	}
	return "pipeline.stateInactive";
}

const char* getStateName(ePipelineState state)
{
	switch (state)
	{
	case ePipelineState::configured:
		return "configured";
	case ePipelineState::running:
		return "running";
	case ePipelineState::degraded:
		return "degraded";
	case ePipelineState::notConfigured:
		return "not-configured";
	case ePipelineState::inactive:
	default:
		break;
	}
	return "inactive";
}

int findNodeById(const PipelineSnapshot& snapshot, const std::string& id)
{
	if (id.empty())
		return -1;

	for (int index= 0; index < (int)snapshot.nodes.size(); ++index)
	{
		if (snapshot.nodes[index].id == id)
			return index;
	}
	return -1;
}

std::vector<int> findNodesByComponentId(const PipelineSnapshot& snapshot, int componentId)
{
	std::vector<int> result;
	if (componentId < 0)
		return result;

	for (int index= 0; index < (int)snapshot.nodes.size(); ++index)
	{
		if (snapshot.nodes[index].componentId == componentId)
			result.push_back(index);
	}
	return result;
}

std::vector<int> getTopRow(const PipelineSnapshot& snapshot)
{
	std::vector<int> result;
	for (int index= 0; index < (int)snapshot.nodes.size(); ++index)
	{
		if (snapshot.nodes[index].kind == ePipelineNodeKind::stage)
			result.push_back(index);
	}
	return result;
}

std::vector<int> getChildren(const PipelineSnapshot& snapshot, int nodeIndex)
{
	std::vector<int> result;
	if (!isValidIndex(snapshot, nodeIndex))
		return result;

	for (int index= 0; index < (int)snapshot.nodes.size(); ++index)
	{
		if (snapshot.nodes[index].parentIndex == nodeIndex)
			result.push_back(index);
	}
	return result;
}

PipelineStatus computeOwnStatus(const PipelineSnapshot& snapshot, int nodeIndex)
{
	PipelineStatus status;
	if (!isValidIndex(snapshot, nodeIndex))
		return status;

	const PipelineNode& node= snapshot.nodes[nodeIndex];
	if (node.facts.bPresent)
	{
		status= computePresentStatus(node);
	}
	else if (node.facts.bRequired)
	{
		status= makeStatus(ePipelineState::notConfigured, makeKeyText("pipeline.valueMissing"),
						   makeKeyText(getMissingReasonKey(node.box)));
	}
	else
	{
		status= makeStatus(ePipelineState::inactive, makeKeyText("pipeline.valueNone"));
	}
	status.sourceNodeIndex= nodeIndex;
	return status;
}

PipelineStatus computeStatus(const PipelineSnapshot& snapshot, int nodeIndex)
{
	PipelineStatus status= computeOwnStatus(snapshot, nodeIndex);
	if (!isValidIndex(snapshot, nodeIndex))
		return status;

	// The box keeps its own value. Only the state and the reason it carries
	// come from the worst node below it, the first one on a tie.
	for (int childIndex : getChildren(snapshot, nodeIndex))
	{
		const PipelineStatus childStatus= computeStatus(snapshot, childIndex);
		if ((int)childStatus.state > (int)status.state && (int)childStatus.state >= (int)ePipelineState::degraded)
		{
			status.state= childStatus.state;
			status.reason= childStatus.reason;
			status.sourceNodeIndex= childStatus.sourceNodeIndex;
		}
	}
	return status;
}

void applyClick(const PipelineSnapshot& snapshot, int nodeIndex, PipelineSelection& selection)
{
	if (!isValidIndex(snapshot, nodeIndex))
		return;

	selection.selectedId= snapshot.nodes[nodeIndex].id;
	openRowsForNode(snapshot, nodeIndex, selection);
}

void applyFollow(const PipelineSnapshot& snapshot, int componentId, PipelineSelection& selection)
{
	const std::vector<int> nodeIndices= findNodesByComponentId(snapshot, componentId);
	if (nodeIndices.empty())
	{
		selection.selectedId.clear();
		return;
	}

	// A component can sit in two boxes (a mount under the volume and the
	// camera). The one already on screen wins, so following never closes the
	// row being looked at.
	int chosenIndex= nodeIndices.front();
	for (int nodeIndex : nodeIndices)
	{
		if (snapshot.nodes[nodeIndex].id == selection.selectedId)
		{
			chosenIndex= nodeIndex;
			break;
		}
	}
	if (snapshot.nodes[chosenIndex].id != selection.selectedId)
	{
		for (int nodeIndex : nodeIndices)
		{
			if (snapshot.nodes[nodeIndex].kind != ePipelineNodeKind::stage
				&& isNodeRowOpen(snapshot, nodeIndex, selection))
			{
				chosenIndex= nodeIndex;
				break;
			}
		}
	}
	applyClick(snapshot, chosenIndex, selection);
}

void validateSelection(const PipelineSnapshot& snapshot, PipelineSelection& selection)
{
	if (findNodeById(snapshot, selection.selectedId) < 0)
		selection.selectedId.clear();
	if (findNodeById(snapshot, selection.openStageId) < 0)
		selection.openStageId.clear();
	if (findNodeById(snapshot, selection.openKindId) < 0)
		selection.openKindId.clear();
}
} // namespace ProjectPipelineModel
