-- event_bus.lua
-- Simple pub/sub event bus.

local Bus = {}
Bus._listeners = {}
Bus._nextToken = 1

function Bus.subscribe(event, fn)
    if type(event) ~= "string" then return nil end
    Bus._listeners[event] = Bus._listeners[event] or {}
    local token = { id = Bus._nextToken, event = event }
    Bus._nextToken = Bus._nextToken + 1
    Bus._listeners[event][token.id] = fn
    return token
end

function Bus.unsubscribe(token)
    if not token then return end
    local t = Bus._listeners[token.event]
    if t then t[token.id] = nil end
end

function Bus.publish(event, payload)
    local t = Bus._listeners[event]
    if not t then return end
    for _, fn in pairs(t) do
        local ok, err = pcall(fn, payload)
        if not ok then
            if cpp_log then cpp_log("event_bus publish error: "..tostring(err)) else print("event_bus publish error", err) end
        end
    end
end

return Bus
