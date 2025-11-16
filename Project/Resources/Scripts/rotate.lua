-- rotate.lua
-- Rotates an object continuously
-- Beginner-friendly example!

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    mixins = { TransformMixin },
    
    fields = {
        rotationSpeed = 45.0  -- degrees per second
    },
    
    Start = function(self)
        self.currentAngle = 0
        print("Rotate behavior started!")
    end,
    
    Update = function(self, dt)
        -- Update rotation angle
        self.currentAngle = self.currentAngle + (self.rotationSpeed * dt)
        
        -- Keep angle in 0-360 range
        if self.currentAngle >= 360 then
            self.currentAngle = self.currentAngle - 360
        end
        
        -- Apply rotation (for now just prints, you'd implement proper quaternion rotation)
        print("Rotating to angle: " .. tostring(self.currentAngle))
    end
}