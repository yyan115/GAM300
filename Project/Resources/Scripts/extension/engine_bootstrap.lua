-- engine_bootstrap.lua
-- Minimal bootstrap so scripts can rely on _G.__instances, scheduler, and event_bus.
-- Put this in Resources/Scripts and require it or let your main script require it.

_G.__instances = _G.__instances or {}

-- Ensure scheduler exists (if not provided elsewhere)
_G.scheduler = _G.scheduler or {
    _timers = {},
    _nextId = 1,
    after = function(seconds, fn)
        local id = scheduler._nextId; scheduler._nextId = scheduler._nextId + 1
        scheduler._timers[id] = { remaining = seconds or 0, fn = fn, every = nil }
        return id
    end,
    every = function(interval, fn)
        local id = scheduler._nextId; scheduler._nextId = scheduler._nextId + 1
        scheduler._timers[id] = { remaining = interval or 0, fn = fn, every = interval }
        return id
    end,
    cancel = function(id) scheduler._timers[id] = nil end,
    tick = function(dt)
        local expired = {}
        for id, t in pairs(scheduler._timers) do
            t.remaining = t.remaining - dt
            while t.remaining <= 0 do
                local ok, err = pcall(t.fn)
                if not ok then
                    if cpp_log then cpp_log("scheduler error: "..tostring(err)) else print("scheduler error:", err) end
                end
                if t.every and t.every > 0 then
                    t.remaining = t.remaining + t.every
                    break
                else
                    table.insert(expired, id)
                    break
                end
            end
        end
        for _, id in ipairs(expired) do scheduler._timers[id] = nil end
    end
}

-- Ensure event_bus exists
_G.event_bus = _G.event_bus or (function()
    local bus = { _listeners = {}, _next = 1 }
    function bus.subscribe(event, fn)
        if not bus._listeners[event] then bus._listeners[event] = {} end
        local token = { id = bus._next, event = event }
        bus._next = bus._next + 1
        bus._listeners[event][token.id] = fn
        return token
    end
    function bus.unsubscribe(token)
        if not token then return end
        local t = bus._listeners[token.event]
        if t then t[token.id] = nil end
    end
    function bus.publish(event, payload)
        local t = bus._listeners[event]
        if not t then return end
        for _, fn in pairs(t) do
            local ok, err = pcall(fn, payload)
            if not ok then
                if cpp_log then cpp_log("event_bus error: "..tostring(err)) else print("event_bus error:", err) end
            end
        end
    end
    return bus
end)()

-- Default global update if none provided: iterate __instances and call UpdateSafe(dt)
if not _G.update then
    function _G.update(dt)
        -- Only tick scheduler, don't iterate instances
        if scheduler and type(scheduler.tick) == "function" then
            scheduler.tick(dt)
        end
    end
end

-- Hot-reload helper: call OnHotReload on instances
if not _G.on_hot_reload then
    function _G.on_hot_reload()
        if not _G.__instances then return end
        for _, inst in ipairs(_G.__instances) do
            if inst and type(inst.OnHotReload) == "function" then
                pcall(inst.OnHotReload, inst)
            end
        end
    end
end
