---@meta
-- Mikan scripting API — component class hierarchy.
-- This file is a LuaLS definition file; it contains no executable code.
-- Keep in sync with: src/Editor/ECS/ component bindLuaFunctions implementations.

------------------------------------------------------------------------
-- MikanComponent — base class for all Mikan components
------------------------------------------------------------------------

---@class MikanComponent
---@field name string Component name (read/write)
---@field className string Component class name (read-only)
---@field componentId integer Unique component ID (read-only)
local MikanComponent = {}

--- Get the CameraObjectSystem singleton.
---@return CameraObjectSystem
function MikanComponent:getCameraSystem() end

--- Get the SceneObjectSystem singleton.
---@return SceneObjectSystem
function MikanComponent:getSceneSystem() end

--- Get the DMXObjectSystem singleton.
---@return DMXObjectSystem
function MikanComponent:getDMXSystem() end

--- Get the AnchorObjectSystem singleton.
---@return AnchorObjectSystem
function MikanComponent:getAnchorSystem() end

--- Get the CompositorObjectSystem singleton.
---@return CompositorObjectSystem
function MikanComponent:getCompositorSystem() end

------------------------------------------------------------------------
-- TransformComponent : MikanComponent
------------------------------------------------------------------------

---@class TransformComponent : MikanComponent
---@field relativePosition Vec3f Position relative to parent (read/write)
---@field relativeRotation Vec3f Euler rotation in degrees relative to parent (read/write)
---@field relativeScale Vec3f Scale relative to parent (read/write)
local TransformComponent = {}

------------------------------------------------------------------------
-- SceneComponent : TransformComponent
------------------------------------------------------------------------

---@class SceneComponent : TransformComponent
---@field parentStageId integer ID of the parent stage (read-only)
---@field displayCompositorId integer ID of the display compositor (read-only)
local SceneComponent = {}

--- Switch the scene's display to its compositor output.
function SceneComponent:showCompositorOutput() end

--- Get the parent StageComponent.
---@return StageComponent
function SceneComponent:getParentStage() end

--- Get the number of output compositors for this scene.
---@return integer
function SceneComponent:getOutputCompositorCount() end

--- Get the output compositor at the given index.
---@param index integer Zero-based index
---@return CompositorComponent
function SceneComponent:getOutputCompositorAtIndex(index) end

------------------------------------------------------------------------
-- StageComponent : TransformComponent
------------------------------------------------------------------------

---@class StageComponent : TransformComponent
---@field stageId integer Unique stage ID (read-only)
---@field trackingVolumeId integer Tracking volume ID (read/write)
---@field stageBoundsMin Vec3f Minimum bounds of the stage in mm (read-only)
---@field stageBoundsMax Vec3f Maximum bounds of the stage in mm (read-only)
local StageComponent = {}

------------------------------------------------------------------------
-- CameraComponent : TransformComponent
------------------------------------------------------------------------

---@class CameraComponent : TransformComponent
---@field ownerStageId integer ID of the owning stage (read-only)
---@field trackingMountId integer ID of the tracking mount (read-only)
---@field videoSourceId integer ID of the video source (read-only)
---@field trackingFrameDelay integer Frame delay for tracking (read/write)
---@field hasValidApertureOffset boolean Whether aperture offset is valid (read-only)
---@field aperturePositionOffset Vec3f Aperture position offset (read-only)
---@field apertureOrientationOffset Vec3f Aperture orientation offset as Euler angles (read-only)
local CameraComponent = {}

--- Trigger camera alignment.
function CameraComponent:alignCamera() end

--- Get the owning StageComponent.
---@return StageComponent
function CameraComponent:getOwnerStage() end

------------------------------------------------------------------------
-- AnchorComponent : TransformComponent
------------------------------------------------------------------------

---@class AnchorComponent : TransformComponent
local AnchorComponent = {}

--- Open the anchor editor UI.
function AnchorComponent:editAnchor() end

------------------------------------------------------------------------
-- StencilComponent : TransformComponent
------------------------------------------------------------------------

---@class StencilComponent : TransformComponent
---@field isDisabled boolean Whether the stencil is disabled (read/write)
---@field cullMode ScriptContext.CullMode Cull mode for this stencil (read/write)
local StencilComponent = {}

------------------------------------------------------------------------
-- BoxStencilComponent : StencilComponent
------------------------------------------------------------------------

---@class BoxStencilComponent : StencilComponent
---@field boxXSize number Half-extent along X axis in meters (read/write)
---@field boxYSize number Half-extent along Y axis in meters (read/write)
---@field boxZSize number Half-extent along Z axis in meters (read/write)
local BoxStencilComponent = {}

------------------------------------------------------------------------
-- QuadStencilComponent : StencilComponent
------------------------------------------------------------------------

---@class QuadStencilComponent : StencilComponent
---@field quadWidth number Width of the quad stencil in meters (read/write)
---@field quadHeight number Height of the quad stencil in meters (read/write)
---@field isDoubleSided boolean Whether the stencil is double-sided (read/write)
local QuadStencilComponent = {}

------------------------------------------------------------------------
-- ModelStencilComponent : StencilComponent
------------------------------------------------------------------------

---@class ModelStencilComponent : StencilComponent
---@field modelPath string Path to the stencil mesh file (read-only)
local ModelStencilComponent = {}

--- Trigger stencil alignment to the camera.
function ModelStencilComponent:alignStencil() end

------------------------------------------------------------------------
-- ShapeComponent : TransformComponent
------------------------------------------------------------------------

---@class ShapeComponent : TransformComponent
local ShapeComponent = {}

------------------------------------------------------------------------
-- QuadShapeComponent : ShapeComponent
------------------------------------------------------------------------

---@class QuadShapeComponent : ShapeComponent
---@field quadWidth number Width of the quad shape in meters (read/write)
---@field quadHeight number Height of the quad shape in meters (read/write)
---@field isDoubleSided boolean Whether the quad shape is double-sided (read/write)
local QuadShapeComponent = {}

------------------------------------------------------------------------
-- BoxShapeComponent : ShapeComponent
------------------------------------------------------------------------

---@class BoxShapeComponent : ShapeComponent
---@field boxXSize number Size along X axis in meters (read/write)
---@field boxYSize number Size along Y axis in meters (read/write)
---@field boxZSize number Size along Z axis in meters (read/write)
local BoxShapeComponent = {}

------------------------------------------------------------------------
-- ModelShapeComponent : ShapeComponent
------------------------------------------------------------------------

---@class ModelShapeComponent : ShapeComponent
---@field modelPath string Path to the shape mesh file (read-only)
local ModelShapeComponent = {}

------------------------------------------------------------------------
-- DMXFixtureComponent : TransformComponent
------------------------------------------------------------------------

---@class DMXFixtureComponent : TransformComponent
---@field dmxUniverse integer DMX universe number (read/write)
---@field dmxStartChannel integer DMX start channel (read/write)
---@field dmxChannelCount integer Number of DMX channels used (read-only)
---@field isDisabled boolean Whether the fixture is disabled (read/write)
---@field ownerStageId integer ID of the owning stage (read-only)
local DMXFixtureComponent = {}

--- Triangulate the light position using camera data.
function DMXFixtureComponent:triangulateLight() end

--- Get the owning StageComponent.
---@return StageComponent
function DMXFixtureComponent:getOwnerStage() end

------------------------------------------------------------------------
-- RGBSpotLightComponent : DMXFixtureComponent
------------------------------------------------------------------------

---@class RGBSpotLightComponent : DMXFixtureComponent
---@field red integer Red channel value 0-255 (read/write)
---@field green integer Green channel value 0-255 (read/write)
---@field blue integer Blue channel value 0-255 (read/write)
---@field coneAngleDegrees number Cone angle in degrees (read/write)
---@field coneRangeMeters number Cone range in meters (read/write)
local RGBSpotLightComponent = {}

--- Set the RGB color of the spot light.
---@param r integer Red 0-255
---@param g integer Green 0-255
---@param b integer Blue 0-255
function RGBSpotLightComponent:setRGB(r, g, b) end

------------------------------------------------------------------------
-- RGBPixelGridComponent : DMXFixtureComponent
------------------------------------------------------------------------

---@class RGBPixelGridComponent : DMXFixtureComponent
---@field columns integer Number of pixel columns (read/write)
---@field rows integer Number of pixel rows (read/write)
---@field originPixel integer Wiring origin corner: 0 upper left, 1 upper right, 2 lower left, 3 lower right (read/write)
---@field zigZag boolean True when alternating rows are wired backwards (read/write)
local RGBPixelGridComponent = {}

--- Where a grid cell falls in the DMX stream, given the origin and zig-zag
--- layout. Returns -1 when the cell is out of range.
---@param col integer Column index (0-based)
---@param row integer Row index (0-based)
---@return integer
function RGBPixelGridComponent:getPixelWireIndex(col, row) end

--- Set a single pixel color.
---@param col integer Column index (0-based)
---@param row integer Row index (0-based)
---@param r integer Red 0-255
---@param g integer Green 0-255
---@param b integer Blue 0-255
function RGBPixelGridComponent:setPixel(col, row, r, g, b) end

--- Fill all pixels with a solid color.
---@param r integer Red 0-255
---@param g integer Green 0-255
---@param b integer Blue 0-255
function RGBPixelGridComponent:fillPixels(r, g, b) end

------------------------------------------------------------------------
-- DMXFixtureGroupComponent : MikanComponent
------------------------------------------------------------------------

--- A named set of DMX fixtures on one stage. Membership is a set of fixture
--- component ids; a fixture may belong to any number of groups.
---@class DMXFixtureGroupComponent : MikanComponent
---@field stageId integer Owning stage component id (read-only)
local DMXFixtureGroupComponent = {}

--- Number of member fixtures that currently resolve to a live component.
---@return integer
function DMXFixtureGroupComponent:getFixtureCount() end

--- A member fixture by position among the resolved members, as its concrete
--- class, so a pixel grid member answers to RGBPixelGridComponent's own fields.
---@param index integer Zero-based index
---@return DMXFixtureComponent|RGBSpotLightComponent|RGBPixelGridComponent|nil
function DMXFixtureGroupComponent:getFixtureAtIndex(index) end

---@param fixtureId integer
---@return boolean
function DMXFixtureGroupComponent:containsFixture(fixtureId) end

--- Add a fixture by component id. Returns false when already a member.
---@param fixtureId integer
---@return boolean
function DMXFixtureGroupComponent:addFixture(fixtureId) end

--- Remove a fixture by component id. Returns false when not a member.
---@param fixtureId integer
---@return boolean
function DMXFixtureGroupComponent:removeFixture(fixtureId) end

------------------------------------------------------------------------
-- DMXPresetComponent : MikanComponent
------------------------------------------------------------------------

--- A fixed set of DMX channel bytes for the fixtures of one group, captured
--- from the live fixtures or edited in the preset panel.
---@class DMXPresetComponent : MikanComponent
---@field groupId integer The DMXFixtureGroupComponent this preset addresses (read-only)
local DMXPresetComponent = {}

--- Write the preset to every member fixture of its group. A member the
--- preset holds no bytes for lands on zeros.
function DMXPresetComponent:apply() end

--- Snapshot every member fixture's current channel bytes into the preset.
function DMXPresetComponent:capture() end

------------------------------------------------------------------------
-- DMXSequenceComponent : MikanComponent
------------------------------------------------------------------------

--- An animation of one fixture group. A Script sequence is driven by the
--- SequenceUpdate method of the script component it names, which fills the
--- frame buffer itself. The other content sources are rasterized by the
--- editor; a script is optional there and only its SequenceStart and
--- SequenceStop are called, for picking the content with setText or
--- setContentPath.
---@class DMXSequenceComponent : MikanComponent
---@field groupId integer The DMXFixtureGroupComponent this sequence animates (read-only)
---@field scriptComponentId integer The ScriptComponent driving the sequence, -1 for none (read-only)
---@field timeSinceStart number Seconds since play started, wrapped when looping (read-only)
---@field isPlaying boolean
---@field contentSource integer 0 script, 1 scroll bitmap, 2 scroll text, 3 play animation (read-only)
---@field brightness number 0 to 1 dimmer applied to every channel the sequence sends (read-only)
local DMXSequenceComponent = {}

---@return DMXFixtureGroupComponent
function DMXSequenceComponent:getGroup() end

--- Start from zero (calling the script's SequenceStart) or resume from a pause.
function DMXSequenceComponent:play() end

function DMXSequenceComponent:pause() end

--- Stop, calling the handler's stop. The last frame stays on the fixtures.
function DMXSequenceComponent:stop() end

--- Frame buffer: the first three channels of a fixture.
---@param fixtureId integer
---@param r integer
---@param g integer
---@param b integer
function DMXSequenceComponent:setFixtureColor(fixtureId, r, g, b) end

--- Frame buffer: one pixel of a pixel grid member; ignored for other fixtures.
---@param fixtureId integer
---@param col integer Zero-based column
---@param row integer Zero-based row
---@param r integer
---@param g integer
---@param b integer
function DMXSequenceComponent:setPixel(fixtureId, col, row, r, g, b) end

--- Frame buffer: a fixture's raw channel bytes.
---@param fixtureId integer
---@param bytes integer[]
function DMXSequenceComponent:setFixtureChannels(fixtureId, bytes) end

--- Frame buffer: every member of the group to one color (every pixel of a grid).
---@param r integer
---@param g integer
---@param b integer
function DMXSequenceComponent:fillGroup(r, g, b) end

--- Override the text a ScrollText sequence rasterizes, for this run only.
--- Call it from the handler's start; it never writes the component definition.
---@param text string UTF-8
function DMXSequenceComponent:setText(text) end

--- Override the image a ScrollBitmap or PlayAnimation sequence reads, for this
--- run only. Call it from the handler's start.
---@param path string
function DMXSequenceComponent:setContentPath(path) end

--- The text in use: the override when one is set, otherwise the definition's.
---@return string
function DMXSequenceComponent:getText() end

--- The content image in use: the override when one is set, otherwise the
--- definition's.
---@return string
function DMXSequenceComponent:getContentPath() end

------------------------------------------------------------------------
-- MarkerComponent : MikanComponent
------------------------------------------------------------------------

---@class MarkerComponent : MikanComponent
---@field arucoId integer ArUco marker ID (read/write)
---@field lengthMM number Physical marker side length in millimeters (read/write)
local MarkerComponent = {}

--- Print the marker image to a file or display.
function MarkerComponent:printMarker() end

------------------------------------------------------------------------
-- CompositorComponent : MikanComponent
------------------------------------------------------------------------

---@class CompositorComponent : MikanComponent
---@field compositorId integer Unique compositor ID (read-only)
---@field cameraId integer ID of the associated camera (read-only)
---@field ownerSceneId integer ID of the owning scene (read-only)
---@field ownerStageId integer ID of the owning stage (read-only)
---@field isSpoutOutputStreaming boolean Whether Spout output streaming is active (read/write)
local CompositorComponent = {}

--- Open the compositor graph editor UI.
function CompositorComponent:editCompositorGraph() end

--- Get the owning StageComponent.
---@return StageComponent
function CompositorComponent:getOwnerStage() end

------------------------------------------------------------------------
-- QuadShapeSystem — singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class QuadShapeSystem
local QuadShapeSystem = {}

--- Get a QuadShapeComponent by its shape ID.
---@param id integer
---@return QuadShapeComponent
function QuadShapeSystem:getQuadShapeById(id) end

--- Get a QuadShapeComponent by its name.
---@param name string
---@return QuadShapeComponent
function QuadShapeSystem:getQuadShapeByName(name) end

--- Get the total number of quad shapes in the system.
---@return integer
function QuadShapeSystem:getQuadShapeCount() end

--- Get a QuadShapeComponent at a zero-based index.
---@param index integer
---@return QuadShapeComponent
function QuadShapeSystem:getQuadShapeAtIndex(index) end

------------------------------------------------------------------------
-- BoxShapeSystem — singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class BoxShapeSystem
local BoxShapeSystem = {}

--- Get a BoxShapeComponent by its shape ID.
---@param id integer
---@return BoxShapeComponent
function BoxShapeSystem:getBoxShapeById(id) end

--- Get a BoxShapeComponent by its name.
---@param name string
---@return BoxShapeComponent
function BoxShapeSystem:getBoxShapeByName(name) end

--- Get the total number of box shapes in the system.
---@return integer
function BoxShapeSystem:getBoxShapeCount() end

--- Get a BoxShapeComponent at a zero-based index.
---@param index integer
---@return BoxShapeComponent
function BoxShapeSystem:getBoxShapeAtIndex(index) end

------------------------------------------------------------------------
-- ModelShapeSystem — singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class ModelShapeSystem
local ModelShapeSystem = {}

--- Get a ModelShapeComponent by its shape ID.
---@param id integer
---@return ModelShapeComponent
function ModelShapeSystem:getModelShapeById(id) end

--- Get a ModelShapeComponent by its name.
---@param name string
---@return ModelShapeComponent
function ModelShapeSystem:getModelShapeByName(name) end

--- Get the total number of model shapes in the system.
---@return integer
function ModelShapeSystem:getModelShapeCount() end

--- Get a ModelShapeComponent at a zero-based index.
---@param index integer
---@return ModelShapeComponent
function ModelShapeSystem:getModelShapeAtIndex(index) end

------------------------------------------------------------------------
-- Global injected by ProjectScriptContext
------------------------------------------------------------------------

--- The QuadShapeSystem singleton.
---@type QuadShapeSystem
QuadShapeSystem = nil

--- The BoxShapeSystem singleton.
---@type BoxShapeSystem
BoxShapeSystem = nil

--- The ModelShapeSystem singleton.
---@type ModelShapeSystem
ModelShapeSystem = nil
