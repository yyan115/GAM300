require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {

    },

    Awake = function(self)

    end,

    Start = function(self)

    end,

    Update = function(self, dt)
        -- [CRITICAL ADDITION] Ensure the scheduler actually runs!
        -- If your engine doesn't call _G.update(dt) automatically, the feathers will never spawn.
        if _G.scheduler and _G.scheduler.tick then
             _G.scheduler.tick(dt)
        end
    end,

    OnDisable = function(self)

    end,
}