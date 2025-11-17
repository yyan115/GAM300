-- rotate.lua
-- Rotates an object continuously around the Y axis
-- Beginner-friendly example!

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

-- Helper function to create a rotation quaternion from Euler angles (degrees)
local function eulerToQuat(pitch, yaw, roll)
    -- Convert degrees to radians
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
        rotationSpeed = 45.0,  -- degrees per second
        axis = "Y"             -- Which axis to rotate around: X, Y, or Z
    },
    
    Start = function(self)
        self.currentAngle = 0
        print("Rotate behavior started! Speed: " .. tostring(self.rotationSpeed) .. " deg/s around " .. self.axis .. " axis")
    end,
    
    Update = function(self, dt)
        -- Update rotation angle
        self.currentAngle = self.currentAngle + (self.rotationSpeed * dt)
        
        -- Keep angle in 0-360 range
        if self.currentAngle >= 360 then
            self.currentAngle = self.currentAngle - 360
        end
        
        -- Convert to quaternion based on axis
        local quat
        if self.axis == "X" then
            quat = eulerToQuat(self.currentAngle, 0, 0)
        elseif self.axis == "Z" then
            quat = eulerToQuat(0, 0, self.currentAngle)
        else
            -- Default to Y axis
            quat = eulerToQuat(0, self.currentAngle, 0)
        end
        
        -- Apply rotation using the mixin
        self:SetRotation(quat.w, quat.x, quat.y, quat.z)
    end
}