local Component = require("mono_helper")

return Component {
    fields = {
        name = "CharacterControllerTest",
        physicsSystem = nil,    -- to store the physics system reference
        controller = nil        -- to store the CharacterController instance
    },

    Awake = function(self)
        print("CharacterControllerTest Awake - Getting Physics System")
        -- Get PhysicsSystem from engine 
        self.physicsSystem = PhysicsSystem.GetSystem()
        if self.physicsSystem == nil then
            print("ERROR: PhysicsSystem not available!")
            return
        end
    end,

    Start = function(self)
        print("CharacterControllerTest Start - Creating CharacterController")
        self.controller = CharacterController.new(self.physicsSystem)
        if self.controller == nil then
            print("ERROR: Failed to create CharacterController!")
            return
        end
        print("CharacterController created successfully!")
    end,

    Update = function(self, dt)
        if self.controller == nil then return end

        -- Example movement: move forward continuously
        self.controller:Move(1, 0, 0)
        print("Controller moved along X by speed*dt")

        -- Example jump (trigger once)
        if Input.GetKeyDown(Input.Key.Space) then
            self.controller:Jump(5)  -- jump height = 5
            print("Controller jumped!")
        end

        -- Update controller for this frame
        self.controller:Update(dt)

        -- Print position if GetPosition wrapper exists
        if self.controller.GetPosition then
            local pos = self.controller:GetPosition()
            print("Current position: x=" .. pos.x .. " y=" .. pos.y .. " z=" .. pos.z)
        end
    end,

    OnDisable = function(self)
        print("CharacterControllerTest disabled")
        -- Optionally destroy controller later if Destroy wrapper exists
        -- CharacterController.Destroy(self.controller)
    end
}
