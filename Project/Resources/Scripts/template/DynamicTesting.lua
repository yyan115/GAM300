require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
-- local TransformMixin = require("extension.transform_mixin")

return Component {
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

        -- if Input.GetKey(Input.Key.W) then 
        --     RigidBodyComponent.AddForce(self._rb,0.0,1000.0,0.0)
        -- end

        --FORCE TEST

        RigidBodyComponent.AddForce(self._rb,0.0,1000.0,0.0)

        --TORQUE TEST
        -- RigidBodyComponent.AddTorque(self._rb, 0.0, 500.0, 600.0)
        --


        --IMPULSE TEST
        -- RigidBodyComponent.AddImpulse(self._rb, 0.0, 500.0, 200.0)
    end
}