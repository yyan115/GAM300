-- input_map.lua
-- Map action names to keys and provide query API.
local Input = {}
Input._maps = {}     -- action -> set of keys
Input._keyState = {} -- key -> bool
Input._prevState = {}

function Input.map(action, key)
    if not Input._maps[action] then Input._maps[action] = {} end
    Input._maps[action][key] = true
end

function Input.unmap(action, key)
    if Input._maps[action] then Input._maps[action][key] = nil end
end

-- engine should call this each frame or when key events happen
function Input.set_key_state(key, down)
    Input._prevState[key] = Input._keyState[key]
    Input._keyState[key] = not not down
end

function Input.is_pressed(action)
    local map = Input._maps[action]
    if not map then return false end
    for key, _ in pairs(map) do
        if Input._keyState[key] then return true end
    end
    return false
end

function Input.was_just_pressed(action)
    local map = Input._maps[action]
    if not map then return false end
    for key, _ in pairs(map) do
        if Input._keyState[key] and not Input._prevState[key] then return true end
    end
    return false
end

return Input
