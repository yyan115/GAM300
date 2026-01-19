-- JoystickVisual.lua
-- Attach to the OUTER joystick circle entity (e.g., "(ANDROID)Joystick")
-- Set innerJoystickName to the inner circle's entity name
--
-- This script:
-- 1. Hides the joystick on desktop
-- 2. On Android: moves inner circle to where player touches on outer joystick
-- 3. Calculates movement direction from touch position relative to center

local M = {
    -- Inspector-editable properties
    innerJoystickName = "(ANDROID)JoystickInner",
    joystickRadius = 50.0,  -- Radius in game units for direction calculation

    -- Private variables
    _innerTransform = nil,
    _baseX = 0,
    _baseY = 0,
    _baseZ = 0,
    _isAndroid = false,
    _moveX = 0,
    _moveY = 0
}

function M:Start()
    -- Check platform
    if Platform and Platform.IsAndroid then
        self._isAndroid = Platform.IsAndroid()
    else
        self._isAndroid = false
    end

    if not self._isAndroid then
        -- Hide on desktop
        local activeComp = self:GetComponent("ActiveComponent")
        if activeComp then
            activeComp.isActive = false
        end
        return
    end

    -- Find inner joystick transform
    self._innerTransform = Engine.FindTransformByName(self.innerJoystickName)
    if self._innerTransform then
        local pos = self._innerTransform.localPosition
        if pos then
            self._baseX = tonumber(pos.x) or 0
            self._baseY = tonumber(pos.y) or 0
            self._baseZ = tonumber(pos.z) or 0
        end
    end
end

function M:Update(dt)
    if not self._isAndroid then return end
    if not self._innerTransform then return end

    -- Get movement axis (returns -1 to 1 on Android when joystick is touched)
    local axis = Input.GetAxis("Movement")
    local axisX = axis and axis.x or 0
    local axisY = axis and axis.y or 0

    -- Check if joystick is being used (any non-zero input)
    local isActive = (axisX ~= 0 or axisY ~= 0) or Input.IsActionPressed("Movement")

    if isActive and Input.IsActionPressed("Movement") then
        -- Convert normalized axis (-1 to 1) to visual offset
        local visualX = axisX * self.joystickRadius
        local visualY = axisY * self.joystickRadius

        -- Move inner joystick visual
        local pos = self._innerTransform.localPosition
        if pos then
            if type(pos) == "userdata" then
                pos.x = self._baseX + visualX
                pos.y = self._baseY + visualY
            else
                self._innerTransform.localPosition = {
                    x = self._baseX + visualX,
                    y = self._baseY + visualY,
                    z = self._baseZ
                }
            end
            self._innerTransform.isDirty = true
        end

        -- Store normalized movement direction
        self._moveX = axisX
        self._moveY = axisY
    else
        -- Joystick released - return inner to center
        local pos = self._innerTransform.localPosition
        if pos then
            if type(pos) == "userdata" then
                pos.x = self._baseX
                pos.y = self._baseY
            else
                self._innerTransform.localPosition = {
                    x = self._baseX,
                    y = self._baseY,
                    z = self._baseZ
                }
            end
            self._innerTransform.isDirty = true
        end

        self._moveX = 0
        self._moveY = 0
    end
end

-- Public function for other scripts to get movement direction
-- Returns x, y values from -1 to 1
function M:GetMovementDirection()
    return self._moveX, self._moveY
end

return M
