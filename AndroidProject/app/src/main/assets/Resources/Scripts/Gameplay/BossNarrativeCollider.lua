-- Resources/Scripts/Gameplay/DoorTrigger.lua

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

local Engine    = _G.Engine
local event_bus = _G.event_bus

-------------------------------------------------
-- Component
-------------------------------------------------

return Component {

    mixins = { TransformMixin },

    fields = {
        NarrativeDuration   = 25.0,
        PanDuration         = 5.0,
    },

    -------------------------------------------------
    -- Start
    -------------------------------------------------

    Start = function(self)
        self._narrativeDuration = 0.0
        self._narrativeDurationOver = true
        self._hasTriggered = false
        self._narrativeDone  = false  -- true once the narrative timer has fully elapsed
        self._bossDead       = false  -- true once boss_killed fires

        if not (_G.event_bus and _G.event_bus.subscribe) then return end

        self._bossKilledSub = _G.event_bus.subscribe("boss_killed", function(_)
            self._bossDead = true
        end)

        -- If the player dies during the narrative, stop the in-progress timer so it
        -- doesn't fire boss_narrative_ended while the player is already respawning.
        self._respawnSub = _G.event_bus.subscribe("respawnPlayer", function(respawn)
            if not respawn then return end
            if not self._narrativeDurationOver then
                self._narrativeDurationOver = true
                self._narrativeDuration = 0.0
            end
        end)
    end,

    -------------------------------------------------
    -- Update
    -------------------------------------------------

    Update = function(self, dt)
        if not self._narrativeDurationOver then
            self._narrativeDuration = self._narrativeDuration + dt

            if self._narrativeDuration >= self.NarrativeDuration then
                if event_bus and event_bus.publish then
                    event_bus.publish("freeze_enemy", false)
                    event_bus.publish("set_attacks_enabled", true)
                    event_bus.publish("boss_narrative_ended")
                end
                self._narrativeDurationOver = true
                self._narrativeDone = true
            end
        end
    end,

    -------------------------------------------------
    -- Helper: climb to root entity
    -------------------------------------------------

    _toRoot = function(self, entityId)
        local targetId = entityId
        if Engine and Engine.GetParentEntity then
            while true do
                local parentId = Engine.GetParentEntity(targetId)
                if not parentId or parentId < 0 then break end
                targetId = parentId
            end
        end
        return targetId
    end,

    -------------------------------------------------
    -- Trigger
    -------------------------------------------------

    OnTriggerEnter = function(self, otherEntityId)

        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        if not (tagComp and Tag.Compare(tagComp.tagIndex, "Player")) then return end

        if self._hasTriggered then
            -- Narrative already played. If the boss is still alive and the narrative
            -- fully completed, skip straight to boss BGM (player respawned and re-entered).
            if self._narrativeDone and not self._bossDead then
                if event_bus and event_bus.publish then
                    event_bus.publish("freeze_enemy", false)
                    event_bus.publish("set_attacks_enabled", true)
                    event_bus.publish("boss_narrative_ended")
                end
            end
            return
        end

        -- First entry: play narrative
        if event_bus and event_bus.publish then
            event_bus.publish("set_attacks_enabled", false)
            event_bus.publish("cinematic.targetName", "BossCinematicCamPos")
            event_bus.publish("cinematic.trigger", true)
            event_bus.publish("cinematic.stayDuration", self.NarrativeDuration - self.PanDuration + 1.0)
            event_bus.publish("cinematic.transitionDuration", self.PanDuration)
            event_bus.publish("boss_narrative_started")
        end

        self._narrativeDuration = 0.0
        self._narrativeDurationOver = false
        self._hasTriggered = true
    end,

    OnDisable = function(self)
        if _G.event_bus and _G.event_bus.unsubscribe then
            if self._bossKilledSub then _G.event_bus.unsubscribe(self._bossKilledSub); self._bossKilledSub = nil end
            if self._respawnSub    then _G.event_bus.unsubscribe(self._respawnSub);    self._respawnSub    = nil end
        end
    end,
}