-- Resources/Scripts/Gameplay/KnifePool.lua
require("extension.engine_bootstrap")

local KnifePool = {
    knives = {},
    index = 1,
}

function KnifePool.Register(knifeComponent)
    table.insert(KnifePool.knives, knifeComponent)
end

function KnifePool.Request()
    local n = #KnifePool.knives
    if n == 0 then
        return nil
    end

    -- round-robin
    for i = 1, n do
        local k = KnifePool.knives[KnifePool.index]
        KnifePool.index = KnifePool.index + 1
        if KnifePool.index > n then KnifePool.index = 1 end

        -- IMPORTANT: check reserved too
        if k and (not k.active) and (not k.reserved) then
            return k
        end
    end

    return nil
end

function KnifePool.RequestMany(count)
    count = count or 1
    local got = {}

    for i = 1, count do
        local k = KnifePool.Request()
        if not k then
            -- give back reservations we already took
            for j = 1, #got do
                if got[j] then got[j].reserved = false end
            end
            return nil
        end

        -- reserve immediately so we can't hand it out twice
        k.reserved = true
        table.insert(got, k)
    end

    return got
end

return KnifePool