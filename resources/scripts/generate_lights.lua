-- Lays out RGB spot lights on a stage in a zig-zag grid with sequential DMX
-- addressing. Every parameter is a field init assigns: edit it in the script
-- panel, then press the GenerateLights trigger. Each run replaces the lights
-- this script generated on the stage and leaves hand-placed lights alone.
--
-- Positions are in the stage's local frame in meters. Angles are Euler
-- degrees. An integer default makes an INT parameter, so a float parameter
-- must be written with a decimal point.

local GenerateLights = ScriptBehavior:extend("GenerateLights")

local LIGHT_PREFIX = "LIGHT_gen_"
local CHANNELS_PER_LIGHT = 3

function GenerateLights:init()
	self.num_rows = 2
	self.lights_per_row = 4
	self.origin_offset_vec3 = Vec3f(-1.5, 3.0, -2.0)
	self.row_step_vec3 = Vec3f(1.0, 0.0, 0.0)
	self.row_offset_vec3 = Vec3f(0.0, 0.0, 1.5)
	self.light_angles_vec3 = Vec3f(-90.0, 0.0, 0.0)
	self.cone_angle = 30.0
	self.cone_range = 4.0
	self.start_universe_id = 1
	self.start_channel_index = 1
	-- The stage to generate on; none falls back to the first stage
	self.parent_stage = ComponentRef("StageComponent")
end

function GenerateLights:_resolveStage()
	local stage = self.parent_stage
	if stage == nil then
		stage = StageSystem:getFirstStage()
	end
	return stage
end

-- Collect first, remove after: removing while walking the light list is unsafe
function GenerateLights:_removeGeneratedLights(stageId)
	local ids = {}
	for i = 0, RGBSpotLightSystem:getLightCount() - 1 do
		local light = RGBSpotLightSystem:getLightAtIndex(i)
		if light.ownerStageId == stageId and string.sub(light.name, 1, #LIGHT_PREFIX) == LIGHT_PREFIX then
			table.insert(ids, light.componentId)
		end
	end
	for _, id in ipairs(ids) do
		RGBSpotLightSystem:removeLight(id)
	end
end

function GenerateLights:Trigger_GenerateLights(args)
	local stage = self:_resolveStage()
	if stage == nil then
		print("GenerateLights: no stage to generate on")
		return
	end
	local stageId = stage.stageId

	self:_removeGeneratedLights(stageId)

	local universe = self.start_universe_id
	local channel = self.start_channel_index
	local maxChannel = DMXSystem.universeChannelCount
	local index = 0

	for r = 0, self.num_rows - 1 do
		-- Odd rows run backwards from the far end, so the channel order follows
		-- the physical path
		local base = self.origin_offset_vec3 + self.row_offset_vec3:scaleUniform(r)
		local step = self.row_step_vec3
		if r % 2 == 1 then
			base = base + self.row_step_vec3:scaleUniform(self.lights_per_row - 1)
			step = self.row_step_vec3:scaleUniform(-1)
		end

		for c = 0, self.lights_per_row - 1 do
			if channel + CHANNELS_PER_LIGHT - 1 > maxChannel then
				universe = universe + 1
				channel = 1
			end

			local light = RGBSpotLightSystem:createLight(stageId, LIGHT_PREFIX .. index)
			light.relativePosition = base + step:scaleUniform(c)
			light.relativeRotation = self.light_angles_vec3
			light.coneAngleDegrees = self.cone_angle
			light.coneRangeMeters = self.cone_range
			light.dmxUniverse = universe
			light.dmxStartChannel = channel

			channel = channel + CHANNELS_PER_LIGHT
			index = index + 1
		end
	end
end

return GenerateLights
