-- simple_mover.lua
-- Moves an entity in a direction at a given speed
-- This is what a non-programmer would write!

require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
-- local TransformMixin = require("extension.transform_mixin")

return Component {
    -- -- Apply the transform mixin for easy movement
    -- mixins = { TransformMixin },
    
    -- Editable fields (visible in inspector)
    fields = {
        velocityX = -3.0,
        velocityY = 0.0,
        velocityZ = 0.0
    },
    
    -- Called once when the component is created
    Start = function(self)
        self._rb = self:GetComponent("RigidBodyComponent")
    end,
    
    -- Called every frame
    Update = function(self, dt)
        if not self._rb then 
            print("rigidbody cant be found")
            return
        end

        if Input.GetKey(Input.Key.W) then 
            print("W is pressed")
        end

        self._rb.linearVel.x = -3


    end
}