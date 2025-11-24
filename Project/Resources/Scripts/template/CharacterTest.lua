local Component = require("mono_helper")
return Component {
    fields = {
        name = "CharacterControllerTest",
    },

    Awake = function(self)
        print("[LUA] CharacterControllerTest Awake - Creating Controller")
        
        -- Step 1: Create controller
        self.controller = CharacterController.new()
        
        if self.controller == nil then
            print("[LUA ERROR] Failed to create CharacterController!")
            return
        end
        print("[LUA] CharacterController created successfully!")

        -- Get the components for initialisation
        local collider = self:GetComponent("ColliderComponent")
        local transform = self:GetComponent("Transform")

        -- Initialise CharacterController
        CharacterController.Initialise(self.controller, collider, transform)
    end,  

    Update = function(self, dt)
        if not self.controller then return end
        
        CharacterController.Move(self.controller, 0, 0, 1)
        CharacterController.Update(self.controller, dt)
    end
}