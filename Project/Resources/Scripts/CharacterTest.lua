require("extension.engine_bootstrap")
local Component = require("extension.mono_helper")
local TransformMixin = require("extension.transform_mixin")

return Component {

    mixins = {TransformMixin},

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
    end,

    Start = function(self)
        -- Get the components for initialisation
        local collider = self:GetComponent("ColliderComponent")
        local transform = self:GetComponent("Transform")

        -- Initialise CharacterController
        CharacterController.Initialise(self.controller, collider, transform)
    end,  

    Update = function(self, dt)
        if not self.controller then return end
        
        CharacterController.Move(self.controller, 0, -1, 0)
        --update the internal jolt
        CharacterController.Update(self.controller, dt)

        --get Position from Jolt and pass it to ECS
        local position = CharacterController.GetPosition(self.controller)
        self:SetPosition(position.x, position.y, position.z)
    end,

    OnDisable = function(self)
        print("[LUA] OnDisable called - cleaning up controller")
        if self.controller then
            CharacterController.Destroy(self.controller)
            self.controller = nil
        end
    end
}