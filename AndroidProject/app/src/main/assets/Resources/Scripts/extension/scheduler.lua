-- scheduler.lua
-- Tiny scheduler: after(seconds, fn), every(interval, fn), cancel(id), tick(dt)

local S = {}
S._timers = {}
S._nextId = 1

function S.after(seconds, fn)
    local id = S._nextId; S._nextId = S._nextId + 1
    S._timers[id] = { remaining = tonumber(seconds) or 0, fn = fn, every = nil }
    return id
end

function S.every(interval, fn)
    local id = S._nextId; S._nextId = S._nextId + 1
    S._timers[id] = { remaining = tonumber(interval) or 0, fn = fn, every = tonumber(interval) or 0 }
    return id
end

function S.cancel(id) S._timers[id] = nil end

function S.tick(dt)
    local toRemove = {}
    for id, t in pairs(S._timers) do
        t.remaining = t.remaining - dt
        while t.remaining <= 0 do
            local ok, err = pcall(t.fn)
            if not ok then
                if cpp_log then cpp_log("scheduler tick error: "..tostring(err)) else print("scheduler tick error", err) end
            end
            if t.every and t.every > 0 then
                t.remaining = t.remaining + t.every
                break
            else
                table.insert(toRemove, id)
                break
            end
        end
    end
    for _, id in ipairs(toRemove) do S._timers[id] = nil end
end

return S
