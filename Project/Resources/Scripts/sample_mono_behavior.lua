-- sample_mono_behaviour.lua
-- MonoBehaviour-style script that returns an instance table.
-- Designed for the C++ glue in this project:
--  - CreateInstanceFromFile expects the chunk to return a table (the instance).
--  - The runtime calls a global update(dt) each frame; this script registers itself so that
--    per-instance Update(dt) is invoked automatically.

local M = {
    -- public fields (visible to serializer/editor)
    position = { x = 1.0, y = 2.0, z = 3.0 },
    health = 42,
    name = "PlayerOne",
    enabled = true,
    inventory = { "sword", "potion" },

    -- simulated asset reference (will be serialized via custom serializer)
    asset = { type = "AssetRef", data = { id = "asset_hero_mesh" } },

    -- editor metadata (kept as a field with leading __ is optional)
    __editor = {
        health = { displayName = "Health", tooltip = "Hit points (0..100)", editorHint = "slider:0,100" },
        position = { displayName = "Position", tooltip = "World position" },
        name = { displayName = "Character Name", tooltip = "Name shown on HUD" },
        inventory = { displayName = "Inventory", tooltip = "Items held by character" },
        asset = { displayName = "Mesh Asset", tooltip = "Linked mesh asset (ID)", editorHint = "asset" }
    },

    -- internal private state (leading underscore)
    _co = nil
}

local function log(fmt, ...)
    if cpp_log then
        cpp_log(string.format(fmt, ...))
    else
        print(string.format(fmt, ...))
    end
end

-- Instance methods (use colon so 'self' is passed automatically by CallInstanceFunction)
function M:Awake()
    log("[lua] Awake: name=%s health=%d", self.name, self.health)
end

function M:Start()
    log("[lua] Start: position=(%.2f,%.2f,%.2f)", self.position.x, self.position.y, self.position.z)
    if not self._co then
        self._co = coroutine.create(function()
            for i = 1, 3 do
                log("[lua] coroutine step %d for %s", i, self.name)
                coroutine.yield()
            end
            log("[lua] coroutine finished for %s", self.name)
        end)
    end
end

function M:Update(dt)
    if self.enabled then
        -- example mutation that will show up in serializer
        self.position.x = self.position.x + (dt * 0.1)
    end

    -- advance coroutine if alive
    if self._co and coroutine.status(self._co) ~= "dead" then
        local ok, err = coroutine.resume(self._co)
        if not ok then
            log("[lua] coroutine error for %s: %s", tostring(self.name), tostring(err))
        end
    end

    log("[lua] Update: name=%s dt=%.3f pos.x=%.3f health=%d", self.name, dt, self.position.x, self.health)
end

function M:OnDisable()
    log("[lua] OnDisable called for %s", self.name)
end

function M:on_reload()
    log("[lua] on_reload invoked — script reloaded; current name=%s", self.name)
end

-- Remove self from global instance list (call before C++ calls DestroyInstance)
function M:Destroy()
    if _G.__instances then
        for i = #_G.__instances, 1, -1 do
            if _G.__instances[i] == self then
                table.remove(_G.__instances, i)
                break
            end
        end
    end
end

-- Register instance into global list so a global update() can drive per-instance updates.
if not _G.__instances then
    _G.__instances = {}
end
table.insert(_G.__instances, M)

-- Ensure a global update(dt) exists that iterates registered instances.
if not _G.update then
    function _G.update(dt)
        if not _G.__instances then return end
        for _, inst in ipairs(_G.__instances) do
            if inst and inst.Update then
                -- call as method (inst:Update(dt))
                local ok, err = pcall(function() inst:Update(dt) end)
                if not ok then
                    if cpp_log then cpp_log("Error in instance Update: " .. tostring(err)) end
                end
            end
        end
    end
end

-- Return the instance table
return M
