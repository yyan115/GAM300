-- sample_mono_behaviour_NEW.lua
-- Demonstrative component that shows scheduler, event_bus, input_map, FSM, UI, coroutine, and inspector metadata.
-- Improved: more visible behaviour for teaching/demo purposes.

require("extension.engine_bootstrap")

local Component = require("extension.mono_helper")
local IM = require("extension.inspector_meta")
local debug = require("extension.debug_helpers")

-- seed random for demo damage
math.randomseed(os.time())

return Component {
    -- Public fields (serialized by editor)
    fields = {
        name = "DemoAgent",
        health = 60,          -- start < max so UI progress isn't always 1.00
        maxHealth = 100,
        position = { x = 0, y = 0 },
        patrolSpeed = 3.5,    -- faster so movement is obvious
        patrolPoints = { {x=0,y=0}, {x=10,y=0}, {x=10,y=6}, {x=0,y=6} }, -- larger square
        inputJumpAction = "jump" -- action name to map in input_map
    },

    -- Editor metadata to drive inspector widgets
    meta = {
        health = IM.number(0, 999),
        maxHealth = IM.number(1, 999),
        patrolSpeed = IM.number(0, 20, 0.1),
        position = IM.string(),
        inputJumpAction = IM.string()
    },

    -- Internal private state (not serialized)
    _fsm = nil,
    _co = nil,
    _tickId = nil,
    _everyId = nil,
    _subToken = nil,

    -------------------------------------------------------
    -- Awake: called immediately on creation
    -------------------------------------------------------
    Awake = function(self)
        debug.pp({
            msg = "Awake called (demo)",
            name = self.name,
            health = self.health,
            pos = self.position
        })
        -- make sure position table exists
        self.position = self.position or { x = 0, y = 0 }
        -- for UI ids use unique name to avoid collision when multiple instances exist
        self._uiHealthId = "demo_agent_health_" .. tostring(self.name or "agent")
        self._uiPosId = "demo_agent_pos_" .. tostring(self.name or "agent")
        -- friendly startup message
        if cpp_log then cpp_log(string.format("[lua] %s Awake (health=%.1f / max=%.1f)", tostring(self.name), self.health or 0, self.maxHealth or 0)) end
    end,

    -------------------------------------------------------
    -- Start: called after Awake, use it to register subscriptions, timers, etc.
    -------------------------------------------------------
    Start = function(self)
        -- subscribe to direct damage events for this demo
        self._subToken = self:Subscribe("player_damaged", function(ev)
            local amt = (ev and ev.amount) or 0
            self.health = math.max(0, (self.health or 0) - amt)
            if cpp_log then cpp_log(string.format("[lua] %s took %d damage -> health=%d", tostring(self.name), amt, self.health)) end
        end)

        -- also subscribe to global player_jump events to demonstrate cross-listening
        self._jumpListener = self:Subscribe("player_jump", function(ev)
            if cpp_log then cpp_log(string.format("[lua] %s observed player_jump by %s", tostring(self.name), tostring(ev and ev.name))) end
        end)

        -- spawn timer: publish "agent_spawned" (demo)
        self:After(1.0, function()
            if cpp_log then cpp_log(string.format("[lua] %s spawn timer fired -> publishing agent_spawned", tostring(self.name))) end
            if event_bus and event_bus.publish then event_bus.publish("agent_spawned", { name = self.name }) end
        end)

        -- repeating regen every 2s (keeps health moving toward max)
        self._everyId = self:Every(2.0, function()
            if (self.health or 0) < (self.maxHealth or 100) then
                self.health = math.min(self.maxHealth, (self.health or 0) + 1)
                if cpp_log then cpp_log(string.format("[lua] %s regenerates -> health=%d", tostring(self.name), self.health)) end
            end
        end)

        -- demo periodic random damage so the bar visibly fluctuates
        self._damageId = self:Every(5.0, function()
            local dmg = math.random(3, 12)
            if cpp_log then cpp_log(string.format("[lua] demo: applying %d dmg to %s", dmg, tostring(self.name))) end
            if event_bus and event_bus.publish then event_bus.publish("player_damaged", { amount = dmg }) end
        end)

        -- Start a coroutine handshake that prints 3 timed steps so students can follow
        self._co = coroutine.create(function()
            for i = 1, 3 do
                if cpp_log then cpp_log(string.format("[lua] coroutine step %d for %s", i, tostring(self.name))) end
                coroutine.yield()
            end
            if cpp_log then cpp_log(string.format("[lua] coroutine finished for %s", tostring(self.name))) end
        end)

        -- FSM for patrol behaviour (with verbose logging)
        local FSM = require("extension.simple_fsm")
        local defs = {
            patrol = {
                on_enter = function(s)
                    s.targetIdx = (self._nextIdx or 1)
                    if cpp_log then cpp_log(string.format("[lua] FSM enter patrol idx=%d for %s", s.targetIdx, tostring(self.name))) end
                end,
                update = function(s, dt)
                    local pt = self.patrolPoints[s.targetIdx] or {x=0,y=0}
                    local pos = self.position
                    local dx = pt.x - pos.x
                    local dy = pt.y - pos.y
                    local dist = math.sqrt(dx*dx + dy*dy)
                    if dist < 0.2 then
                        -- arrived: advance
                        s.targetIdx = (s.targetIdx % #self.patrolPoints) + 1
                        if cpp_log then cpp_log(string.format("[lua] %s reached waypoint %d -> next=%d", tostring(self.name), (s.targetIdx - 1), s.targetIdx)) end
                    else
                        local speed = self.patrolSpeed or 1
                        pos.x = pos.x + (dx / math.max(dist,1e-6)) * speed * dt
                        pos.y = pos.y + (dy / math.max(dist,1e-6)) * speed * dt
                    end
                end
            },
            idle = {
                on_enter = function(s) if cpp_log then cpp_log("[lua] FSM idle for "..tostring(self.name)) end end,
                update = function(s, dt) end
            }
        }
        self._fsm = FSM.new(defs, "patrol")

        -- safe require demo helper
        local safe = self:Require("extension.safe_require")
        if safe and safe.reload then
            safe.reload("extension.debug_helpers")
        end

        -- small friendly log
        if cpp_log then cpp_log(string.format("[lua] %s Start() complete - health=%.1f pos=(%.2f,%.2f)", tostring(self.name), self.health or 0, self.position.x, self.position.y)) end
    end,

    -------------------------------------------------------
    -- Update: called each frame (engine_bootstrap global update will call UpdateSafe on instances)
    -- Shows: coroutine driving, FSM tick, input handling, UI binding, periodic debug
    -------------------------------------------------------
    Update = function(self, dt)
        -- tick FSM
        if self._fsm then self._fsm:update(dt) end

        -- resume coroutine once per frame while it's alive (this prints 3 steps across frames)
        if self._co and coroutine.status(self._co) ~= "dead" then
            local ok, err = coroutine.resume(self._co)
            if not ok and cpp_log then cpp_log("[lua] coroutine resume error: "..tostring(err)) end
        end

        -- input handling: if jump pressed, publish and do a quick visual "leap"
        local input = require("extension.input_map")
        if input and input.was_just_pressed and input.was_just_pressed(self.inputJumpAction) then
            if cpp_log then cpp_log("[lua] jump action pressed for "..tostring(self.name)) end
            -- small leap: bump y and schedule return
            self.position.y = (self.position.y or 0) + 1.5
            self:After(0.25, function()
                -- land back down
                self.position.y = math.max(0, (self.position.y or 0) - 1.5)
            end)
            if event_bus and event_bus.publish then event_bus.publish("player_jump", { name = self.name }) end
            -- also cause small self-damage to show event-driven changes
            if event_bus and event_bus.publish then event_bus.publish("player_damaged", { amount = 2 }) end
        end

        -- Update UI: show health progress and position (unique ids per instance)
        local UI = require("extension.ui_helpers")
        if UI and UI.show_progress then
            local frac = 0
            if self.maxHealth and self.maxHealth > 0 then frac = (self.health or 0) / self.maxHealth end
            UI.show_progress(self._uiHealthId, frac)
        end
        if UI and UI.show_progress and UI.show_text then
            -- some UIs provide show_text; show position as text if available
            local posStr = string.format("pos=(%.2f,%.2f) hp=%d/%d", (self.position.x or 0), (self.position.y or 0), (self.health or 0), (self.maxHealth or 0))
            if UI.show_text then UI.show_text(self._uiPosId, posStr) end
        else
            -- fallback: print a concise debug line occasionally
            self._private = self._private or {}
            self._private._print_acc = (self._private._print_acc or 0) + dt
            if self._private._print_acc >= 1.0 then
                self._private._print_acc = 0
                if cpp_log then cpp_log(string.format("[lua] %s %s hp=%d/%d", tostring(self.name), string.format("pos(%.2f,%.2f)", (self.position.x or 0),(self.position.y or 0)), (self.health or 0),(self.maxHealth or 0))) end
            end
        end
    end,

    -------------------------------------------------------
    -- OnDisable / Destroy / OnDestroy: cleanup subscriptions/timers
    -------------------------------------------------------
    OnDisable = function(self)
        if cpp_log then cpp_log("[lua] OnDisable called for "..tostring(self.name)) end
    end,

    OnDestroy = function(self)
        if cpp_log then cpp_log("[lua] OnDestroy called for "..tostring(self.name)) end
        -- cleanup: cancel timers if scheduler supports cancel and we tracked ids
        if self._everyId and scheduler and scheduler.cancel then
            scheduler.cancel(self._everyId)
            self._everyId = nil
        end
        if self._damageId and scheduler and scheduler.cancel then
            scheduler.cancel(self._damageId)
            self._damageId = nil
        end
        -- unsubscribe tokens removed by helper's Destroy, but ensure event listeners cleared
    end,

    Destroy = function(self)
        -- super-clean: let mono_helper handle OnDisable/OnDestroy and unsubscribes.
        -- This placeholder ensures the wrapper's Destroy exists for engine calls.
        rawget(self, "Destroy")
    end,

    -------------------------------------------------------
    -- on_reload: called when script is reloaded (hot-reload)
    -------------------------------------------------------
    on_reload = function(self)
        if cpp_log then cpp_log("[lua] on_reload invoked for "..tostring(self.name)) end
        -- preserve coroutine / re-create small demo coroutine after reload
        self._co = coroutine.create(function()
            if cpp_log then cpp_log("[lua] reload coroutine start for "..tostring(self.name)) end
            coroutine.yield()
            if cpp_log then cpp_log("[lua] reload coroutine resumed for "..tostring(self.name)) end
        end)
    end
}
