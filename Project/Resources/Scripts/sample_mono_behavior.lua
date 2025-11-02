-- mono_behaviour.lua
-- A simple "MonoBehaviour"-style script to exercise serializer / inspector / hot-reload.
-- Public fields (no leading underscore) are intended to be visible in the editor.

-- public fields
position = { x = 1.0, y = 2.0, z = 3.0 }
health = 42
name = "PlayerOne"
enabled = true
inventory = { "sword", "potion" }

-- simulated asset reference that will appear as an object with type/data in JSON
asset = { type = "AssetRef", data = { id = "asset_hero_mesh" } }

-- editor-specific metadata: display names, tooltips and editor hints (e.g. slider) WIP
__editor = {
    health = { displayName = "Health", tooltip = "Hit points (0..100)", editorHint = "slider:0,100" },
    position = { displayName = "Position", tooltip = "World position" },
    name = { displayName = "Character Name", tooltip = "Name shown on HUD" },
    inventory = { displayName = "Inventory", tooltip = "Items held by character" },
    asset = { displayName = "Mesh Asset", tooltip = "Linked mesh asset (ID)", editorHint = "asset" }
}

-- coroutine example (internal)
_co = nil

-- calls the engine logging function (provided by the runtime)
local function log(fmt, ...)
    if cpp_log then
        cpp_log(string.format(fmt, ...))
    else
        print(string.format(fmt, ...))
    end
end

function Awake()
    -- called once on instance creation
    log("[lua] Awake: name=%s health=%d", name, health)
end

function Start()
    log("[lua] Start: position=(%.2f,%.2f,%.2f)", position.x, position.y, position.z)
    if not _co then
        _co = coroutine.create(function()
            for i=1,3 do
                log("[lua] coroutine step %d", i)
                coroutine.yield()
            end
            log("[lua] coroutine finished")
        end)
    end
end

function Update(dt)
    -- demonstrate being able to mutate fields and have them serialized
    if enabled then
        health = math.max(0, health - 0) -- no change unless editor modifies
        -- increment position.x slightly so serializer shows changes across frames
        position.x = position.x + (dt * 0.1)
    end

    -- advance coroutine if alive
    if _co and coroutine.status(_co) ~= "dead" then
        local ok, err = coroutine.resume(_co)
        if not ok then
            log("[lua] coroutine error: %s", tostring(err))
        end
    end

    log("[lua] Update: dt=%.3f pos.x=%.3f health=%d", dt, position.x, health)
end

function OnDisable()
    log("[lua] OnDisable called")
end

function on_reload()
    -- called by hot-reload manager (convention)
    log("[lua] on_reload invoked — script reloaded; current name=%s", name)
end
