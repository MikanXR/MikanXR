-- A chase: one lit fixture runs along the members of a DMXFixtureGroup.
-- Assign this file to a script component, then pick that component as the
-- script of a DMXSequenceComponent and press Play. SequenceUpdate writes into
-- the sequence's frame buffer; the sequence system pushes that buffer to the
-- fixtures after every update.

local Chase = ScriptBehavior:extend("Chase")

function Chase:init()
	self.chase_speed = 2.0 -- fixtures per second
end

function Chase:SequenceStart(sequence)
	sequence:fillGroup(0, 0, 0)
end

function Chase:SequenceUpdate(sequence, timeSinceStart, deltaSeconds)
	local group = sequence:getGroup()
	if group == nil then
		return
	end

	local count = group:getFixtureCount()
	if count == 0 then
		return
	end

	local head = math.floor(timeSinceStart * self.chase_speed) % count
	sequence:fillGroup(0, 0, 0)
	sequence:setFixtureColor(group:getFixtureAtIndex(head).componentId, 255, 40, 0)
end

-- Optional: Stop leaves the last frame unless the method writes another
function Chase:SequenceStop(sequence)
	sequence:fillGroup(0, 0, 0)
end

return Chase
