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

        --GET THE COMPONENTS FOR INITIALISATION
        local collider = self:GetComponent("ColliderComponent")
        local transform = self:GetComponent("Transform")

        CharacterController.Initialise(self.controller, collider, transform)

        


    end  -- Close the Awake function
    }
