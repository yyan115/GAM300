-- Resources/Scripts/GamePlay/AttackHitbox.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    fields = {},

    Start = function(self)
        print("[AttackHitbox] Testing overlap detection")
        print("[AttackHitbox] Entity ID: " .. tostring(self.id))
        print("[AttackHitbox] Entity ID type: " .. type(self.entityId))
    end,

    Update = function(self, dt)
        -- Try converting entityId to a number
        --[[]
        local id = tonumber(self.entityId)
        if not id then
            print("[AttackHitbox] ERROR: Cannot convert entityId to number")
            return
        end
        ]]
        local overlaps = Physics.GetOverlappingEntities(Engine.GetEntityByName("AttackHitbox"))
        
        for i,id in ipairs(overlaps) do
            print(i, id)
        end
    end,
}