-- Resources/Scripts/Gameplay/KnifeBootstrap.lua
require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")

return Component {
    fields = {
        PoolSize = 30,
        TemplateName = "Knife",     -- name of the template entity in scene / prefab
        InstancePrefix = "Knife",   -- Knife1..KnifeN
        EnableLogs = false,
    },

    Start = function(self)
        local n = tonumber(self.PoolSize) or 0
        if n <= 0 then return end

        -- Duplicate template into Knife1..KnifeN (engine semantics assumed same as ChainBootstrap)
        Engine.CreateEntityDup(self.TemplateName, self.InstancePrefix, n)

        print("[KnifeBootstrap] dup done, checking Knife1 script…")
        local tr = Engine.FindTransformByName("Knife1")
        print("[KnifeBootstrap] Knife1 transform:", tostring(tr))

        if self.EnableLogs then
            print(string.format("[KnifeBootstrap] Spawned pool: %s x%d", self.TemplateName, n))
        end

        -- Optional: verify transforms exist (purely for debugging)
        if self.EnableLogs then
            local found = 0
            for i = 1, n do
                local tr = Engine.FindTransformByName(self.InstancePrefix .. tostring(i))
                if tr then found = found + 1 end
            end
            print(string.format("[KnifeBootstrap] Found transforms: %d/%d", found, n))
        end
    end,
}
