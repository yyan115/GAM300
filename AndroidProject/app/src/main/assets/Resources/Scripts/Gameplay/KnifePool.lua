-- Resources/Scripts/Gameplay/KnifePool.lua
require("extension.engine_bootstrap")

local KnifePool = {
    knives = {},     -- array of transforms/entities you register
    index = 1,
}

function KnifePool.Register(knifeComponent)
    table.insert(KnifePool.knives, knifeComponent)
end

function KnifePool.Request()
    local n = #KnifePool.knives
    if n == 0 then
        -- print("[KnifePool] No knives registered!")
        return nil
    end

    -- round-robin
    for i = 1, n do
        local k = KnifePool.knives[KnifePool.index]
        KnifePool.index = KnifePool.index + 1
        if KnifePool.index > n then KnifePool.index = 1 end

        if k and not k.active then
            return k
        end
    end

    -- none free
    -- print("[KnifePool] No free knives available")
    return nil
end

return KnifePool
