-- AndroidOnly.lua
-- Attach this script to any entity that should only be visible on Android.
-- Useful for touch controls (joystick, attack button, jump button, etc.)

local M = {}

function M:Start()
    local isAndroid = false
    if Platform and Platform.IsAndroid then
        isAndroid = Platform.IsAndroid()
    end

    if not isAndroid then
        -- Hide this entity on desktop
        local activeComp = self:GetComponent("ActiveComponent")
        if activeComp then
            activeComp.isActive = false
        end
    end
end

return M
