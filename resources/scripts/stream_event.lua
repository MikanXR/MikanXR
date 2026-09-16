-- A stream event that takes the scene over for a few seconds and gives it back.
--
-- An external tool (Streamer.bot, a Stream Deck, anything that can fetch a URL)
-- calls a route with the event's details in the query string:
--
--   http://127.0.0.1:8090/trigger/gift_sub?user=bob&tier=3
--
-- Routes are bound in the HTTP Triggers panel: add a route named gift_sub,
-- pick the script component this file is assigned to, and pick the GiftSub
-- function. The method receives the query pairs as a table. Values are always
-- strings, so anything numeric needs tonumber(). The table is always present
-- and is empty when the caller sent no query string, as the panel button does.

local StreamEvent = ScriptBehavior:extend("StreamEvent")

function StreamEvent:init()
	self.effect_scene_name = "SubCelebration"
	self.effect_seconds = 6.0
	self.effect_sequence = "SEQ_Rainbow"

	-- The scene to return to, and the guard that keeps a second event arriving
	-- mid-effect from caching the effect scene as the one to restore
	self._cached_scene_id = nil
end

function StreamEvent:HttpTrigger_GiftSub(args)
	if self._cached_scene_id ~= nil then
		-- Already running. Anything but returning here restores the wrong scene.
		return
	end

	local current_scene = SceneSystem:getCurrentScene()
	local effect_scene = SceneSystem:getSceneByName(self.effect_scene_name)
	if current_scene == nil or effect_scene == nil then
		return
	end

	self._cached_scene_id = current_scene.componentId

	local user = args.user or "someone"
	local tier = tonumber(args.tier) or 1

	SceneSystem:setCurrentScene(effect_scene)

	local sequence = DMXSequenceSystem:getSequenceByName(self.effect_sequence)
	if sequence ~= nil then
		sequence:play()
	end

	-- Reaches connected clients (the Unreal plugin, a browser overlay) as a
	-- MikanScriptMessagePostedEvent, so they can play their own effects
	ScriptContext.broadcastMessage(string.format("gift_sub:%s:%d", user, tier))

	start_coroutine(function()
		wait_seconds(self.effect_seconds)

		SceneSystem:setCurrentSceneById(self._cached_scene_id)
		self._cached_scene_id = nil
	end)
end

-- The same event from the script panel button, with no query args
function StreamEvent:Trigger_GiftSub(args)
	self:HttpTrigger_GiftSub(args)
end

return StreamEvent
