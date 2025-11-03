-- sample_mono_behaviour.lua
-- Comprehensive example that exercises scheduler, event_bus, input_map, FSM, UI, coroutine, and inspector metadata.
-- Requires:
--   mono_helper.lua, engine_bootstrap.lua, event_bus.lua, scheduler.lua, input_map.lua, simple_fsm.lua,
--   ui_helpers.lua, debug_helpers.lua, inspector_meta.lua, engine_bootstrap

-- ensure engine bootstrap utilities exist (scheduler, event_bus, __instances, update)
require("extension.engine_bootstrap")

print("cpp_log type:", type(cpp_log))
if cpp_log then cpp_log("cpp_log test") end

local Component = require("extension.mono_helper")
local IM = require("extension.inspector_meta")
local debug = require("extension.debug_helpers")

return Component {
    -- Public fields (serialized by editor)
    fields = {
        name = "DemoAgent",
        health = 100,
        maxHealth = 100,
        position = { x = 0, y = 0 },
        patrolSpeed = 2.0,
        patrolPoints = { {x=0,y=0}, {x=5,y=0}, {x=5,y=5}, {x=0,y=5} },
        inputJumpAction = "jump" -- action name to map in input_map
    },

    -- Editor metadata to drive inspector widgets
    meta = {
        health = IM.number(0, 999),
        maxHealth = IM.number(1, 999),
        patrolSpeed = IM.number(0, 10, 0.1),
        position = IM.string(), -- editor may show as vector or text
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
        -- quick debug print of the fields table
        debug.pp({
            msg = "Awake called",
            name = self.name,
            health = self.health,
            pos = self.position
        })
    end,

    -------------------------------------------------------
    -- Start: called after Awake, use it to register subscriptions, timers, etc.
    -------------------------------------------------------
    Start = function(self)
        -- 1) Subscribe to a global event (send with event_bus.publish elsewhere)
        self._subToken = self:Subscribe("player_damaged", function(ev)
            local amt = (ev and ev.amount) or 0
            self.health = math.max(0, (self.health or 0) - amt)
            if cpp_log then cpp_log(string.format("[lua] %s took %d damage -> health=%d", tostring(self.name), amt, self.health)) end
        end)

        -- 2) Schedule an after() timer (one-shot) to publish an event that this agent "spawns"
        self:After(1.0, function()
            if cpp_log then cpp_log("[lua] spawn timer fired -> publishing spawn event") end
            if event_bus and event_bus.publish then event_bus.publish("agent_spawned", { name = self.name }) end
        end)

        -- 3) Schedule an every() repeating task to regenerate a little health
        self._everyId = self:Every(2.0, function()
            if (self.health or 0) < (self.maxHealth or 100) then
                self.health = math.min(self.maxHealth, (self.health or 0) + 1)
                if cpp_log then cpp_log(string.format("[lua] %s regenerates -> health=%d", tostring(self.name), self.health)) end
            end
        end)

        -- 4) Start a coroutine for a scripted handshake (demonstrates coroutine & scheduler interplay)
        self._co = coroutine.create(function()
            for i = 1, 3 do
                if cpp_log then cpp_log(string.format("[lua] coroutine step %d (agent=%s)", i, tostring(self.name))) end
                coroutine.yield()
            end
            if cpp_log then cpp_log(string.format("[lua] coroutine finished for %s", tostring(self.name))) end
        end)

        -- 5) Demonstrate using simple_fsm for patrol behaviour
        local FSM = require("simple_fsm")
        local defs = {
            patrol = {
                on_enter = function(s)
                    s.targetIdx = (self._nextIdx or 1)
                    if cpp_log then cpp_log(string.format("[lua] FSM enter patrol idx=%d", s.targetIdx)) end
                end,
                update = function(s, dt)
                    -- simple lerp towards next point
                    local pt = self.patrolPoints[s.targetIdx] or {x=0,y=0}
                    self.position = self.position or { x = 0, y = 0 }
                    local dx = pt.x - self.position.x
                    local dy = pt.y - self.position.y
                    local dist = math.sqrt(dx*dx + dy*dy)
                    if dist < 0.1 then
                        s.targetIdx = (s.targetIdx % #self.patrolPoints) + 1
                    else
                        local speed = self.patrolSpeed or 1
                        self.position.x = self.position.x + (dx / math.max(dist,1e-6)) * speed * dt
                        self.position.y = self.position.y + (dy / math.max(dist,1e-6)) * speed * dt
                    end
                end
            },
            idle = {
                on_enter = function(s) if cpp_log then cpp_log("[lua] FSM idle") end end,
                update = function(s, dt) end
            }
        }
        self._fsm = FSM.new(defs, "patrol")

        -- 6) Demo safe require of a helper module (safe_require)
        local safe = self:Require("safe_require")
        if safe and safe.reload then
            -- reload debug_helpers if present (just a no-op example)
            safe.reload("debug_helpers")
        end
    end,

    -------------------------------------------------------
    -- Update: called each frame (engine_bootstrap global update will call UpdateSafe on instances)
    -- This method demonstrates:
    --   - driving the coroutine (resume)
    --   - FSM ticking
    --   - scheduler is ticked by engine_bootstrap before this
    --   - input_map checks
    --   - UI binding (health progress)
    -------------------------------------------------------
    Update = function(self, dt)
        -- tick FSM if exists
        if self._fsm then self._fsm:update(dt) end

        -- resume coroutine once per frame if it's not dead
        if self._co and coroutine.status(self._co) ~= "dead" then
            local ok, err = coroutine.resume(self._co)
            if not ok and cpp_log then cpp_log("[lua] coroutine resume error: "..tostring(err)) end
        end

        -- Example input action handling (engine must call input_map.set_key_state externally)
        local input = require("input_map")
        if input and input.was_just_pressed and input.was_just_pressed(self.inputJumpAction) then
            if cpp_log then cpp_log("[lua] jump action pressed for "..tostring(self.name)) end
            -- on jump, we publish an event
            if event_bus and event_bus.publish then event_bus.publish("player_jump", { name = self.name }) end
        end

        -- Example UI: show health progress
        local UI = require("ui_helpers")
        if UI and UI.show_progress then
            local frac = 0
            if self.maxHealth and self.maxHealth > 0 then frac = (self.health or 0) / self.maxHealth end
            UI.show_progress("demo_agent_health", frac)
        end

        -- quick debug print occasionally (every 5 seconds using private time)
        self._private = self._private or {}
        self._private._acc = (self._private._acc or 0) + dt
        if self._private._acc >= 5.0 then
            self._private._acc = 0
            -- pretty print a summary
            debug.pp({ name = self.name, pos = self.position, health = self.health })
        end
    end,

    -------------------------------------------------------
    -- OnDisable / Destroy / OnDestroy: cleanup subscriptions/timers
    -------------------------------------------------------
    OnDisable = function(self)
        if cpp_log then cpp_log("[lua] OnDisable called for "..tostring(self.name)) end
        -- nothing else here; Destroy handles actual cleanup
    end,

    OnDestroy = function(self)
        if cpp_log then cpp_log("[lua] OnDestroy called for "..tostring(self.name)) end
    end,

    Destroy = function(self)
        -- intentionally call parent Destroy to run default cleanup in mono_helper_v2,
        -- but also ensure scheduler tasks are cancelled if we tracked IDs.
        if self._everyId and scheduler and scheduler.cancel then
            scheduler.cancel(self._everyId)
            self._everyId = nil
        end
        -- call the default Destroy (mono_helper_v2 provides it)
        -- since we returned the Component wrapper, this resolves to the method defined there.
        -- Here we call OnDisable and unregister subscriptions via the helper's Destroy implementation.
        -- To be explicit: call the helper-provided Destroy (it will call OnDisable and OnDestroy)
        rawget(self, "Destroy") -- placeholder to ensure method set; the wrapper will actually remove references when engine calls DestroyInstance
        -- Note: if wrapper's Destroy will be called by C++ flow, avoid double-calling; ensure your C++ calls instance:Destroy then DestroyInstance(inst).
    end,

    -------------------------------------------------------
    -- on_reload: called when script is reloaded (hot-reload)
    -------------------------------------------------------
    on_reload = function(self)
        -- the engine/hot-reload flow should call _G.on_hot_reload() or instance:OnHotReload()
        if cpp_log then cpp_log("[lua] on_reload invoked for "..tostring(self.name)) end
        -- attempt to preserve runtime fields if possible (mono_helper_v2::SerializeFields is helpful for editor)
        -- e.g. re-subscribe or restart coroutine
        -- re-create coroutine example on reload
        self._co = coroutine.create(function()
            if cpp_log then cpp_log("[lua] reload coroutine start for "..tostring(self.name)) end
            coroutine.yield()
            if cpp_log then cpp_log("[lua] reload coroutine resumed for "..tostring(self.name)) end
        end)
    end
}
