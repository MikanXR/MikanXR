---@meta
-- Mikan scripting API — object system classes and globals.
-- This file is a LuaLS definition file; it contains no executable code.
-- Keep in sync with: src/Editor/ECS/ object system bindLuaFunctions implementations.

------------------------------------------------------------------------
-- CameraObjectSystem
------------------------------------------------------------------------

---@class CameraObjectSystem
local CameraObjectSystem = {}

---@param id integer
---@return CameraComponent
function CameraObjectSystem:getCameraById(id) end

---@param name string
---@return CameraComponent
function CameraObjectSystem:getCameraByName(name) end

---@return integer
function CameraObjectSystem:getCameraCount() end

---@param index integer Zero-based index
---@return CameraComponent
function CameraObjectSystem:getCameraAtIndex(index) end

------------------------------------------------------------------------
-- SceneObjectSystem
------------------------------------------------------------------------

---@class SceneObjectSystem
local SceneObjectSystem = {}

--- Get the currently active scene.
---@return SceneComponent
function SceneObjectSystem:getCurrentScene() end

---@param id integer
---@return SceneComponent
function SceneObjectSystem:getSceneById(id) end

---@param name string
---@return SceneComponent
function SceneObjectSystem:getSceneByName(name) end

---@return integer
function SceneObjectSystem:getSceneCount() end

---@param index integer Zero-based index
---@return SceneComponent
function SceneObjectSystem:getSceneAtIndex(index) end

--- Make a scene the active one, deactivating whichever scene is current.
--- An unknown id is a no-op.
---@param id integer
function SceneObjectSystem:setCurrentSceneById(id) end

--- Make a scene the active one. A nil scene is a no-op.
---@param scene SceneComponent
function SceneObjectSystem:setCurrentScene(scene) end

------------------------------------------------------------------------
-- StageObjectSystem
------------------------------------------------------------------------

---@class StageObjectSystem
local StageObjectSystem = {}

---@param id integer
---@return StageComponent
function StageObjectSystem:getStageById(id) end

---@param name string
---@return StageComponent
function StageObjectSystem:getStageByName(name) end

---@return integer
function StageObjectSystem:getFirstStageId() end

--- Get the first stage in the project.
---@return StageComponent
function StageObjectSystem:getFirstStage() end

---@return integer
function StageObjectSystem:getStageCount() end

---@param index integer Zero-based index
---@return StageComponent
function StageObjectSystem:getStageAtIndex(index) end

------------------------------------------------------------------------
-- DMXObjectSystem
------------------------------------------------------------------------

---@class DMXObjectSystem
---@field universeChannelCount integer Number of DMX channels per universe (read-only)
local DMXObjectSystem = {}

--- Blackout: zero every fixture (spot lights and pixel grids, visuals included)
--- and every active DMX universe.
function DMXObjectSystem:zeroAllChannels() end

---@return integer
function DMXObjectSystem:getSpotLightCount() end

---@param index integer Zero-based index
---@return RGBSpotLightComponent
function DMXObjectSystem:getSpotLightAtIndex(index) end

---@return integer
function DMXObjectSystem:getPixelGridCount() end

---@param index integer Zero-based index
---@return RGBPixelGridComponent
function DMXObjectSystem:getPixelGridAtIndex(index) end

------------------------------------------------------------------------
-- AnchorObjectSystem
------------------------------------------------------------------------

---@class AnchorObjectSystem
local AnchorObjectSystem = {}

---@param id integer
---@return AnchorComponent
function AnchorObjectSystem:getAnchorById(id) end

---@param name string
---@return AnchorComponent
function AnchorObjectSystem:getAnchorByName(name) end

---@return integer
function AnchorObjectSystem:getAnchorCount() end

---@param index integer Zero-based index
---@return AnchorComponent
function AnchorObjectSystem:getAnchorAtIndex(index) end

------------------------------------------------------------------------
-- CompositorObjectSystem
------------------------------------------------------------------------

---@class CompositorObjectSystem
local CompositorObjectSystem = {}

---@param id integer
---@return CompositorComponent
function CompositorObjectSystem:getCompositorById(id) end

---@param name string
---@return CompositorComponent
function CompositorObjectSystem:getCompositorByName(name) end

---@return integer
function CompositorObjectSystem:getCompositorCount() end

---@param index integer Zero-based index
---@return CompositorComponent
function CompositorObjectSystem:getCompositorAtIndex(index) end

--- Get the number of compositors belonging to a stage.
---@param stageId integer
---@return integer
function CompositorObjectSystem:getCompositorCountForStage(stageId) end

--- Get the compositor for a stage at the given index.
---@param stageId integer
---@param index integer Zero-based index
---@return CompositorComponent
function CompositorObjectSystem:getCompositorForStageAtIndex(stageId, index) end

------------------------------------------------------------------------
-- Globals injected by ProjectScriptContext
------------------------------------------------------------------------

--- The CameraObjectSystem singleton.
---@type CameraObjectSystem
CameraSystem = nil

--- The SceneObjectSystem singleton.
---@type SceneObjectSystem
SceneSystem = nil

--- The StageObjectSystem singleton.
---@type StageObjectSystem
StageSystem = nil

--- The AnchorObjectSystem singleton.
---@type AnchorObjectSystem
AnchorSystem = nil

--- The CompositorObjectSystem singleton.
---@type CompositorObjectSystem
CompositorSystem = nil

--- The DMXObjectSystem singleton.
---@type DMXObjectSystem
DMXSystem = nil

------------------------------------------------------------------------
-- ModelStencilSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class ModelStencilSystem
ModelStencilSystem = {}

---@param id integer
---@return ModelStencilComponent
function ModelStencilSystem:getModelStencilById(id) end

---@param name string
---@return ModelStencilComponent
function ModelStencilSystem:getModelStencilByName(name) end

---@return integer
function ModelStencilSystem:getModelStencilCount() end

---@param index integer Zero-based index
---@return ModelStencilComponent
function ModelStencilSystem:getModelStencilAtIndex(index) end

------------------------------------------------------------------------
-- BoxStencilSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class BoxStencilSystem
BoxStencilSystem = {}

---@param id integer
---@return BoxStencilComponent
function BoxStencilSystem:getBoxStencilById(id) end

---@param name string
---@return BoxStencilComponent
function BoxStencilSystem:getBoxStencilByName(name) end

---@return integer
function BoxStencilSystem:getBoxStencilCount() end

---@param index integer Zero-based index
---@return BoxStencilComponent
function BoxStencilSystem:getBoxStencilAtIndex(index) end

------------------------------------------------------------------------
-- QuadStencilSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class QuadStencilSystem
QuadStencilSystem = {}

---@param id integer
---@return QuadStencilComponent
function QuadStencilSystem:getQuadStencilById(id) end

---@param name string
---@return QuadStencilComponent
function QuadStencilSystem:getQuadStencilByName(name) end

---@return integer
function QuadStencilSystem:getQuadStencilCount() end

---@param index integer Zero-based index
---@return QuadStencilComponent
function QuadStencilSystem:getQuadStencilAtIndex(index) end

------------------------------------------------------------------------
-- RGBSpotLightSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class RGBSpotLightSystem
RGBSpotLightSystem = {}

---@param id integer
---@return RGBSpotLightComponent
function RGBSpotLightSystem:getLightById(id) end

---@param name string
---@return RGBSpotLightComponent
function RGBSpotLightSystem:getLightByName(name) end

---@return integer
function RGBSpotLightSystem:getLightCount() end

---@param index integer Zero-based index
---@return RGBSpotLightComponent
function RGBSpotLightSystem:getLightAtIndex(index) end

--- Create a new RGB spot light attached to a stage.
---@param stageId integer
---@param name string Component name, or "" to auto-generate one
---@return RGBSpotLightComponent
function RGBSpotLightSystem:createLight(stageId, name) end

--- Remove an RGB spot light.
---@param lightId integer
---@return boolean
function RGBSpotLightSystem:removeLight(lightId) end

------------------------------------------------------------------------
-- DMXFixtureGroupSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class DMXFixtureGroupSystem
DMXFixtureGroupSystem = {}

---@param id integer
---@return DMXFixtureGroupComponent
function DMXFixtureGroupSystem:getGroupById(id) end

---@param name string
---@return DMXFixtureGroupComponent
function DMXFixtureGroupSystem:getGroupByName(name) end

---@return integer
function DMXFixtureGroupSystem:getGroupCount() end

---@param index integer Zero-based index
---@return DMXFixtureGroupComponent
function DMXFixtureGroupSystem:getGroupAtIndex(index) end

--- Create an empty fixture group on a stage.
---@param stageId integer
---@param name string Component name, or "" to auto-generate one
---@return DMXFixtureGroupComponent
function DMXFixtureGroupSystem:createGroup(stageId, name) end

--- Remove a fixture group and every preset that addresses it.
---@param groupId integer
---@return boolean
function DMXFixtureGroupSystem:removeGroup(groupId) end

------------------------------------------------------------------------
-- DMXPresetSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class DMXPresetSystem
DMXPresetSystem = {}

---@param id integer
---@return DMXPresetComponent
function DMXPresetSystem:getPresetById(id) end

---@param name string
---@return DMXPresetComponent
function DMXPresetSystem:getPresetByName(name) end

---@return integer
function DMXPresetSystem:getPresetCount() end

---@param index integer Zero-based index
---@return DMXPresetComponent
function DMXPresetSystem:getPresetAtIndex(index) end

--- Create an empty preset for a fixture group.
---@param groupId integer
---@param name string Component name, or "" to auto-generate one
---@return DMXPresetComponent
function DMXPresetSystem:createPreset(groupId, name) end

--- Remove a preset.
---@param presetId integer
---@return boolean
function DMXPresetSystem:removePreset(presetId) end

------------------------------------------------------------------------
-- DMXSequenceSystem — global singleton injected by ProjectScriptContext
------------------------------------------------------------------------

---@class DMXSequenceSystem
DMXSequenceSystem = {}

---@param id integer
---@return DMXSequenceComponent
function DMXSequenceSystem:getSequenceById(id) end

---@param name string
---@return DMXSequenceComponent
function DMXSequenceSystem:getSequenceByName(name) end

---@return integer
function DMXSequenceSystem:getSequenceCount() end

---@param index integer Zero-based index
---@return DMXSequenceComponent
function DMXSequenceSystem:getSequenceAtIndex(index) end

--- Create a stopped sequence for a fixture group.
---@param groupId integer
---@param name string Component name, or "" to auto-generate one
---@return DMXSequenceComponent
function DMXSequenceSystem:createSequence(groupId, name) end

--- Remove a sequence.
---@param sequenceId integer
---@return boolean
function DMXSequenceSystem:removeSequence(sequenceId) end

------------------------------------------------------------------------
-- SpoutOutputSystem
------------------------------------------------------------------------

---@class SpoutOutputSystem
SpoutOutputSystem = {}

--- Get a Spout output by ID.
---@param id integer
---@return SpoutOutputComponent
function SpoutOutputSystem:getOutputById(id) end

--- Get a Spout output by component name.
---@param name string
---@return SpoutOutputComponent
function SpoutOutputSystem:getOutputByName(name) end

--- Get the number of Spout outputs.
---@return integer
function SpoutOutputSystem:getOutputCount() end

--- Get the Spout output at the given index.
---@param index integer Zero-based index
---@return SpoutOutputComponent
function SpoutOutputSystem:getOutputAtIndex(index) end

------------------------------------------------------------------------
-- RTPOutputSystem
------------------------------------------------------------------------

---@class RTPOutputSystem
RTPOutputSystem = {}

--- Get an RTP output by ID.
---@param id integer
---@return RTPOutputComponent
function RTPOutputSystem:getOutputById(id) end

--- Get an RTP output by component name.
---@param name string
---@return RTPOutputComponent
function RTPOutputSystem:getOutputByName(name) end

--- Get the number of RTP outputs.
---@return integer
function RTPOutputSystem:getOutputCount() end

--- Get the RTP output at the given index.
---@param index integer Zero-based index
---@return RTPOutputComponent
function RTPOutputSystem:getOutputAtIndex(index) end
