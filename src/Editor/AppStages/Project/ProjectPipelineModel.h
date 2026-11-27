#pragma once

#include <string>
#include <vector>

// The chain that feeds the outputs from the current scene, as the Pipeline
// panel shows it: volume, stage, scene, camera, compositor, output, each with
// its sub-stage boxes and the members of the boxes that stand for a set. Pure
// data and pure rules: no ImGui and no object systems. A gather step fills a
// PipelineSnapshot from the project once per frame, and everything here is a
// function of that snapshot, so the status rules run under test on hand-built
// snapshots. Text leaves as localization keys with numeric arguments, or as a
// component's own name.

enum class ePipelineStage : int
{
	INVALID= -1,

	volume= 0,
	stage,
	scene,
	camera,
	compositor,
	output,

	COUNT
};

enum class ePipelineNodeKind : int
{
	INVALID= -1,

	// One of the six top row stages
	stage= 0,
	// A sub-stage that stands for one thing
	box,
	// A sub-stage that stands for a set, whose members fill the third row
	kindBox,
	// One member of a kind box
	member,

	COUNT
};

// What a node is, which selects its status rule
enum class ePipelineBox : int
{
	INVALID= -1,

	volume= 0,
	steamVR,
	originMarker,
	matMount,
	trackingMounts,
	trackingMount,

	stage,
	dmx,
	lightGroups,
	lightGroup,
	ungroupedLights,

	scene,
	environmentLight,
	depthMeshes,
	depthMesh,
	lightingPreset,

	camera,
	cameraMount,
	videoSource,
	lensCalibration,

	compositor,
	textureSources,
	clientTextureSource,
	firstClientTextureSource,
	spoutTextureSource,
	cefTextureSource,
	unresolvedTextureSource,
	stencils,
	stencil,
	shapes,
	shape,

	output,
	spoutOutput,
	rtpOutput,
	returnFeed,

	COUNT
};

enum class ePipelineState : int
{
	// Not part of this chain, switched off, or nothing to do
	inactive= 0,
	// Assigned and usable, with no live signal to say more
	configured,
	running,
	// Running, but part of what it produces is missing
	degraded,
	// The chain needs it and it is missing or failed
	notConfigured,

	COUNT
};

// The facts variant holds for the boxes that have one
enum class ePipelineVolumeVariant : int
{
	marker= 0,
	vr,
};
enum class ePipelineRuntimeVariant : int
{
	uninitialized= 0,
	initializing,
	ready,
	failed,
};
enum class ePipelineLightGroupVariant : int
{
	dark= 0,
	preset,
	sequence,
};
enum class ePipelineVideoVariant : int
{
	failed= 0,
	stopped,
	pendingStart,
	started,
};
enum class ePipelineLensVariant : int
{
	uncalibrated= 0,
	resolutionMismatch,
	calibrated,
};
enum class ePipelineCompositorVariant : int
{
	publishing= 0,
	// Running only because a graph editor window holds it, so it feeds no output
	editorHeld,
};
enum class ePipelineBrowserVariant : int
{
	ready= 0,
	loading,
};

// A localization key and the numbers its format takes, or a literal name. A
// key ending in "Fmt" is a printf format whose specifiers are all floating
// point, taken in order from args. With no key the text is the name.
struct PipelineText
{
	std::string key;
	double args[3]= {0.0, 0.0, 0.0};
	std::string name;

	bool isEmpty() const { return key.empty() && name.empty(); }
};

// What the gather step read for one node, as plain values. Each box's rule
// reads the fields that apply to it and ignores the rest.
struct PipelineFacts
{
	// The component or link the node stands for exists
	bool bPresent= false;
	// The chain cannot work without it, so absent reads not configured
	// rather than inactive
	bool bRequired= false;
	// Not switched off (an output's enabled flag, a stencil's disabled flag,
	// a compositor holding a valid graph)
	bool bEnabled= true;
	// A live signal exists for this node. Without one a present node reads
	// configured and bActive is ignored.
	bool bHasSignal= false;
	// The live signal says it is working (streaming, connected, sender open)
	bool bActive= false;
	// A failure of the node's own (a model that did not load, an output with
	// no destination)
	bool bFault= false;
	// Whether graph use is known for the member, and whether a node pulls it
	bool bGraphUseKnown= false;
	bool bUsedByGraph= false;
	// Members, universes, publishing outputs, destinations
	int count= 0;
	float rateHz= 0.f;
	int errorCount= 0;
	// One of the ePipeline*Variant enums, by box
	int variant= 0;
	// A name the value quotes: the playing sequence, the preset, the sender
	std::string detailName;
};

struct PipelineNode
{
	// The stable untranslated id automation uses: "camera", "camera.video",
	// "compositor.stencils", "compositor.stencils.1042"
	std::string id;
	ePipelineNodeKind kind= ePipelineNodeKind::INVALID;
	ePipelineBox box= ePipelineBox::INVALID;
	ePipelineStage stage= ePipelineStage::INVALID;
	// Index into PipelineSnapshot::nodes, -1 for a top row stage
	int parentIndex= -1;
	// The component a click selects, -1 when the node stands for none
	int componentId= -1;
	// The component's own name, shown as a member's label and as the value
	// of a box that has nothing better to show
	std::string name;
	// The function a double-click invokes and the component it runs on,
	// empty and -1 when the node has no tool
	std::string toolFunctionId;
	int toolComponentId= -1;
	PipelineFacts facts;
};

struct PipelineSnapshot
{
	bool bHasCurrentScene= false;
	// Top row stages first in ePipelineStage order, each parent ahead of its
	// children
	std::vector<PipelineNode> nodes;
};

struct PipelineStatus
{
	ePipelineState state= ePipelineState::inactive;
	// The one number or word the box shows
	PipelineText value;
	// Why the node is not simply running, empty when there is nothing to say
	PipelineText reason;
	// The node the state and reason came from: the node itself, or the child
	// whose state rolled up into it
	int sourceNodeIndex= -1;
};

// Which box is highlighted and which rows are open, by node id so it survives
// a snapshot rebuild
struct PipelineSelection
{
	std::string selectedId;
	// The top row stage whose sub-stages fill the second row
	std::string openStageId;
	// The kind box whose members fill the third row
	std::string openKindId;
};

namespace ProjectPipelineModel
{
// "pipeline.box<Name>": the label of a stage, box or kind box. A member is
// labeled by its component name instead.
const char* getBoxNameKey(ePipelineBox box);
// "pipeline.state<Name>"
const char* getStateKey(ePipelineState state);
// The untranslated spelling automation prints: "running", "not-configured"
const char* getStateName(ePipelineState state);

int findNodeById(const PipelineSnapshot& snapshot, const std::string& id);
std::vector<int> findNodesByComponentId(const PipelineSnapshot& snapshot, int componentId);

// The six stage nodes, left to right
std::vector<int> getTopRow(const PipelineSnapshot& snapshot);
// A node's direct children in snapshot order: a stage's sub-stages, a kind
// box's members
std::vector<int> getChildren(const PipelineSnapshot& snapshot, int nodeIndex);

// The node's status from its own facts alone
PipelineStatus computeOwnStatus(const PipelineSnapshot& snapshot, int nodeIndex);
// The node's status with its children rolled up: the worst state among the
// node and everything below it. Nothing rolls downstream, so a stage is never
// marked for a fault in the stage to its left.
PipelineStatus computeStatus(const PipelineSnapshot& snapshot, int nodeIndex);

// Selecting a node highlights it and opens the rows that hold it. A stage
// opens its own sub-stage row, a kind box its member row.
void applyClick(const PipelineSnapshot& snapshot, int nodeIndex, PipelineSelection& selection);
// The editor's selection moved to a component. In the chain: as a click on
// its node, preferring a node in a row that is already open. Not in the
// chain: the highlight drops and the open rows stay.
void applyFollow(const PipelineSnapshot& snapshot, int componentId, PipelineSelection& selection);
// Clears whatever the selection names that the snapshot no longer holds
void validateSelection(const PipelineSnapshot& snapshot, PipelineSelection& selection);
} // namespace ProjectPipelineModel
