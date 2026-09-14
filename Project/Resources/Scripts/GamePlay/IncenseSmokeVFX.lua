local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Helper: Create Quaternion from Euler
local function eulerToQuat(pitch, yaw, roll)
    local p = math.rad(pitch or 0) * 0.5
    local y = math.rad(yaw or 0) * 0.5
    local r = math.rad(roll or 0) * 0.5
    
    local sinP, cosP = math.sin(p), math.cos(p)
    local sinY, cosY = math.sin(y), math.cos(y)
    local sinR, cosR = math.sin(r), math.cos(r)
    
    return {
        w = cosP * cosY * cosR + sinP * sinY * sinR,
        x = sinP * cosY * cosR - cosP * sinY * sinR,
        y = cosP * sinY * cosR + sinP * cosY * sinR,
        z = cosP * cosY * sinR - sinP * sinY * cosR
    }
end

return Component {
    mixins = { TransformMixin },

    fields = {
        RotationSpeed = 360, -- Degrees per second
    },

    Start = function(self)
        local spiralEntityId = Engine.GetEntityByName("Spiral")

        if not spiralEntityId then
            print("[SpiralRotate] ERROR: Spiral entity not found!")
            return
        end

        self._transform = self:GetComponent("Transform")
        self.currentAngle = 0
    end,

    Update = function(self, dt)
        if not self._transform then return end

        -- Increase angle based on time
        self.currentAngle = self.currentAngle + (self.RotationSpeed * dt)

        -- Keep angle from growing infinitely
        if self.currentAngle > 360 then
            self.currentAngle = self.currentAngle - 360
        end

        -- Convert rotation to quaternion
        local rotQuat = eulerToQuat(0, self.currentAngle, 0)

        -- Apply rotation
        self:SetRotation(rotQuat.w, rotQuat.x, rotQuat.y, rotQuat.z)
    end
}