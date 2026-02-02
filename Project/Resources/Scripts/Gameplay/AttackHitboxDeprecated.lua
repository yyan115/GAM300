-- Resources/Scripts/GamePlay/AttackHitbox.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },

    --fields = { },

    Start = function(self)
    end,

    Update = function(self)
        local cacheId = Engine.GetEntitiesByTag("Enemy", 4)
        local count = Engine.GetTagCacheCount(cacheId)
        print(count)

        for i = 0, count - 1 do
            local enemyId = Engine.GetTagCacheAt(cacheId, i)
            
            if Physics.CheckDistance(self.entityId, enemyId, 1.0) then
                print("Hit enemy " .. enemyId)
            end
        end

        Engine.ClearTagCache(cacheId)
    end,
}