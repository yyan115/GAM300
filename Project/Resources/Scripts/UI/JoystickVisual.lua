-- JoystickVisual.lua
-- Attach to the OUTER joystick circle. Set innerJoystickName to the inner circle's entity name.
-- This script:
-- 1. Hides the joystick on desktop
-- 2. Moves the inner circle based on Movement axis input on Android

local M = {
    -- Inspector-editable properties
    innerJoystickName = "(ANDROID)JoystickInner",
    maxOffset = 50.0,

    -- Private variables
    _innerTransform = nil,
    _basePosition = nil,
    _isAndroid = false
}

function M:Start()
    -- Check platform
    if Platform and Platform.IsAndroid then
        self._isAndroid = Platform.IsAndroid()
        print("[JoystickVisual] Platform.IsAndroid() = " .. tostring(self._isAndroid))
    else
        print("[JoystickVisual] ERROR: Platform is nil!")
        self._isAndroid = false
    end

    if not self._isAndroid then
        print("[JoystickVisual] Hiding joystick (not Android)")
        -- Hide on desktop by setting ActiveComponent.isActive = false
        local activeComp = self:GetComponent("ActiveComponent")
        if activeComp then
            activeComp.isActive = false
        end
        return
    end

    print("[JoystickVisual] Running on Android - keeping joystick visible")

    -- Find inner joystick transform
    self._innerTransform = Engine.FindTransformByName(self.innerJoystickName)
    if self._innerTransform then
        -- Store the base position of inner circle (get from localPosition directly)
        local pos = self._innerTransform.localPosition
        if pos then
            -- Extract numeric values
            self._baseX = tonumber(pos.x) or 0
            self._baseY = tonumber(pos.y) or 0
            self._baseZ = tonumber(pos.z) or 0
            print("[JoystickVisual] Found inner joystick at " .. self._baseX .. ", " .. self._baseY)
        else
            print("[JoystickVisual] ERROR: localPosition is nil")
        end
    else
        print("[JoystickVisual] ERROR: Could not find inner joystick: " .. self.innerJoystickName)
    end
end

function M:Update(dt)
    if not self._isAndroid then return end
    if not self._innerTransform or not self._baseX then return end

    -- Get movement axis from input system
    local axis = Input.GetAxis("Movement")
    if not axis then return end

    -- Get axis values (axis is a Vector2D userdata)
    local axisX = tonumber(axis.x) or 0
    local axisY = tonumber(axis.y) or 0

    -- Debug: Log axis values when non-zero
    if axisX ~= 0 or axisY ~= 0 then
        print("[JoystickVisual] Movement axis: x=" .. axisX .. " y=" .. axisY)
    end

    -- Calculate offset (axis values are -1 to 1)
    -- Invert Y for UI coordinates (screen Y increases downward, UI Y may increase upward)
    local offsetX = axisX * self.maxOffset
    local offsetY = -axisY * self.maxOffset

    -- Calculate new position
    local newX = self._baseX + offsetX
    local newY = self._baseY + offsetY

    -- Update inner circle position (handle both userdata and table cases)
    local pos = self._innerTransform.localPosition
    if pos then
        local posType = type(pos)

        -- Debug: Log position update
        if axisX ~= 0 or axisY ~= 0 then
            print("[JoystickVisual] Updating inner pos: (" .. newX .. ", " .. newY .. ") type=" .. posType)
        end

        if posType == "userdata" then
            -- Modify userdata in-place (LuaBridge allows this)
            pos.x = newX
            pos.y = newY
        else
            -- For tables, create new table and reassign
            self._innerTransform.localPosition = {
                x = newX,
                y = newY,
                z = self._baseZ
            }
        end
        -- Mark transform as dirty
        self._innerTransform.isDirty = true
    end
end

return M
