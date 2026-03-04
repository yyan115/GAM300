require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {
        AliveDuration = 3.0,
    },

    Awake = function(self)

    end,

    Start = function(self)
        self._aliveDuration = self.AliveDuration
    end,

    Update = function(self, dt)
        self._aliveDuration = self._aliveDuration - dt
        if self._aliveDuration <= 0 then
            Engine.DestroyEntity(self.entityId)
        end
    end,

    OnDisable = function(self)

    end,
}