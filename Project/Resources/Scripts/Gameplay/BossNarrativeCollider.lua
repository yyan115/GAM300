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
        NarrativeDuration = 25.0,
        PanDuration = 5.0,
    },

    -------------------------------------------------
    -- Start
    -------------------------------------------------

    Start = function(self)
        self._narrativeDuration = 0.0
        self._narrativeDurationOver = true
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
                end
                self._narrativeDurationOver = true
            end
        end
    end,

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

    OnTriggerEnter = function(self, otherEntityId)
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")
        local rootname = Engine.GetEntityName(rootId)

        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            -- Disable attacks and fire the cinematic immediately on enter
            if event_bus and event_bus.publish then
                event_bus.publish("set_attacks_enabled", false)
                event_bus.publish("cinematic.trigger", true)
                event_bus.publish("cinematic.stayDuration", self.NarrativeDuration - self.PanDuration + 1.0)
                event_bus.publish("cinematic.transitionDuration", self.PanDuration)
                self._narrativeDurationOver = false
            end
        end
    end
}