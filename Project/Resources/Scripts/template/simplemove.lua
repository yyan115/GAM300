-- simple_mover.lua
-- Moves an entity in a direction at a given speed
-- This is what a non-programmer would write!

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {
    -- Apply the transform mixin for easy movement
    mixins = { TransformMixin },
    
    -- Editable fields (visible in inspector)
    fields = {
        speed = 2.0,
        directionX = 3.0,
        directionY = 0.0,
        directionZ = 0.0
    },
    
    -- Called once when the component is created
    Start = function(self)
        print("SimpleMover started with speed: " .. tostring(self.speed))
    end,
    
    -- Called every frame
    Update = function(self, dt)
        -- Calculate movement delta
        local dx = self.directionX * self.speed * dt
        local dy = self.directionY * self.speed * dt
        local dz = self.directionZ * self.speed * dt
        
        -- Move the entity (handles all the complexity internally)
        self:Move(dx, dy, dz)
    end
}