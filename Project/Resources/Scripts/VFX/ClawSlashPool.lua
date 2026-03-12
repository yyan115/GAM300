require("extension.engine_bootstrap")

local SlashPool = {
    slashes = {},
    index = 1,
}

function SlashPool.Register(slashComponent)
    table.insert(SlashPool.slashes, slashComponent)
end

function SlashPool.Request()
    local n = #SlashPool.slashes
    if n == 0 then return nil end

    for i = 1, n do
        local s = SlashPool.slashes[SlashPool.index]
        SlashPool.index = SlashPool.index + 1
        if SlashPool.index > n then SlashPool.index = 1 end

        -- Only return if it's not currently visible/active
        if s and (not s.active) then
            return s
        end
    end
    return nil
end

return SlashPool