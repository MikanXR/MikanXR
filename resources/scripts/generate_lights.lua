-- Lays out RGB spot lights on a stage in a zig-zag grid with sequential DMX
-- addressing. Every parameter is a script variable: edit it in the script
-- panel, then press the generate_lights trigger. Each run replaces the lights
-- this script generated on the stage and leaves hand-placed lights alone.
--
-- Positions are in the stage's local frame in meters. Angles are Euler
-- degrees. Integer defaults register INT variables, so a float parameter
-- must be written with a decimal point.

ScriptContext.registerVariable("num_rows", 2)
ScriptContext.registerVariable("lights_per_row", 4)
ScriptContext.registerVariable("origin_offset_vec3", Vec3f(-1.5, 3.0, -2.0))
ScriptContext.registerVariable("row_step_vec3", Vec3f(1.0, 0.0, 0.0))
ScriptContext.registerVariable("row_offset_vec3", Vec3f(0.0, 0.0, 1.5))
ScriptContext.registerVariable("light_angles_vec3", Vec3f(-90.0, 0.0, 0.0))
ScriptContext.registerVariable("cone_angle", 30.0)
ScriptContext.registerVariable("cone_range", 4.0)
ScriptContext.registerVariable("start_universe_id", 1)
ScriptContext.registerVariable("start_channel_index", 1)
ScriptContext.registerVariable("parent_stage", -1)

local LIGHT_PREFIX = "LIGHT_gen_"
local CHANNELS_PER_LIGHT = 3

local function resolve_stage()
	local stage = StageSystem:getStageById(parent_stage)
	if stage == nil then
		stage = StageSystem:getFirstStage()
	end
	return stage
end

-- Collect first, remove after: removing while walking the light list is unsafe
local function remove_generated_lights(stageId)
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

function generate_lights()
	local stage = resolve_stage()
	if stage == nil then
		print("generate_lights: no stage to generate on")
		return
	end
	local stageId = stage.stageId

	remove_generated_lights(stageId)

	local universe = start_universe_id
	local channel = start_channel_index
	local maxChannel = DMXSystem.universeChannelCount
	local index = 0

	for r = 0, num_rows - 1 do
		-- Odd rows run backwards from the far end, so the channel order follows
		-- the physical path
		local base = origin_offset_vec3 + row_offset_vec3:scaleUniform(r)
		local step = row_step_vec3
		if r % 2 == 1 then
			base = base + row_step_vec3:scaleUniform(lights_per_row - 1)
			step = row_step_vec3:scaleUniform(-1)
		end

		for c = 0, lights_per_row - 1 do
			if channel + CHANNELS_PER_LIGHT - 1 > maxChannel then
				universe = universe + 1
				channel = 1
			end

			local light = RGBSpotLightSystem:createLight(stageId, LIGHT_PREFIX .. index)
			light.relativePosition = base + step:scaleUniform(c)
			light.relativeRotation = light_angles_vec3
			light.coneAngleDegrees = cone_angle
			light.coneRangeMeters = cone_range
			light.dmxUniverse = universe
			light.dmxStartChannel = channel

			channel = channel + CHANNELS_PER_LIGHT
			index = index + 1
		end
	end
end
ScriptContext.registerTrigger("generate_lights")
