-- Resources/Scripts/GamePlay/AttackHitbox.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    --fields = { },

    Start = function(self)
        print("[AttackHitbox] Testing overlap detection")
        print("[AttackHitbox] Entity ID: " .. tostring(self.id))
        print("[AttackHitbox] Entity ID type: " .. type(self.entityId))
    end,

    Update = function(self)
        --[[
        local cacheId = Physics.GetOverlappingEntities(Engine.GetEntityByName("AttackHitbox"))
        local count = Physics.GetOverlapCount(cacheId)
        
        if count > 0 then
            print("[AttackHitbox] HIT " .. count .. " entities:")
            for i = 0, count - 1 do
                local entityId = Physics.GetOverlapAt(cacheId, i)
                print("  Entity " .. entityId)
            end
        end
        
        Physics.ClearOverlapCache(cacheId)  -- Clean up when done
        ]]
    end,
}