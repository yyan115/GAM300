-- Shared main-menu visibility and platform layout policy.
local M = {}

local BUTTONS = {"PlayGame", "Credits", "ExitGame", "Settings", "Controls"}
local TEXTS = {"PlayGameText", "SettingText", "CreditsText", "ExitGameText", "ControlsText"}

function M.supportsControls()
    return not (Platform and Platform.IsAndroid and Platform.IsAndroid())
end

function M.isEntryAvailable(name)
    return (name ~= "Controls" and name ~= "ControlsText") or M.supportsControls()
end

local function component(name, kind)
    local entity = Engine.GetEntityByName(name)
    return entity and GetComponent(entity, kind) or nil
end

function M.setButtonsInteractable(interactable)
    for _, name in ipairs(BUTTONS) do
        local button = component(name, "ButtonComponent")
        if button then button.interactable = interactable and M.isEntryAvailable(name) end
    end
end

function M.setTextsActive(active)
    for _, name in ipairs(TEXTS) do
        local state = component(name, "ActiveComponent")
        if state then state.isActive = active and M.isEntryAvailable(name) end
    end
end

local function moveByY(transform, offset)
    if not transform or offset == 0 then return end
    local position = transform.localPosition
    if type(position) == "userdata" then
        position.y = position.y + offset
    else
        transform.localPosition = {x = position.x, y = position.y + offset, z = position.z}
    end
    transform.isDirty = true
end

-- Both the controller and hover initialization use this before caching bounds.
-- The remaining Controls-to-Credits gap becomes zero after the first call, so
-- initialization order and repeated calls cannot shift the menu twice.
function M.applyPlatformLayout()
    if M.supportsControls() then return end
    local controls = component("Controls", "Transform")
    local credits = component("Credits", "Transform")
    if controls and credits then
        local offset = controls.localPosition.y - credits.localPosition.y
        moveByY(credits, offset)
        moveByY(component("ExitGame", "Transform"), offset)
    end
    for _, name in ipairs({"Controls", "ControlsText"}) do
        local state = component(name, "ActiveComponent")
        if state then state.isActive = false end
    end
    local button = component("Controls", "ButtonComponent")
    if button then button.interactable = false end
end

return M
