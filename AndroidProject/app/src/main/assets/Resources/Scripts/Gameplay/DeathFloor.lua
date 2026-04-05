require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")
local event_bus = _G.event_bus

return Component {
    mixins = { TransformMixin },

    fields = {

    },

    Awake = function(self)

    end,

    Start = function(self)

    end,

    Update = function(self, dt)

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
        --print("[DeathFloor] OnTriggerEnter")
        
        local rootId = self:_toRoot(otherEntityId)
        local tagComp = GetComponent(rootId, "TagComponent")

        if tagComp and Tag.Compare(tagComp.tagIndex, "Player") then
            event_bus.publish("triggerPlayerDeath", true)
        end
    end,

    OnDisable = function(self)

    end,
}
